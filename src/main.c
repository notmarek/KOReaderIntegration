#include "openlipc.h"
#include "sqlite3.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>

struct LipcStringHandler;

typedef LIPCcode(SimpleStringSetterCB)(struct LipcStringHandler *this,
                                       LIPC *lipc, char *value);
typedef LIPCcode(SimpleStringGetterCB)(struct LipcStringHandler *this,
                                       LIPC *lipc, char **value);

struct LipcStringHandler {
  const char *command;
  void *data;
  SimpleStringGetterCB *getter;
  SimpleStringSetterCB *setter;
};

struct StringHandlerEntry {
  struct LipcStringHandler *handler;
  struct StringHandlerEntry *prev;
};

struct StringHandlerEntry *tail = NULL;

LIPCcode lipc_string_callbacker(bool setter, LIPC *lipc, const char *property,
                                void *value, void *data) {
  struct StringHandlerEntry *node = tail;
  while (node != NULL) {
    if (strcmp(property, node->handler->command) == 0) {
      char *out;
      if (setter) {
        return node->handler->setter(node->handler, lipc, value);
      }
      LIPCcode res = (node->handler->getter)(node->handler, lipc, &out);
      if (*(int *)data < strlen(out) + 1) {
        *(int *)data = strlen(out) + 1;
        return LIPC_ERROR_BUFFER_TOO_SMALL;
      }
      strcpy(value, out);
      free(out);
      return res;
    }
    if (node->prev == NULL)
      break;
    node = node->prev;
  }
  return LIPC_ERROR_NO_SUCH_PROPERTY;
}

LIPCcode lipc_string_setter(LIPC *lipc, const char *property, void *value,
                            void *data) {
  return lipc_string_callbacker(true, lipc, property, value, data);
}

LIPCcode lipc_string_getter(LIPC *lipc, const char *property, void *value,
                            void *data) {
  return lipc_string_callbacker(false, lipc, property, value, data);
}

int register_string_handler(LIPC *lipc, const char *command,
                            SimpleStringGetterCB *getter,
                            SimpleStringSetterCB *setter) {
  printf("Registering string handler for %s\n", command);
  LipcRegisterStringProperty(lipc, command,
                             getter == NULL ? NULL : lipc_string_getter,
                             setter == NULL ? NULL : lipc_string_setter, NULL);
  struct LipcStringHandler *handler = malloc(sizeof(struct LipcStringHandler));
  handler->command = command;
  handler->getter = getter;
  handler->setter = setter;
  handler->data = NULL;
  struct StringHandlerEntry *new_node =
      malloc(sizeof(struct StringHandlerEntry));
  new_node->handler = handler;
  new_node->prev = tail;
  tail = new_node;
  return 0;
}

LIPCcode lipc_string_setter_exit(struct LipcStringHandler *this, LIPC *lipc,
                                 char *value) {
  printf("Closing.\n");
  LipcClose(lipc);
  exit(0);
  return LIPC_OK;
}

LIPCcode run_cmd_getter(struct LipcStringHandler *this, LIPC *lipc,
                        char **value) {
  if (this->data != NULL) {
    *value = (char *)malloc(strlen(this->data) + 1);
    strcpy(*value, this->data);
    free(this->data);
    this->data = NULL;
  } else {
    *value = (char *)malloc(5);
    strcpy(*value, "null");
  }
  return LIPC_OK;
}

