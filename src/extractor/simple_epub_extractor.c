#include <minizip/unzip.h>
#include "cJSON.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "libxml2.h"
// Function to print element contents recursively
xmlNode *find_element_by_prop(xmlNode *start_node, const char *name,
                              const char *prop_name, const char *prop_value) {
  if (!start_node || !prop_name || !prop_value)
    return NULL;
  for (xmlNode *cur_node = start_node; cur_node; cur_node = cur_node->next) {
    xmlChar *val = xmlGetProp(cur_node, (const xmlChar *)prop_name);
    if (cur_node->type == XML_ELEMENT_NODE &&
        xmlStrcmp(val, (const xmlChar *)prop_value) == 0 &&
        xmlStrcmp(cur_node->name, (const xmlChar *)name) == 0) {
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
  // Open the zip file
  unzFile zfile = unzOpen(zipfile);
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
  char *buffer = malloc(file_info.uncompressed_size);

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

  // Cleanup
  unzCloseCurrentFile(zfile);
  unzClose(zfile);
  *out = buffer;
  return file_info.uncompressed_size;
}

cJSON *generate_change_request(char *file_path, char *uuid) {
    FILE* f = fopen("/tmp/epub_meta.log", "a");
    fprintf(f, "Generating change request for %s\n", file_path);
  char *meta_inf = NULL;
  int size = read_file_from_zip(file_path, "META-INF/container.xml", &meta_inf);
  meta_inf[size] = 0x0;

  if (meta_inf == NULL || size == -1) {
    fprintf(f, "Could not read META-INF/container.xml\n");
    fclose(f);
    return NULL;
  }
  xmlDoc *doc = xmlParseDoc((const xmlChar *)meta_inf);
  if (doc == NULL) {
      fprintf(f,"data %s", meta_inf);
    fprintf(f, "Could not parse metinf XML document\n");
    fclose(f);
    return NULL;
  }
  xmlNode *root_element = xmlDocGetRootElement(doc);
  xmlNode *rootfile = find_element_by_name(root_element, "rootfile");
  if (rootfile == NULL) {
    fprintf(f, "Could not find rootfile element\n");
    fclose(f);
    xmlFreeDoc(doc);
    return NULL;
  }
  xmlChar *metadata_path = xmlGetProp(rootfile, (const xmlChar *)"full-path");

  char *metadata;
  size = read_file_from_zip(file_path, (const char *)metadata_path, &metadata);
  metadata[size] = 0x0;
  doc = xmlParseDoc((const xmlChar *)metadata);
  if (doc == NULL) {
    fprintf(f, "Could not parse lol XML document\n");
    fclose(f);
    return NULL;
  }
  root_element = xmlDocGetRootElement(doc);

  cJSON *json = cJSON_CreateObject();
  cJSON_AddItemToObject(json, "uuid", cJSON_CreateString(uuid));
  cJSON_AddItemToObject(json, "location", cJSON_CreateString(file_path));
  cJSON_AddItemToObject(json, "type", cJSON_CreateString("Entry:Item"));
  cJSON_AddItemToObject(json, "cdeType", cJSON_CreateString("PDOC"));

  cJSON_AddItemToObject(json, "cdeKey", cJSON_CreateString(file_path));
  struct stat st;
  stat(file_path, &st);
  cJSON_AddItemToObject(json, "modificationTime",
                        cJSON_CreateNumber(st.st_mtime));
  cJSON_AddItemToObject(json, "diskUsage", cJSON_CreateNumber(st.st_size));
  cJSON_AddItemToObject(json, "isVisibleInHome", cJSON_CreateTrue());
  cJSON_AddItemToObject(json, "isArchived", cJSON_CreateFalse());
  cJSON_AddItemToObject(json, "mimeType",
                        cJSON_CreateString("application/epub+zip"));

  xmlNode *creator_node = find_element_by_name(root_element, "creator");
  xmlChar *creator = xmlNodeGetContent(creator_node);
  cJSON *authors = cJSON_CreateArray();
  cJSON *author = cJSON_CreateObject();
  cJSON_AddItemToObject(author, "kind", cJSON_CreateString("Author"));
  cJSON *author_name = cJSON_CreateObject();
  cJSON_AddItemToObject(author_name, "display",
                        cJSON_CreateString((const char *)creator));
  cJSON_AddItemToObject(author, "name", author_name);
  cJSON_AddItemToArray(authors, author);
  cJSON_AddItemToObject(json, "credits", authors);

  xmlNode *publisher_node = find_element_by_name(root_element, "publisher");
  xmlChar *publisher = xmlNodeGetContent(publisher_node);
  cJSON_AddItemToObject(json, "publisher",
                        cJSON_CreateString((const char *)publisher));

  cJSON *refs = cJSON_CreateArray();
  cJSON *titles_ref = cJSON_CreateObject();
  cJSON_AddItemToObject(titles_ref, "ref", cJSON_CreateString("titles"));
  cJSON_AddItemToArray(refs, titles_ref);
  cJSON *authors_ref = cJSON_CreateObject();
  cJSON_AddItemToObject(authors_ref, "ref", cJSON_CreateString("credits"));
  cJSON_AddItemToArray(refs, authors_ref);
  cJSON_AddItemToObject(json, "displayObjects", refs);

  const char *tags[] = {"NEW"};
  cJSON_AddItemToObject(json, "displayTags", cJSON_CreateStringArray(tags, 1));

  // Get the cover image
  xmlNode *cover_id_node =
      find_element_by_prop(root_element, "meta", "name", "cover");
  if (cover_id_node == NULL) {
    fprintf(f, "Could not find dc:title element\n");
    fclose(f);
    xmlFreeDoc(doc);
    return NULL;
  }
  xmlChar *cover_id = xmlGetProp(cover_id_node, (xmlChar *)"content");
  xmlNode *cover_node =
      find_element_by_prop(root_element, "item", "id", (const char *)cover_id);
  xmlChar *cover_path = xmlGetProp(cover_node, (xmlChar *)"href");
  char *absolute_cover_path;
  char *s = strrchr((const char *)metadata_path, '/');
  if (s != NULL) {
    *s = 0x0;
    absolute_cover_path = malloc(strlen((const char *)metadata_path) +
                                 strlen((const char *)cover_path) + 1);
    sprintf(absolute_cover_path, "%s/%s", (const char *)metadata_path,
            (const char *)cover_path);
  } else {
    absolute_cover_path = (char *)cover_path;
  }
  char thumbnail_path[40 + 37];
  snprintf((char *)&thumbnail_path, 40 + 37,
           "/mnt/us/system/thumbnails/thumbnail_%s.jpg", uuid);

  FILE *thumb = fopen((const char *)&thumbnail_path, "w");
  if (thumb == NULL) {
    fprintf(stderr, "Could not open thumbnail file\n");
    xmlFreeDoc(doc);
    return NULL;
  }
  char *cover_data;
  size = read_file_from_zip(file_path, absolute_cover_path, &cover_data);
  if (size == -1) {
    fprintf(f, "Could not read cover image\n");
    fclose(f);
    fclose(thumb);
    xmlFreeDoc(doc);
    return NULL;
  }
  fwrite(cover_data, size, 1, thumb);
  fclose(thumb);
  free(cover_data);
  cJSON_AddItemToObject(json, "thumbnail", cJSON_CreateString(thumbnail_path));

  xmlNode *book_title_node = find_element_by_name(root_element, "title");
  xmlChar *book_title = xmlNodeGetContent(book_title_node);
  cJSON *titles = cJSON_CreateArray();
  cJSON *title = cJSON_CreateObject();
  cJSON_AddItemToObject(title, "display",
                        cJSON_CreateString((const char *)book_title));
  cJSON_AddItemToArray(titles, title);
  cJSON_AddItemToObject(json, "titles", titles);

  // Cleanup function for the XML library
  xmlFreeDoc(doc);
  xmlCleanupParser();
  free(meta_inf);

  return json;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *file_path = argv[1];

    cJSON * json = generate_change_request(file_path, "45");

    char* lol = cJSON_Print(json);
    printf("josn: %s\n", lol);
    return EXIT_SUCCESS;
}
