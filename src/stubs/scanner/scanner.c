// libscanner stub file to link against
#include "cJSON.h"

int scanner_post_change(cJSON *json) { return 0; }

void scanner_gen_uuid(char *out, int buffer_size) {
  out = "00000000-0000-0000-0000-000000000000";
  return;
}

char *scanner_get_thumbnail_for_uuid(char *uuid) {
  return "/path/to/thumbnail.png";
}

void scanner_update_ccat(char *uuid, char *thumbnail_path) { return; }

void scanner_delete_ccat_entry(char *uuid) { return; }

char* getSha1Hash(const char* data) {
  return "000000000000000";
}
