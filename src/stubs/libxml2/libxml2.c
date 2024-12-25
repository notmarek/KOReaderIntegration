#include "libxml2.h"

xmlChar *xmlGetProp(xmlNode *node, const xmlChar *name) { return 0; }

int xmlStrcmp(const xmlChar *str1, const xmlChar *str2) { return 0; }
xmlDoc *xmlParseDoc(const xmlChar *buffer) { return 0; }
xmlNode *xmlDocGetRootElement(xmlDoc *doc) { return 0; }
void xmlFreeDoc(xmlDoc *doc) { return; }
xmlChar *xmlNodeGetContent(xmlNode *node) { return 0; }
void xmlCleanupParser() { return; }
void xmlFree(void *xml) { return; }
xmlDoc *xmlReadFile(const char *file, const char *encoding, int options) {
  return 0;
}
int xmlAddEncodingAlias(const char *name, const char *alias) { return 0; };
