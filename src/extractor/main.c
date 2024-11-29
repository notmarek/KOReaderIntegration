#include "cJSON.h"
#include "scanner.h"
#include "simple_epub_extractor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void index_file(char *path, char *filename) {
  FILE *f = fopen("/tmp/indexer.log", "a");
  fprintf(f, "Indexing file: %s/%s\n", path, filename);
  fflush(f);
  char *full_path = malloc(strlen(path) + strlen(filename) + 2);
  sprintf(full_path, "%s/%s", path, filename);

  char uuid[37] = {0};
  scanner_gen_uuid((char *)&uuid, 37);
  fprintf(f, "UUID: %s\n", uuid);
  cJSON *json = cJSON_CreateObject();
  cJSON_AddItemToObject(json, "type", cJSON_CreateString("ChangeRequest"));
  cJSON *arr = cJSON_CreateArray();
  cJSON *what = cJSON_CreateObject();
  cJSON *change = generate_change_request(full_path, uuid);

  cJSON *filter = cJSON_CreateObject();
  cJSON *Equals = cJSON_CreateObject();
  cJSON *location_filter = cJSON_CreateObject();
  cJSON_AddItemToObject(location_filter, "path",
                        cJSON_CreateString("location"));
  cJSON_AddItemToObject(location_filter, "value",
                        cJSON_CreateString(full_path));
  cJSON_AddItemToObject(filter, "onConflict", cJSON_CreateString("REPLACE"));
  cJSON_AddItemToObject(Equals, "Equals", location_filter);
  cJSON_AddItemToObject(filter, "filter", Equals);
  cJSON_AddItemToObject(filter, "entry", change);
  cJSON_AddItemToObject(what, "insertOr", filter);
  cJSON_AddItemToArray(arr, what);
  cJSON_AddItemToObject(json, "commands", arr);
  char* string = cJSON_Print(json);
  fprintf(f, "JSON: %s\n", string);
  fflush(f);
  int result = scanner_post_change(json);
  if (result >= 0) {
    fprintf(f, "success adding to ccat.\n");
  }
  fprintf(f, "Result: %d\n", result);

  cJSON_Delete(json);
  fflush(f);
  fclose(f);
}

FILE *f;
int epub_extractor(const struct scanner_event *event) {
  switch (event->event_type) {
  case SCANNER_ADD:

    f = fopen("/tmp/epub_extractor.log", "a");
    fprintf(f, "SCANNER_ADD path: %s, filename: %s, uuid: %s, glob: %s\n",
            event->path, event->filename, event->uuid, event->glob);
    fflush(f);
    fclose(f);
    index_file(event->path, event->filename);

    // Extract the epub file to ccat
    break;
  case SCANNER_DELETE:
    f = fopen("/tmp/epub_extractor.log", "a");
    fprintf(f, "SCANNER_DELETE path: %s, filename: %s, uuid: %s, glob: %s\n",
            event->path, event->filename, event->uuid, event->glob);
    fflush(f);
    fclose(f);

    char *app_path = malloc(strlen(event->path) + strlen(event->filename) + 1);
    sprintf(app_path, "%s/%s", event->path, event->filename);
    remove(app_path);
    free(app_path);
    scanner_delete_ccat_entry(event->uuid);
    // Remove the extracted epub file from ccat
    break;
  case SCANNER_UPDATE:
    f = fopen("/tmp/epub_extractor.log", "a");
    fprintf(f, "SCANNER_UPDATE path: %s, filename: %s, uuid: %s, glob: %s\n",
            event->path, event->filename, event->uuid, event->glob);
    fflush(f);
    fclose(f);
    // Update the extracted epub file in ccat
    break;
  default:
    f = fopen("/tmp/epub_extractor.log", "a");
    fprintf(f, "Unknown event type: %d\n", event->event_type);
    fflush(f);
    fclose(f);
    return 0; // ignore unknown event types
  }
  return 0;
}

[[gnu::visibility("default")]]
void load_epub_extractor(ScannerEventHandler **handler, int *unk1) {
  *handler = epub_extractor;
  *unk1 = 0;
}
