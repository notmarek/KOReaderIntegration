#include "utils.h"
#include "minizip/unzip.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <regex.h>

char* force_replace_encoding_to_utf8(const char* input) {
    regex_t regex;
    regmatch_t matches[1];
    syslog(LOG_INFO, "Replacing encoding.");
    // Compile the regex pattern
    if (regcomp(&regex, "encoding=\"(.*?)\"", REG_EXTENDED) != 0) {
        syslog(LOG_ERR, "Could not compile regex\n");
        return NULL;
    }

    // Allocate memory for the output string (make it large enough)
    char* output = malloc(strlen(input) + 1);
    strcpy(output, input);

    // Find and replace
    char* pos = output;
    while (regexec(&regex, pos, 1, matches, 0) == 0) {
        // Calculate the match position
        int match_start = matches[0].rm_so;
        int match_end = matches[0].rm_eo;
        // Create a new string with the replacement
        char temp[strlen(output) + 1];
        strncpy(temp, pos, match_start);
        temp[match_start] = '\0';

        strcat(temp, "encoding=\"UTF-8\"");
        strcat(temp, pos + match_end);

        // Copy back to output
        strcpy(output, temp);
        syslog(LOG_INFO, "Replaced encoding. %s", output    );
        // Move position to continue searching
        break;
        //pos = output + match_start + strlen("encoding=\"utf8\"");
    }

    // Free the compiled regex
    regfree(&regex);

    return output;
}

xmlNode *find_element_by_prop(xmlNode *start_node, const char *name,
                              const char *prop_name, const char *prop_value) {
  if (!start_node || !prop_name || !prop_value)
    return NULL;
  for (xmlNode *cur_node = start_node; cur_node; cur_node = cur_node->next) {
    xmlChar *val = xmlGetProp(cur_node, (const xmlChar *)prop_name);
    int match = (cur_node->type == XML_ELEMENT_NODE && val &&
                 xmlStrcmp(val, (const xmlChar *)prop_value) == 0 &&
                 xmlStrcmp(cur_node->name, (const xmlChar *)name) == 0);
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

int read_first_file_from_zip(const char *zipfile, char** out) {
    unzFile zfile = NULL;
    char *buffer = NULL;

    // Open the zip file
    zfile = unzOpen(zipfile);
    if (zfile == NULL) {
      fprintf(stderr, "Could not open zip file %s\n", zipfile);
      return -1;
    }

    // Locate the file in the zip archive
    if (unzGoToFirstFile(zfile) != UNZ_OK) {
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

cJSON *generate_change_request(const char *mimetype, const char *uuid,
                               const char *file_path, int modification_time,
                               int file_size, const char *creator,
                               const char *publisher, const char *book_title,
                               const char *thumbnail_path) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddItemToObject(json, "uuid", cJSON_CreateString(uuid));
  cJSON_AddItemToObject(json, "location", cJSON_CreateString(file_path));
  cJSON_AddItemToObject(json, "type", cJSON_CreateString("Entry:Item"));
  cJSON_AddItemToObject(json, "cdeType", cJSON_CreateString("PDOC"));
  cJSON_AddItemToObject(json, "cdeKey",
                        cJSON_CreateString(getSha1Hash(file_path)));
  cJSON_AddItemToObject(json, "modificationTime",
                        cJSON_CreateNumber(modification_time));
  cJSON_AddItemToObject(json, "diskUsage", cJSON_CreateNumber(file_size));
  cJSON_AddItemToObject(json, "isVisibleInHome", cJSON_CreateTrue());
  cJSON_AddItemToObject(json, "isArchived", cJSON_CreateFalse());
  cJSON_AddItemToObject(json, "mimeType", cJSON_CreateString(mimetype));
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
  cJSON_AddItemToObject(
      json, "publisher",
      cJSON_CreateString((const char *)(publisher ? publisher : "Unknown")));
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
  if (thumbnail_path) {
    cJSON_AddItemToObject(json, "thumbnail",
                          cJSON_CreateString(thumbnail_path));
  }
  cJSON *titles = cJSON_CreateArray();
  cJSON *title = cJSON_CreateObject();
  cJSON_AddItemToObject(
      title, "display",
      cJSON_CreateString((const char *)(book_title ? book_title : "Unknown")));
  cJSON_AddItemToArray(titles, title);
  cJSON_AddItemToObject(json, "titles", titles);
  return json;
}

// More comprehensive check with multiple magic numbers
bool is_zip_file(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    syslog(LOG_ERR, "is_zip_file: Couldn't read file: %s", filename);
    return false;
  }

  // Read first 8 bytes to check multiple ZIP signatures
  uint8_t magic[8];
  size_t bytes_read = fread(magic, 1, sizeof(magic), file);
  fclose(file);

  if (bytes_read < 4) {
    return false;
  }

  // Known ZIP magic numbers and signatures
  const uint8_t zip_magic_standard[] = {0x50, 0x4B, 0x03, 0x04}; // Standard ZIP
  const uint8_t zip_magic_empty[] = {0x50, 0x4B, 0x05,
                                     0x06}; // Empty ZIP archive
  const uint8_t zip_magic_spanned[] = {0x50, 0x4B, 0x07, 0x08}; // Spanned ZIP

  // Compare first 4 bytes against known signatures
  if (memcmp(magic, zip_magic_standard, 4) == 0)
    return true;
  if (memcmp(magic, zip_magic_empty, 4) == 0)
    return true;
  if (memcmp(magic, zip_magic_spanned, 4) == 0)
    return true;

  return false;
}
