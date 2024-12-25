#include <cJSON.h>
#include <libxml2.h>
#include <minizip/unzip.h>
#include <scanner.h>
#include <stdbool.h>

xmlNode *find_element_by_prop(xmlNode *start_node, const char *name,
                              const char *prop_name, const char *prop_value);
xmlNode *find_element_by_name(xmlNode *start_node, const char *name);
int read_first_file_from_zip(const char *zipfile, char **out);
int read_file_from_zip(const char *zipfile, const char *filename, char **out);
cJSON *generate_change_request(const char *mimetype, const char *uuid,
                               const char *file_path, int modification_time,
                               int file_size, const char *creator,
                               const char *publisher, const char *book_title,
                               const char *thumbnail_path);
bool is_zip_file(const char *filename);
char* force_replace_encoding_to_utf8(const char* input);
