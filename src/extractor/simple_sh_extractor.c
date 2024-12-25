#include <cJSON.h>
#include "utils.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <syslog.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simple_sh_extractor.h"

int parse_shit_app(const char* input, char* name, char* author, char* icon) {
    regex_t regex;
    regmatch_t matches[4];

    const char* pattern =
        "#\\sSH-IT\\sAPP\\s#$\n"
        "#\\sName:\\s([^#\\n]+)$\n"
        "#\\sAuthor:\\s([^#\\n]+)$\n"
        "#\\sIcon:\\s([^#\\n]+)$";

    if (regcomp(&regex, pattern, REG_EXTENDED | REG_NEWLINE)) {
        return -1;
    }

    if (regexec(&regex, input, 4, matches, 0) == 0) {
        int name_len = matches[1].rm_eo - matches[1].rm_so;
        strncpy(name, input + matches[1].rm_so, name_len);
        name[name_len] = '\0';

        int author_len = matches[2].rm_eo - matches[2].rm_so;
        strncpy(author, input + matches[2].rm_so, author_len);
        author[author_len] = '\0';

        int icon_len = matches[3].rm_eo - matches[3].rm_so;
        strncpy(icon, input + matches[3].rm_so, icon_len);
        icon[icon_len] = '\0';

        regfree(&regex);
        return 0;
    }

    regfree(&regex);
    return -1;
}

char* read_file(const char* filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, size, file);
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

cJSON *generate_change_request_sh(const char *file_path, const char *uuid) {
  syslog(LOG_INFO, "Generating change request for %s\n", file_path);

  char *meta_inf = NULL;
  cJSON *json = NULL;
  struct stat st;
  stat(file_path, &st);
  char name[256];
  char author[256];
  char icon[1024];
  char* shit_app = read_file(file_path);
  parse_shit_app(shit_app, (char*)&name, (char*)&author, (char*)&icon);

  json = generate_change_request("application/shit", uuid, file_path,
                                   st.st_mtim.tv_sec, st.st_size, author,
                                   "SHIT", name, icon);
  return json;
}
