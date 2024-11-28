#include "openlipc.h"
#include "sqlite3.h"
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
#define APP_PREFIX "com.notmarek.jb"

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
int icuCompare(void *udp, int sizeA, const void *textA, int sizeB,
               const void *textB) {
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
LIPCcode get_registered_apps(struct LipcStringHandler *this, LIPC *lipc,
                             char **value) {
  sqlite3 *db;
  sqlite3_open("/var/local/appreg.db", &db);
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db,
                     "SELECT handlerId, value FROM properties WHERE handlerId "
                     "like '" APP_PREFIX "%s' AND name = 'command';",
                     -1, &stmt, NULL);

  char *final = "appName | handlerId | command\n";

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char *handlerId = sqlite3_column_text(stmt, 0);
    const unsigned char *command = sqlite3_column_text(stmt, 1);
    char *appName = (char *)(handlerId + 15);
    char *line =
        malloc(strlen((char *)handlerId) + strlen((char *)command) + 3);
    sprintf(line, "%s|%s|%s\n", appName, handlerId, command);
    final = realloc(final, strlen(final) + strlen(line) + 1);
    strcat(final, line);
    free(line);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  *value = malloc(strlen(final) + 1);
  strcpy(*value, final);

  return LIPC_OK;
}

LIPCcode deregister_app(struct LipcStringHandler *this, LIPC *lips,
                        char *value) {
  // Remove app from appreg.db
  char *handlerId = malloc(strlen(value) + 15 + 1);
  sprintf(handlerId, APP_PREFIX "%s", value);
  const char *sql = "BEGIN TRANSACTION;"
                    "DELETE FROM extenstions WHERE mimetype IN (SELECT "
                    "contentId FROM associations WHERE handlerId = ?);"
                    "DELETE FROM mimetypes WHERE mimetype IN (SELECT contentId "
                    "FROM associations WHERE handlerId = ?);"
                    "DELETE FROM associations WHERE handlerId = ?;"
                    "DELETE FROM properties WHERE handlerId = ?;"
                    "DELETE FROM handlerIds WHERE handlerId = ?;"
                    "COMMIT;";

  sqlite3 *db;
  int rc = sqlite3_open("/var/local/appreg.db", &db);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return LIPC_ERROR_INTERNAL;
  }

  sqlite3_stmt *stmt;
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Preparation failed: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return LIPC_ERROR_INTERNAL;
  }

  for (int i = 1; i <= 5; i++) {
    sqlite3_bind_text(stmt, i, handlerId, -1, SQLITE_STATIC);
  }

  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(db));
    return LIPC_ERROR_INTERNAL;
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  // Remove app from cc.db
  sqlite3_open("/var/local/cc.db", &db);

  sqlite3_create_collation(db, "icu", 4, NULL, icuCompare);

  sqlite3_prepare_v2(db, "DELETE FROM Entries WHERE cdekey = ?;", -1, &stmt,
                     NULL);
  sqlite3_bind_text(stmt, 1, value, -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  sqlite3_close(db);

  free(handlerId);
  return LIPC_OK;
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
      "\"def\":\"handlerId\",\"assoc\":{\"handlerId\":\"" APP_PREFIX "%s\","
      "\"props\":{\"command\":\"%s\"}}},{\"def\":\"association\",\"assoc\":{"
      "\"interface\":\"application\",\"handlerId\":\"" APP_PREFIX "%s\","
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
    printf("Failed to insert app into CC database: %s\n", sqlite3_errmsg(db));
    //  return LIPC_ERROR_INTERNAL;
  }
  int sql = sqlite3_finalize(stmt);
  if (sql != SQLITE_OK) {
    printf("Failed to insert app into CC database: %s\n", sqlite3_errmsg(db));
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

static void skeleton_daemon() {
  // taken from https://stackoverflow.com/a/17955149
  pid_t pid;

  /* Fork off the parent process */
  pid = fork();

  /* An error occurred */
  if (pid < 0)
    exit(EXIT_FAILURE);

  /* Success: Let the parent terminate */
  if (pid > 0)
    exit(EXIT_SUCCESS);

  /* On success: The child process becomes session leader */
  if (setsid() < 0)
    exit(EXIT_FAILURE);

  /* Catch, ignore and handle signals */
  // TODO: Implement a working signal handler */
  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  /* Fork off for the second time*/
  pid = fork();

  /* An error occurred */
  if (pid < 0)
    exit(EXIT_FAILURE);

  /* Success: Let the parent terminate */
  if (pid > 0)
    exit(EXIT_SUCCESS);

  /* Set new file permissions */
  umask(0);

  /* Change the working directory to the root directory */
  /* or another appropriated directory */
  chdir("/");

  /* Close all open file descriptors */
  int x;
  for (x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
    close(x);
  }
}

// Global variables for signal handling
static volatile sig_atomic_t keep_running = 1;
static LIPC *global_handle = NULL;

void handle_signal(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    printf("\nReceiving shutdown signal. Cleaning up...\n");
    keep_running = 0;

    // Close LIPC if it's open
    if (global_handle) {
      LipcClose(global_handle);
      global_handle = NULL;
    }
    exit(0);
  }
}

LIPCcode lipc_string_setter_exit(struct LipcStringHandler *this, LIPC *lipc,
                                 char *value) {
  printf("Closing.\n");
  handle_signal(SIGINT);
  return LIPC_OK;
}

int main(int argc, char *argv[]) {
  bool run_as_daemon = true;

  // Parse command-line options
  int opt;
  static struct option long_options[] = {{"no-daemon", no_argument, 0, 'n'},
                                         {0, 0, 0, 0}};

  while ((opt = getopt_long(argc, argv, "n", long_options, NULL)) != -1) {
    switch (opt) {
    case 'n':
      run_as_daemon = false;
      break;
    default:
      fprintf(stderr, "Usage: %s [--no-daemon|-n]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  // Daemonize only if requested
  if (run_as_daemon) {
    printf("Forking into the background.\n");
    skeleton_daemon();
  } else {
    printf("Running in foreground mode.\n");

    // Setup signal handling for foreground mode
    if (signal(SIGINT, handle_signal) == SIG_ERR) {
      perror("Unable to set SIGINT handler");
      exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, handle_signal) == SIG_ERR) {
      perror("Unable to set SIGTERM handler");
      exit(EXIT_FAILURE);
    }
  }

  LIPCcode opened;
  LIPC *handle = LipcOpenEx(APP_PREFIX, &opened);
  if (opened != LIPC_OK) {
    printf("Failed to open LIPC\n");
    return 1;
  }

  // Store handle globally for signal handler
  global_handle = handle;

  register_string_handler(handle, "exit", NULL, lipc_string_setter_exit);
  register_string_handler(handle, "runCmd", run_cmd_getter, run_cmd_setter);
  register_string_handler(handle, "registerApp", NULL, register_app);
  register_string_handler(handle, "registeredApps", get_registered_apps, NULL);
  register_string_handler(handle, "deregisterApp", NULL, deregister_app);

  printf("JBUtil ready!\n");

  // Main loop with graceful shutdown
  while (keep_running) {
    sleep(1);
  }

  // Cleanup
  LipcClose(handle);
  printf("JBUtil shutting down.\n");

  return 0;
}
