#include "scanner.h"
#include "simple_epub_extractor.h"
#include "simple_fb2_extractor.h"
#include <cJSON.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef cJSON *(ExtractFile)(const char *file_path, const char *uuid);
void index_file(char *path, char *filename, ExtractFile *extractor) {
  FILE *f = NULL;
  char *full_path = NULL;
  char uuid[37] = {0};
  cJSON *json = NULL;
  cJSON *arr = NULL;
  cJSON *what = NULL;
  cJSON *change = NULL;
  cJSON *filter = NULL;
  cJSON *Equals = NULL;
  cJSON *location_filter = NULL;
  char *json_string = NULL;

  // Open log file
  f = fopen("/tmp/indexer.log", "a");
  if (!f) {
    perror("Failed to open indexer log file");
    return;
  }

  // Log indexing start
  fprintf(f, "Indexing file: %s/%s\n", path, filename);
  fflush(f);

  // Create full path
  full_path = malloc(strlen(path) + strlen(filename) + 2);
  if (!full_path) {
    perror("Memory allocation failed for full_path");
    goto cleanup;
  }
  sprintf(full_path, "%s/%s", path, filename);

  // Generate UUID
  scanner_gen_uuid((char *)&uuid, 37);
  if (uuid[0] == 0) {
    fprintf(f, "Failed to generate UUID\n");
    goto cleanup;
  }
  fprintf(f, "UUID: %s\n", uuid);

  // Create JSON objects
  json = cJSON_CreateObject();
  if (!json) {
    fprintf(f, "Failed to create JSON object\n");
    goto cleanup;
  }

  cJSON_AddItemToObject(json, "type", cJSON_CreateString("ChangeRequest"));

  arr = cJSON_CreateArray();
  if (!arr) {
    fprintf(f, "Failed to create JSON array\n");
    goto cleanup;
  }

  what = cJSON_CreateObject();
  if (!what) {
    fprintf(f, "Failed to create 'what' JSON object\n");
    goto cleanup;
  }

  change = extractor(full_path, uuid);
  if (!change) {
    fprintf(f, "Failed to generate change request\n");
    goto cleanup;
  }
  // Remove last ".epub" from full_path and store in a new char*
  char *base_path = strdup(full_path);
  if (!base_path) {
    perror("Memory allocation failed for base_path");
    goto cleanup;
  }
  base_path[strlen(base_path) - 5] = 0;
  char *sdr_path = malloc(strlen(base_path) + 22 + 1);
  sprintf(sdr_path, "%s.sdr/metadata.epub.lua", base_path);
  FILE *sdr = fopen(sdr_path, "r");
  if (sdr != 0) {
    struct stat st;
    stat(sdr_path, &st);
    cJSON_AddItemToObject(change, "lastAccess",
                          cJSON_CreateNumber(st.st_mtime));
    fprintf(f, "openeded : %s\n", sdr_path);
    fflush(f);
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    regex_t regex;
    regex_t complete_regex;
    int value =
        regcomp(&regex, "\\[\"percent_finished\"\\] = (.*?),", REG_EXTENDED);
    if (value != 0) {
      fprintf(f, "Failed to compile regex\n");
      goto cleanup;
    }
    value = regcomp(&complete_regex, "\\[\"status\"\\] = \"complete\",",
                    REG_EXTENDED);
    if (value != 0) {
      fprintf(f, "Failed to compile regex\n");
      goto cleanup;
    }
    regmatch_t *matches = malloc(2 * sizeof(regmatch_t));
    while ((read = getline(&line, &len, sdr)) != -1) {
      value = regexec(&complete_regex, line, 0, NULL, 0);
      if (value == 0) {
        cJSON_AddItemToObject(change, "percentFinished",
                              cJSON_CreateNumber(100));
        cJSON_ReplaceItemInObject(change, "displayTags", cJSON_CreateArray());
        cJSON_AddItemToObject(change, "readState", cJSON_CreateNumber(1));
        break;
      } else {
        value = regexec(&regex, line, 2, matches, 0);
        if (value == REG_NOMATCH) {
          continue;
        } else if (value != 0) {
          fprintf(f, "Failed to execute regex\n");
          goto cleanup;
        } else {
          char *percent_finished =
              malloc(matches[1].rm_eo - matches[1].rm_so + 1);
          strncpy(percent_finished, line + matches[1].rm_so,
                  matches[1].rm_eo - matches[1].rm_so);
          percent_finished[matches[1].rm_eo - matches[1].rm_so] = '\0';
          double percent = strtod(percent_finished, NULL) * 100;

          cJSON_AddItemToObject(change, "percentFinished",
                                cJSON_CreateNumber(percent));
          cJSON_ReplaceItemInObject(change, "displayTags", cJSON_CreateArray());
          if (percent >= 100) {
            cJSON_AddItemToObject(change, "readState", cJSON_CreateNumber(1));
          } else {
            cJSON_AddItemToObject(change, "readState", cJSON_CreateNumber(0));
          }
          free(percent_finished);
          break;
        }
      }
    }
    free(matches);
    fclose(sdr);
  } else {
    fprintf(f, "Couldnt open : %s\n", sdr_path);
  }
  free(sdr_path);

  location_filter = cJSON_CreateObject();
  if (!location_filter) {
    fprintf(f, "Failed to create location filter\n");
    goto cleanup;
  }
  cJSON_AddItemToObject(location_filter, "path",
                        cJSON_CreateString("location"));
  cJSON_AddItemToObject(location_filter, "value",
                        cJSON_CreateString(full_path));

  Equals = cJSON_CreateObject();
  if (!Equals) {
    fprintf(f, "Failed to create Equals object\n");
    goto cleanup;
  }
  cJSON_AddItemToObject(Equals, "Equals", location_filter);

  filter = cJSON_CreateObject();
  if (!filter) {
    fprintf(f, "Failed to create filter object\n");
    goto cleanup;
  }
  cJSON_AddItemToObject(filter, "onConflict", cJSON_CreateString("REPLACE"));
  cJSON_AddItemToObject(filter, "filter", Equals);
  cJSON_AddItemToObject(filter, "entry", change);

  cJSON_AddItemToObject(what, "insertOr", filter);
  cJSON_AddItemToArray(arr, what);
  cJSON_AddItemToObject(json, "commands", arr);

  // Print JSON
  json_string = cJSON_Print(json);
  if (!json_string) {
    fprintf(f, "Failed to convert JSON to string\n");
    goto cleanup;
  }
  fprintf(f, "JSON: %s\n", json_string);
  fflush(f);

  // Post change
  int result = scanner_post_change(json);
  fprintf(f, "Result: %d\n", result);
  if (result >= 0) {
    fprintf(f, "success adding to ccat.\n");
  }

cleanup:
  // Free all allocated resources
  free(full_path);
  free(json_string);

  // Careful deletion of JSON objects to avoid double-free
  if (json) {
    // Note: cJSON_Delete will recursively free child objects
    cJSON_Delete(json);
  }

  fflush(f);
  if (f)
    fclose(f);
}

