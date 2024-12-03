#include "cJSON.h"
#include "libxml2.h"
#include "utils.h"
#include <dlfcn.h>
#include <minizip/unzip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// Function to print element contents recursively
xmlNode *find_element_by_prop(xmlNode *start_node, const char *name,
                              const char *prop_name, const char *prop_value) {
  if (!start_node || !prop_name || !prop_value)
    return NULL;
  for (xmlNode *cur_node = start_node; cur_node; cur_node = cur_node->next) {
    xmlChar *val = xmlGetProp(cur_node, (const xmlChar *)prop_name);
    int match = (cur_node->type == XML_ELEMENT_NODE && val &&
                 xmlStrcmp(val, (const xmlChar *)prop_value) == 0 &&
                 xmlStrcmp(cur_node->name, (const xmlChar *)name) == 0);

    // Free the retrieved property to prevent memory leak
    // if (val) xmlFree(val);

    if (match) {
      return cur_node;
    }

    xmlNode *found =
        find_element_by_prop(cur_node->children, name, prop_name, prop_value);
    if (found)
      return found;
  }
  return NULL;
}

xmlNode *find_element_by_name(xmlNode *start_node, const char *name) {
  if (!start_node || !name)
    return NULL;
  for (xmlNode *cur_node = start_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE &&
        xmlStrcmp(cur_node->name, (const xmlChar *)name) == 0) {
      return cur_node;
    }

    xmlNode *found = find_element_by_name(cur_node->children, name);
    if (found)
      return found;
  }
  return NULL;
}

int read_file_from_zip(const char *zipfile, const char *filename, char **out) {
  unzFile zfile = NULL;
  char *buffer = NULL;

  // Open the zip file
  zfile = unzOpen(zipfile);
  if (zfile == NULL) {
    fprintf(stderr, "Could not open zip file %s\n", zipfile);
    return -1;
  }

  // Locate the file in the zip archive
  if (unzLocateFile(zfile, filename, 0) != UNZ_OK) {
    fprintf(stderr, "File %s not found in the archive\n", filename);
    unzClose(zfile);
    return -1;
  }

  // Get current file info
  unz_file_info file_info;
  char current_filename[256];
  if (unzGetCurrentFileInfo(zfile, &file_info, current_filename,
                            sizeof(current_filename), NULL, 0, NULL,
                            0) != UNZ_OK) {
    fprintf(stderr, "Could not get file info\n");
    unzClose(zfile);
    return -1;
  }

  // Open the current file
  if (unzOpenCurrentFile(zfile) != UNZ_OK) {
    fprintf(stderr, "Could not open file inside zip\n");
    unzClose(zfile);
    return -1;
  }

  // Buffer for reading
  buffer = malloc(file_info.uncompressed_size + 1); // +1 for null-termination
  if (buffer == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    unzCloseCurrentFile(zfile);
    unzClose(zfile);
    return -1;
  }

  // Read the file
  int bytes_read =
      unzReadCurrentFile(zfile, buffer, file_info.uncompressed_size);
  if (bytes_read != file_info.uncompressed_size) {
    fprintf(stderr, "Error reading file from zip\n");
    free(buffer);
    unzCloseCurrentFile(zfile);
    unzClose(zfile);
    return -1;
  }

  // Null-terminate the buffer for string operations
  buffer[file_info.uncompressed_size] = '\0';

  // Cleanup
  unzCloseCurrentFile(zfile);
  unzClose(zfile);
  *out = buffer;
  return file_info.uncompressed_size;
}

