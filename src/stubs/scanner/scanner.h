#include "cJSON.h"

enum ScannerEventType {
    SCANNER_ADD,
    SCANNER_DELETE,
    SCANNER_UPDATE,
    ADD_THUMB,
    UPDATE_THUMB,
};

struct scanner_event
{
    enum ScannerEventType event_type;
    char* path;
    void* lipchandle;
    char* filename;
    char* uuid;
    char* glob;
};

typedef int(ScannerEventHandler)(const struct scanner_event* event);

int scanner_post_change(cJSON* json);
void scanner_gen_uuid(char* out, int buffer_size);
char* scanner_get_thumbnail_for_uuid(char* uuid);
void scanner_update_ccat(char* uuid, char* thumbnail_path);