#include "cJSON.h"
#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void index_file(char *path, char *filename) {
  FILE *f = fopen("/tmp/indexer.log", "a");
  fprintf(f, "Indexing file: %s/%s\n", path, filename);
  fflush(f);

  char uuid[37] = {0};
  scanner_gen_uuid((char *)&uuid, 37);
  fprintf(f, "UUID: %s\n", uuid);
  cJSON *json = cJSON_CreateObject();
  cJSON_AddItemToObject(json, "type", cJSON_CreateString("ChangeRequest"));
  cJSON *arr = cJSON_CreateArray();
  cJSON *what = cJSON_CreateObject();
  cJSON *change = cJSON_CreateObject();
  cJSON_AddItemToObject(change, "uuid", cJSON_CreateString(uuid));
  char *app_path = malloc(strlen(path) + strlen(filename) + 1);
  sprintf(app_path, "%s/%s", path, filename);
  cJSON_AddItemToObject(change, "location", cJSON_CreateString(app_path));
  cJSON_AddItemToObject(change, "type", cJSON_CreateString("Entry:Item"));
  cJSON_AddItemToObject(change, "modificationTime",
                        cJSON_CreateNumber(time(NULL)));
  cJSON_AddItemToObject(change, "isVisibleInHome", cJSON_CreateTrue());
  cJSON_AddItemToObject(change, "isArchived", cJSON_CreateFalse());
  cJSON_AddItemToObject(change, "diskUsage", cJSON_CreateNumber(0));
  cJSON *titles = cJSON_CreateArray();
  cJSON *title = cJSON_CreateObject();
  cJSON_AddItemToObject(title, "display", cJSON_CreateString(filename));
  cJSON_AddItemToArray(titles, title);

  cJSON_AddItemToObject(change, "titles", titles);
  cJSON_AddItemToObject(what, "insert", change);
  cJSON_AddItemToArray(arr, what);
  cJSON_AddItemToObject(json, "commands", arr);

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
