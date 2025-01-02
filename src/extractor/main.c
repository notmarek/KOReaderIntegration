#include "scanner.h"
#include "simple_epub_extractor.h"
#include "simple_fb2_extractor.h"
#include <cJSON.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <time.h>
#include <unistd.h>

typedef cJSON *(ExtractFile)(const char *file_path, const char *uuid);
void index_file(char *path, char *filename, ExtractFile *extractor, char* uuid_override) {
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

  // Log indexing start
  syslog(LOG_INFO, "Indexing file: %s/%s", path, filename);

  // Create full path
  full_path = malloc(strlen(path) + strlen(filename) + 2);
  if (!full_path) {
    syslog(LOG_CRIT, "Failed to malloc memory for file_path.");
    goto cleanup;
  }
  sprintf(full_path, "%s/%s", path, filename);

  // Generate UUID
  if (uuid_override) {
    strncpy(uuid, uuid_override, 37);
  } else {
      scanner_gen_uuid((char *)&uuid, 37);
      if (uuid[0] == 0) {
          syslog(LOG_INFO, "Failed to generate UUID");
          goto cleanup;
      }
  }
  syslog(LOG_INFO, "Generated new UUID: %s", uuid);

  // Create JSON objects
  json = cJSON_CreateObject();
  if (!json) {
    syslog(LOG_CRIT, "Failed to create a JSON object");
    goto cleanup;
  }

  cJSON_AddItemToObject(json, "type", cJSON_CreateString("ChangeRequest"));

  arr = cJSON_CreateArray();
  if (!arr) {
    syslog(LOG_CRIT, "Failed to create a JSON array");
    goto cleanup;
  }

  what = cJSON_CreateObject();
  if (!what) {

    syslog(LOG_CRIT, "Failed to create a JSON object");
    goto cleanup;
  }

  change = extractor(full_path, uuid);
  if (!change) {
    syslog(LOG_CRIT, "Failed to generate a change request");
    goto cleanup;
  }
  // Remove last ".epub" from full_path and store in a new char*
  char *base_path = strdup(full_path);
  if (!base_path) {

    syslog(LOG_CRIT, "Failed to malloc memory for base_path");
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

    syslog(LOG_INFO, "Opened sdr file metadata file: %s", sdr_path);
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    regex_t regex;
    regex_t complete_regex;
    int value =
        regcomp(&regex, "\\[\"percent_finished\"\\] = (.*?),", REG_EXTENDED);
    if (value != 0) {

      syslog(LOG_CRIT, "Failed to compile percent_finished regex.");
      goto cleanup;
    }
    value = regcomp(&complete_regex, "\\[\"status\"\\] = \"complete\",",
                    REG_EXTENDED);
    if (value != 0) {

      syslog(LOG_CRIT, "Failed to compile status regex.");
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

          syslog(LOG_CRIT, "Failed to exec percent_finished regex.");
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

    syslog(LOG_INFO, "Couldn't open sdr metadata: %s", sdr_path);
  }
  free(sdr_path);

  location_filter = cJSON_CreateObject();
  if (!location_filter) {

    syslog(LOG_CRIT, "Failed to create json objecy");
    goto cleanup;
  }
  cJSON_AddItemToObject(location_filter, "path",
                        cJSON_CreateString("location"));
  cJSON_AddItemToObject(location_filter, "value",
                        cJSON_CreateString(full_path));

  Equals = cJSON_CreateObject();
  if (!Equals) {

    syslog(LOG_CRIT, "Failed to create json objecy");
    goto cleanup;
  }
  cJSON_AddItemToObject(Equals, "Equals", location_filter);

  filter = cJSON_CreateObject();
  if (!filter) {

    syslog(LOG_CRIT, "Failed to create json objecy");
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

    syslog(LOG_CRIT, "Failed to convert JSON to string");
    goto cleanup;
  }

  syslog(LOG_DEBUG, "ChangeRequest JSON: %s", json_string);

  // Post change
  int result = scanner_post_change(json);
  if (result >= 0) {
    syslog(LOG_INFO, "Success adding to ccat.");
  }

  syslog(LOG_INFO, "ccat error: %d.", result);

cleanup:
  // Free all allocated resources
  free(full_path);
  free(json_string);

  // Careful deletion of JSON objects to avoid double-free
  if (json) {
    // Note: cJSON_Delete will recursively free child objects
    cJSON_Delete(json);
  }
}

int extractor(const struct scanner_event *event) {
  FILE *log_file = NULL;
  char *app_path = NULL;
  ExtractFile *extractor = NULL;
  if (strcmp(event->glob, "*.epub") == 0) {

    syslog(LOG_INFO, "Indexing EPUB.");
    extractor = generate_change_request_epub;
  } else if (strcmp(event->glob, "*.fb2") == 0 ||
             strcmp(event->glob, "*.fb2.zip") == 0 ||
             strcmp(event->glob, "*.fbz") == 0) {
#ifdef FB2_SUPPORT
    syslog(LOG_INFO, "Indexing FB2.");
    extractor = generate_change_request_fb2;
#else
    syslog(LOG_INFO, "FB2 support not enabled.");
#endif
  }

  switch (event->event_type) {
  case SCANNER_ADD:
    syslog(LOG_INFO, "SCANNER_ADD path: %s, filename: %s, uuid: %s, glob: %s\n",
           event->path, event->filename, event->uuid, event->glob);
    index_file(event->path, event->filename, extractor, NULL);
    break;

  case SCANNER_DELETE:
    syslog(LOG_INFO,
           "SCANNER_DELETE path: %s, filename: %s, uuid: %s, glob: %s\n",
           event->path, event->filename, event->uuid, event->glob);

    // Create thumbnail path for deletion
    char* thumb_path = malloc(40 + 37);
    if (thumb_path) {
        snprintf(thumb_path, 40 + 37,
                 "/mnt/us/system/thumbnails/thumbnail_%s.jpg", event->uuid);
        remove(thumb_path);
        free(thumb_path);
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
    syslog(LOG_INFO,
           "SCANNER_UPDATE path: %s, filename: %s, uuid: %s, glob: %s\n",
           event->path, event->filename, event->uuid, event->glob);
    index_file(event->path, event->filename, extractor, event->uuid);
    break;

  default:
    syslog(
        LOG_WARNING,
        "UNKNOWN EVENT TYPE: %d, path: %s, filename: %s, uuid: %s, glob: %s\n",
        event->event_type, event->path, event->filename, event->uuid,
        event->glob);
    return 0;
  }
  return 0;
}

[[gnu::visibility("default")]]
int load_extractors(ScannerEventHandler **handler, int *unk1) {
  openlog("notmarek.extractor", LOG_PID, LOG_DAEMON);
  *handler = extractor;
  *unk1 = 0;
  return 0;
}