cJSON *generate_change_request_epub(const char *file_path, const char *uuid) {
  FILE *f = fopen("/tmp/epub_meta.log", "a");
  fprintf(f, "Generating change request for %s\n", file_path);

  char *meta_inf = NULL;
  xmlDoc *doc = NULL;
  cJSON *json = NULL;

  int size = read_file_from_zip(file_path, "META-INF/container.xml", &meta_inf);
  if (meta_inf == NULL || size == -1) {
    fprintf(f, "Could not read META-INF/container.xml\n");
    fclose(f);
    return NULL;
  }

  doc = xmlParseDoc((const xmlChar *)meta_inf);
  if (doc == NULL) {
    fprintf(f, "Could not parse metinf XML document\n");
    free(meta_inf);
    fclose(f);
    return NULL;
  }

  xmlNode *root_element = xmlDocGetRootElement(doc);
  xmlNode *rootfile = find_element_by_name(root_element, "rootfile");
  if (rootfile == NULL) {
    fprintf(f, "Could not find rootfile element\n");
    xmlFreeDoc(doc);
    free(meta_inf);
    fclose(f);
    return NULL;
  }

  xmlChar *metadata_path = xmlGetProp(rootfile, (const xmlChar *)"full-path");
  if (metadata_path == NULL) {
    fprintf(f, "Could not get metadata path\n");
    xmlFreeDoc(doc);
    free(meta_inf);
    fclose(f);
    return NULL;
  }

  char *metadata;

  size = read_file_from_zip(file_path, (const char *)metadata_path, &metadata);

  if (metadata == NULL || size == -1) {
    fprintf(f, "Could not read metadata\n");

    xmlFreeDoc(doc);
    free(meta_inf);
    fclose(f);
    return NULL;
  }

  char *metastring = malloc(strlen(metadata) + 1);
  strcpy(metastring, metadata);

  xmlDoc *metadata_doc = NULL;
  metadata_doc =
      xmlParseDoc((const xmlChar *)metastring); // seg fault here for no reason

  if (metadata_doc == NULL) {

    fprintf(f, "Could not parse metadata XML document\n");
    free(metadata);
    xmlFreeDoc(doc);
    free(meta_inf);
    fclose(f);
    return NULL;
  }
  root_element = xmlDocGetRootElement(metadata_doc);

  struct stat st;
  stat(file_path, &st);

  xmlNode *creator_node = find_element_by_name(root_element, "creator");
  xmlChar *creator = creator_node ? xmlNodeGetContent(creator_node) : NULL;
  xmlNode *publisher_node = find_element_by_name(root_element, "publisher");
  xmlChar *publisher =
      publisher_node ? xmlNodeGetContent(publisher_node) : NULL;
  xmlNode *cover_id_node =
      find_element_by_prop(root_element, "meta", "name", "cover");
  char thumbnail_path[40 + 37];
  if (cover_id_node != NULL) {
    char *absolute_cover_path = NULL;
    xmlChar *cover_id = xmlGetProp(cover_id_node, (xmlChar *)"content");
    xmlNode *cover_node = find_element_by_prop(root_element, "item", "id",
                                               (const char *)cover_id);
    xmlChar *cover_path = xmlGetProp(cover_node, (xmlChar *)"href");
    char *s = strrchr((const char *)metadata_path, '/');
    if (s != NULL) {
      *s = '\0';
      absolute_cover_path = malloc(strlen((const char *)metadata_path) +
                                   strlen((const char *)cover_path) + 2);
      sprintf(absolute_cover_path, "%s/%s", (const char *)metadata_path,
              (const char *)cover_path);
    } else {
      absolute_cover_path = strdup((const char *)cover_path);
    }

    snprintf((char *)&thumbnail_path, 40 + 37,
             "/mnt/us/system/thumbnails/thumbnail_%s.jpg", uuid);

    FILE *thumb = fopen((const char *)&thumbnail_path, "w");
    if (thumb == NULL) {
      fprintf(stderr, "Could not open thumbnail file\n");
      // Extensive cleanup
      free(absolute_cover_path);
      free(metadata);
      xmlFreeDoc(metadata_doc);
      xmlFreeDoc(doc);
      free(meta_inf);
      fclose(f);
      cJSON_Delete(json);
      return NULL;
    }

    char *cover_data = NULL;
    size = read_file_from_zip(file_path, absolute_cover_path, &cover_data);
    if (size == -1) {
      fprintf(f, "Could not read cover image\n");
      fclose(thumb);
      free(absolute_cover_path);
      free(metadata);
      xmlFreeDoc(metadata_doc);
      xmlFreeDoc(doc);
      free(meta_inf);
      fclose(f);
      cJSON_Delete(json);
      return NULL;
    }

    fwrite(cover_data, size, 1, thumb);
    fclose(thumb);
    free(cover_data);
    free(absolute_cover_path);
  }

  // Process book title
  xmlNode *book_title_node = find_element_by_name(root_element, "title");
  xmlChar *book_title =
      book_title_node ? xmlNodeGetContent(book_title_node) : NULL;

  json =
      generate_change_request(uuid, file_path, st.st_mtim.tv_sec, st.st_size,
                              creator, publisher, book_title, thumbnail_path);
  // Free book title if it was allocated
  // if (book_title) xmlFree(book_title);

  // Cleanup function for the XML library
  xmlFreeDoc(metadata_doc);
  xmlFreeDoc(doc);
  xmlCleanupParser();

  // Free remaining resources

  free(meta_inf);
  free(metadata);
  fclose(f);

  return json;
}

typedef int(ScannerEventHandler)(const struct scanner_event *event);
typedef void (*ScannerEventHandler2)(ScannerEventHandler **handler, int *unk1);
int main(int argc, char *argv[]) {
  void *handle = dlopen("/mnt/us/libepubExtractor.so", RTLD_NOW);
  if (!handle) {
    fprintf(stderr, "dlopen error: %s\n", dlerror());
    exit(EXIT_FAILURE);
  }
  void *fun = dlsym(handle, "load_epub_extractor");

  printf("hi: %p\n", fun);
  ScannerEventHandler *event_handler;
  int out = -1;
  ((ScannerEventHandler2)fun)(&event_handler, &out);
  printf("hi: %p %d\n", event_handler, out);
  struct scanner_event *event = malloc(sizeof(struct scanner_event));
  event->event_type = 0;
  event->path = "/mnt/us/documents/Stoka";
  event->lipchandle = NULL;
  event->filename = "test.epub";
  event->uuid = "45";
  event->glob = "GL:*.epub";
  event_handler(event);

  // if (argc < 2) {
  //   fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
  //   return EXIT_FAILURE;
  // }

  // const char *file_path = argv[1];

  // cJSON *json = generate_change_request(file_path, "45");
  // if (json == NULL) {
  //   fprintf(stderr, "Failed to generate change request\n");
  //   return EXIT_FAILURE;
  // }

  // char *json_str = cJSON_Print(json);

  // // Free JSON string and JSON object
  // free(json_str);
  // cJSON_Delete(json);

  return EXIT_SUCCESS;
}
