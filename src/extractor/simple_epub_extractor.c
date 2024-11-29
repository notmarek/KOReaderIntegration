#include "cJSON.h"
#include "libxml2.h"
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

cJSON *generate_change_request(char *file_path, char *uuid) {
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

  json = cJSON_CreateObject();
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

  // Process creator
  xmlNode *creator_node = find_element_by_name(root_element, "creator");
  xmlChar *creator = creator_node ? xmlNodeGetContent(creator_node) : NULL;
  cJSON *authors = cJSON_CreateArray();
  cJSON *author = cJSON_CreateObject();
  cJSON_AddItemToObject(author, "kind", cJSON_CreateString("Author"));
  cJSON *author_name = cJSON_CreateObject();
  cJSON_AddItemToObject(
      author_name, "display",
      cJSON_CreateString((const char *)(creator ? creator : "Unknown")));
  cJSON_AddItemToObject(author, "name", author_name);
  cJSON_AddItemToArray(authors, author);
  cJSON_AddItemToObject(json, "credits", authors);

  // Free creator content if it was allocated
  // if (creator) xmlFree(creator);

  // Process publisher
  xmlNode *publisher_node = find_element_by_name(root_element, "publisher");
  xmlChar *publisher =
      publisher_node ? xmlNodeGetContent(publisher_node) : NULL;
  cJSON_AddItemToObject(
      json, "publisher",
      cJSON_CreateString((const char *)(publisher ? publisher : "Unknown")));

  // Free publisher content if it was allocated
  // if (publisher) xmlFree(publisher);

  // Rest of the JSON creation remains the same...
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
  //
  xmlNode *cover_id_node =
      find_element_by_prop(root_element, "meta", "name", "cover");
  if (cover_id_node == NULL) {

    fprintf(f, "Could not find cover meta element\n");
    // Cleanup before returning
    free(metadata);
    xmlFreeDoc(metadata_doc);
    xmlFreeDoc(doc);
    free(meta_inf);
    fclose(f);
    cJSON_Delete(json);
    return NULL;
  }

  xmlChar *cover_id = xmlGetProp(cover_id_node, (xmlChar *)"content");
  xmlNode *cover_node =
      find_element_by_prop(root_element, "item", "id", (const char *)cover_id);
  xmlChar *cover_path = xmlGetProp(cover_node, (xmlChar *)"href");

  char *absolute_cover_path = NULL;
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

  char thumbnail_path[40 + 37];
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

  cJSON_AddItemToObject(json, "thumbnail", cJSON_CreateString(thumbnail_path));

  // Process book title
  xmlNode *book_title_node = find_element_by_name(root_element, "title");
  xmlChar *book_title =
      book_title_node ? xmlNodeGetContent(book_title_node) : NULL;
  cJSON *titles = cJSON_CreateArray();
  cJSON *title = cJSON_CreateObject();
  cJSON_AddItemToObject(
      title, "display",
      cJSON_CreateString((const char *)(book_title ? book_title : "Unknown")));
  cJSON_AddItemToArray(titles, title);
  cJSON_AddItemToObject(json, "titles", titles);

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

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *file_path = argv[1];

  cJSON *json = generate_change_request(file_path, "45");
  if (json == NULL) {
    fprintf(stderr, "Failed to generate change request\n");
    return EXIT_FAILURE;
  }

  char *json_str = cJSON_Print(json);

  // Free JSON string and JSON object
  free(json_str);
  cJSON_Delete(json);

  return EXIT_SUCCESS;
}