LIPCcode run_cmd_setter(struct LipcStringHandler *this, LIPC *lipc,
                        char *value) {
  printf("hello from cmd: %s\n", value);

  const char *out = "Command failed";
  char buf[100];
  char *str = NULL;
  char *temp = NULL;
  unsigned int size = 1;
  unsigned int strlength;

  FILE *ls;
  if (NULL == (ls = popen(value, "r"))) {
    out = "Failed to run command";
    return LIPC_ERROR_INTERNAL;
  }

  while (fgets(buf, sizeof(buf), ls) != NULL) {
    strlength = strlen(buf);
    temp = realloc(str, size + strlength);
    if (temp == NULL) {
      out = "Failed to realloc temp";
      return LIPC_ERROR_INTERNAL;
    } else {
      str = temp;
    }
    strcpy(str + size - 1, buf); // append buffer to str
    size += strlength;
  }
  out = str;
  char *malloced = malloc(strlen(out) + 1);
  strcpy(malloced, out);
  if (this->data != NULL) {
      free(this->data);
  }
  this->data = (void *)malloced;
  return LIPC_OK;
}
int icuCompare(void* udp, int sizeA, const void* textA,
                          int sizeB, const void* textB) {
    int result = sizeA | sizeB;
    if (result != 0) {
        if (sizeA == 0) {
            result = 1;
        } else if (sizeB == 0) {
            result = 0xffffffff;
        } else {
            result = strcoll(textA, textB);
        }
    }
    return result;

}
LIPCcode register_app(struct LipcStringHandler *this, LIPC *lipc, char *value) {
  // split value by "\n"
  const char *app_name = strtok(value, ";");
  const char *app_command = strtok(NULL, "");
  if (app_name == NULL || app_command == NULL) {
    return LIPC_ERROR_NO_SUCH_PARAM;
  }

  // Register the app inside appreg.db
  const char *reg =
      "{\"def\":\"interface\",\"assoc\":{\"interface\":\"application\"}},{"
      "\"def\":\"handlerId\",\"assoc\":{\"handlerId\":\"com.notmarek.jb%s\","
      "\"props\":{\"command\":\"%s\"}}},{\"def\":\"association\",\"assoc\":{"
      "\"interface\":\"application\",\"handlerId\":\"com.notmarek.jb%s\","
      "\"contentIds\":[\"MT:image/"
      "x.jb%s\"],\"default\":\"false\"}},{\"def\":\"mimetype\",\"assoc\":{"
      "\"mimetype\":\"MT:image/x.jb%s\",\"extensions\":[\"jb%s\"]}}";
  // char* out = malloc(strlen(reg) + strlen(app_name) * 5 + strlen(app_command)
  // + 1); sprintf(out, reg, app_name, app_command, app_name, app_name,
  // app_name, app_name);
  FILE *f = fopen("/tmp/tmp.register", "w");
  fprintf(f, reg, app_name, app_command, app_name, app_name, app_name,
          app_name);
  fclose(f);
  // free(out);
  //
  // Let the register binary handle the heavy lifting

  system("/usr/bin/register /tmp/tmp.register");

  // First generate an UUID for the entry
  FILE *uuidgen = popen("/bin/uuidgen", "r");
  char *uuid = malloc(37);
  fgets(uuid, 37, uuidgen);
  pclose(uuidgen);
  uuid[36] = '\0';

  // Insert the app into the CC database because no indexer will pick it up
  sqlite3 *db;
  sqlite3_open("/var/local/cc.db", &db);
  sqlite3_create_collation(db, "icu", 4, NULL, icuCompare);
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(
      db,
      "INSERT INTO Entries "
      "VALUES(?,'Entry:Item',?,1698438434,1698398406,0,?,?,?,?,1,'','[]',0,'[]'"
      ",0,'[]',0,NULL,NULL,NULL,NULL,NULL,1,1,NULL,0,0,NULL,'[]',0,'"
      "application/"
      "octet-stream',NULL,NULL,0,NULL,?,'PDOC',NULL,NULL,'[{\"ref\":\"titles\"}"
      "]',?,NULL,NULL,NULL,0,NULL,0,2147483647,NULL,0,NULL,NULL,NULL,0,NULL,0,?"
      ",0,NULL,0,0,0,NULL,NULL,1,0.0,1,NULL,1,0);",
      -1, &stmt, NULL);

  // 0 - uuid
  // 1 - /mnt/us/documents/app.jbapp
  // 2, 3, 4 - app
  // 5 -
  // [{"display":"app","collation":"app","language":"en","pronunciation":"app"}]
  // 6 - cdekey *0d6a8cd1614fb6462232083d94f9788a0e1be37d
  // 7 - ["App"]
  // 8 - 'App￼App￼'
  sqlite3_bind_text(stmt, 1, uuid, -1, SQLITE_STATIC);
  char *app_path = malloc(strlen(app_name) * 2 + 21 + 1);
  sprintf(app_path, "/mnt/us/documents/%s.jb%s", app_name, app_name);
  f = fopen(app_path, "w");
  fprintf(f, "Stub file for launching %s.", app_name);
  fclose(f);

  sqlite3_bind_text(stmt, 2, app_path, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, app_name, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, app_name, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, app_name, -1, SQLITE_STATIC);
  char *app_display = malloc(strlen(app_name) * 3 + 66 + 1);
  sprintf(app_display,
          "[{\"display\":\"%s\",\"collation\":\"%s\",\"language\":\"en\","
          "\"pronunciation\":\"%s\"}]",
          app_name, app_name, app_name);
  sqlite3_bind_text(stmt, 6, app_display, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 7, app_name, -1, SQLITE_STATIC);
  char *app_tags = malloc(strlen(app_name) + 4 + 1);
  sprintf(app_tags, "[\"%s\"]", app_name);
  sqlite3_bind_text(stmt, 8, app_tags, -1, SQLITE_STATIC);
  char *app_title = malloc(strlen(app_name) * 2 + 2 + 1);
  sprintf(app_title, "'%s\xFF%s\xFF'", app_name, app_name);
  sqlite3_bind_text(stmt, 9, app_title, -1, SQLITE_STATIC);
  int step = sqlite3_step(stmt);
  if (step != SQLITE_DONE) {
    printf("Failed to insert app into CC database: %s\n",
           sqlite3_errmsg(db));
  //  return LIPC_ERROR_INTERNAL;

  }
  int sql = sqlite3_finalize(stmt);
  if (sql != SQLITE_OK) {
    printf("Failed to insert app into CC database: %s\n",
           sqlite3_errmsg(db));
    return LIPC_ERROR_INTERNAL;
  }
  sqlite3_close(db);
  free(app_path);
  free(app_display);
  free(app_tags);
  free(app_title);
  free(uuid);
  return LIPC_OK;
}

int main() {
  LIPCcode opened;
  LIPC *handle = LipcOpenEx("com.notmarek.jb", &opened);
  if (opened != LIPC_OK) {
    printf("Failed to open LIPC\n");
    return 1;
  }
  register_string_handler(handle, "exit", NULL, lipc_string_setter_exit);
  register_string_handler(handle, "runCmd", run_cmd_getter, run_cmd_setter);
  // Switch to hasharray instead of splitting string by ";"
  register_string_handler(handle, "registerApp", NULL, register_app);
  register_string_handler(handle, "registeredApps",
                          NULL /* TODO: Get apps registered with the util */,
                          NULL);
  register_string_handler(handle, "deregisterApp", NULL,
                          NULL /* TODO: deregister app logic */);

  printf("JBUtil ready!\n");
  while (1) {
    sleep(1);
  }
  return 0;
}
