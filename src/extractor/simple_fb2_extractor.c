#include "simple_fb2_extractor.h"
#include "base64.h"
#include "utils.h"
#include <minizip/unzip.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <syslog.h>

cJSON *generate_change_request_fb2(const char *file_path, const char *uuid) {
  syslog(LOG_INFO, "we are inside the fb2 indexer");
  bool unzip = is_zip_file(file_path);
  char *xml = "";
  xmlDoc *metadata_doc = NULL;
  if (unzip) {
    syslog(LOG_INFO, "unzipping");
    char filename[256];
    size_t filename_size = -1;
    char *data = NULL;
    read_first_file_from_zip(file_path, &data);
    metadata_doc =
        xmlParseDoc((const xmlChar *)force_replace_encoding_to_utf8(data));
  } else {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
      return NULL;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    char *buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL) {
      fclose(file);
      return NULL;
    }
    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = 0;
    fclose(file);

    metadata_doc =
        xmlParseDoc((const xmlChar *)force_replace_encoding_to_utf8(buffer));
    free(buffer);
  }
  if (metadata_doc == NULL) {
    syslog(LOG_ERR, "Fuck couldnt parse xml");
    return NULL;
  }
  xmlNode *root_element = xmlDocGetRootElement(metadata_doc);
  xmlNode *title_info = find_element_by_name(root_element, "book-title");
  xmlChar *book_title = title_info ? xmlNodeGetContent(title_info) : NULL;
  xmlNode *first_name = find_element_by_name(root_element, "first-name");
  xmlNode *last_name = find_element_by_name(root_element, "last-name");
  xmlChar *first_name_str = first_name ? xmlNodeGetContent(first_name) : NULL;
  xmlChar *last_name_str = last_name ? xmlNodeGetContent(last_name) : NULL;
  xmlNode *publisher = find_element_by_name(root_element, "publisher");
  xmlChar *publisher_str = publisher ? xmlNodeGetContent(publisher) : NULL;
  char *author = malloc(strlen(first_name_str) + strlen(last_name_str) + 1 + 1);
  sprintf(author, "%s %s", first_name_str, last_name_str);
  xmlNode *cover = find_element_by_name(root_element, "coverpage");
  xmlNode *cover_path_node = find_element_by_name(cover, "image");
  xmlChar *cover_path =
      cover_path_node ? xmlGetProp(cover_path_node, "l:href") : NULL;
  xmlNode *cover_bin_node = find_element_by_prop(
      root_element, "binary", "id", (char *)(cover_path + 1)); // skip #
  xmlChar *cover_bin =
      cover_bin_node ? xmlNodeGetContent(cover_bin_node) : NULL;
  char thumbnail_path[40 + 37];
  snprintf((char *)&thumbnail_path, 40 + 37,
           "/mnt/us/system/thumbnails/thumbnail_%s.jpg", uuid);

  FILE *thumb = fopen((const char *)&thumbnail_path, "w");
  if (thumb == NULL) {
    syslog(LOG_ERR, "Could not open thumbnail file\n");
    // do cleanup
  }
  char *cover_data = NULL;
  size_t size = 0;
  base64_decode((const unsigned char *)cover_bin, strlen(cover_bin), &size);
  fwrite(cover_data, size, 1, thumb);
  fclose(thumb);

  struct stat file_stat;
  stat(file_path, &file_stat);

  cJSON *json = generate_change_request(
      "application/fb2+zip", uuid, file_path, file_stat.st_mtim.tv_sec,
      file_stat.st_size, author, publisher_str, book_title, thumbnail_path);

  return json;
}
