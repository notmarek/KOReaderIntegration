#include "cJSON.h"
#include "scanner.h"
#include "simple_epub_extractor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void index_file(char *path, char *filename) {
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

    change = generate_change_request(full_path, uuid);
    if (!change) {
        fprintf(f, "Failed to generate change request\n");
        goto cleanup;
    }

    location_filter = cJSON_CreateObject();
    if (!location_filter) {
        fprintf(f, "Failed to create location filter\n");
        goto cleanup;
    }
    cJSON_AddItemToObject(location_filter, "path", cJSON_CreateString("location"));
    cJSON_AddItemToObject(location_filter, "value", cJSON_CreateString(full_path));

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
    if (f) fclose(f);
}

int epub_extractor(const struct scanner_event *event) {
    FILE *log_file = NULL;
    char *app_path = NULL;

    switch (event->event_type) {
    case SCANNER_ADD:
        log_file = fopen("/tmp/epub_extractor.log", "a");
        if (log_file) {
            fprintf(log_file, "SCANNER_ADD path: %s, filename: %s, uuid: %s, glob: %s\n",
                    event->path, event->filename, event->uuid, event->glob);
            fflush(log_file);
            fclose(log_file);
        }
        index_file(event->path, event->filename);
        break;

    case SCANNER_DELETE:
        log_file = fopen("/tmp/epub_extractor.log", "a");
        if (log_file) {
            fprintf(log_file, "SCANNER_DELETE path: %s, filename: %s, uuid: %s, glob: %s\n",
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
            fprintf(log_file, "SCANNER_UPDATE path: %s, filename: %s, uuid: %s, glob: %s\n",
                    event->path, event->filename, event->uuid, event->glob);
            fflush(log_file);
            fclose(log_file);
        }
        break;

    default:
        log_file = fopen("/tmp/epub_extractor.log", "a");
        index_file(event->path, event->filename);
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
void load_epub_extractor(ScannerEventHandler **handler, int *unk1) {
    *handler = epub_extractor;
    *unk1 = 0;
}
