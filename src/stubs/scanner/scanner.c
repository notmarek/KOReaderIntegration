// libscanner stub file to link against
#include "cJSON.h"


int scanner_post_change(cJSON* json) {
    return 0;
}

void scanner_gen_uuid(char* out, int buffer_size) {
    return;
}

char* scanner_get_thumbnail_for_uuid(char* uuid) {
    return "/path/to/thumbnail.png";
}
