#include "utils.h"

cJSON *generate_change_request(const char *uuid, const char *file_path,
                               int modification_time, int file_size,
                               const char *creator, const char *publisher,
                               const char *book_title,
                               const char *thumbnail_path) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddItemToObject(json, "uuid", cJSON_CreateString(uuid));
  cJSON_AddItemToObject(json, "location", cJSON_CreateString(file_path));
  cJSON_AddItemToObject(json, "type", cJSON_CreateString("Entry:Item"));
  cJSON_AddItemToObject(json, "cdeType", cJSON_CreateString("PDOC"));
  cJSON_AddItemToObject(json, "cdeKey",
                        cJSON_CreateString(getSha1Hash(file_path)));
  cJSON_AddItemToObject(json, "modificationTime",
                        cJSON_CreateNumber(modification_time));
  cJSON_AddItemToObject(json, "diskUsage", cJSON_CreateNumber(file_size));
  cJSON_AddItemToObject(json, "isVisibleInHome", cJSON_CreateTrue());
  cJSON_AddItemToObject(json, "isArchived", cJSON_CreateFalse());
  cJSON_AddItemToObject(json, "mimeType",
                        cJSON_CreateString("application/epub+zip"));
  cJSON *authors = cJSON_CreateArray();
  cJSON *author = cJSON_CreateObject();
  cJSON_AddItemToObject(author, "kind", cJSON_CreateString("Author"));
  cJSON *author_name = cJSON_CreateObject();
  cJSON_AddItemToObject(
      author_name, "display",
      cJSON_CreateString((const char *)(creator ? creator : "Unknown")));
  cJSON_AddItemToObject(author, "name", author_name);
  cJSON_AddItemToArray(authors, author);
  cJSON_AddItemToObject(json, "credits", authors);
  cJSON_AddItemToObject(
      json, "publisher",
      cJSON_CreateString((const char *)(publisher ? publisher : "Unknown")));
  cJSON *refs = cJSON_CreateArray();
  cJSON *titles_ref = cJSON_CreateObject();
  cJSON_AddItemToObject(titles_ref, "ref", cJSON_CreateString("titles"));
  cJSON_AddItemToArray(refs, titles_ref);
  cJSON *authors_ref = cJSON_CreateObject();
  cJSON_AddItemToObject(authors_ref, "ref", cJSON_CreateString("credits"));
  cJSON_AddItemToArray(refs, authors_ref);
  cJSON_AddItemToObject(json, "displayObjects", refs);
  const char *tags[] = {"NEW"};
  cJSON_AddItemToObject(json, "displayTags", cJSON_CreateStringArray(tags, 1));
  if (thumbnail_path) {
    cJSON_AddItemToObject(json, "thumbnail",
                          cJSON_CreateString(thumbnail_path));
  }
  cJSON *titles = cJSON_CreateArray();
  cJSON *title = cJSON_CreateObject();
  cJSON_AddItemToObject(
      title, "display",
      cJSON_CreateString((const char *)(book_title ? book_title : "Unknown")));
  cJSON_AddItemToArray(titles, title);
  cJSON_AddItemToObject(json, "titles", titles);
  return json;
}