int extractor(const struct scanner_event *event) {
  FILE *log_file = NULL;
  char *app_path = NULL;

  ExtractFile *extractor = NULL;
  if (strcmp(event->glob, "GL:*.epub")) {
    extractor = generate_change_request_epub;
  } else if (strcmp(event->glob, "GL:*.fb2")) {
    extractor = generate_change_request_fb2;
  }

  switch (event->event_type) {
  case SCANNER_ADD:
    log_file = fopen("/tmp/epub_extractor.log", "a");
    if (log_file) {
      fprintf(log_file,
              "SCANNER_ADD path: %s, filename: %s, uuid: %s, glob: %s\n",
              event->path, event->filename, event->uuid, event->glob);
      fflush(log_file);
      fclose(log_file);
    }
    index_file(event->path, event->filename, extractor);
    break;

  case SCANNER_DELETE:
    log_file = fopen("/tmp/epub_extractor.log", "a");
    if (log_file) {
      fprintf(log_file,
              "SCANNER_DELETE path: %s, filename: %s, uuid: %s, glob: %s\n",
              event->path, event->filename, event->uuid, event->glob);
      fflush(log_file);
      fclose(log_file);
    }

    // Create full path for deletion
    app_path = malloc(strlen(event->path) + strlen(event->filename) + 2);
    if (app_path) {
      sprintf(app_path, "%s/%s", event->path, event->filename);
      remove(app_path);
      free(app_path);
    }

    // Delete entry from ccat
    scanner_delete_ccat_entry(event->uuid);
    break;

  case SCANNER_UPDATE:
    log_file = fopen("/tmp/epub_extractor.log", "a");
    if (log_file) {
      fprintf(log_file,
              "SCANNER_UPDATE path: %s, filename: %s, uuid: %s, glob: %s\n",
              event->path, event->filename, event->uuid, event->glob);
      fflush(log_file);
      fclose(log_file);
    }
    break;

  default:
    log_file = fopen("/tmp/epub_extractor.log", "a");
    index_file(event->path, event->filename, extractor);
    if (log_file) {
      fprintf(log_file, "Unknown event type: %d\n", event->event_type);
      fflush(log_file);
      fclose(log_file);
    }
    return 0;
  }
  return 0;
}

[[gnu::visibility("default")]]
int load_extractors(ScannerEventHandler **handler, int *unk1) {
  *handler = extractor;
  *unk1 = 0;
  return 0;
}
