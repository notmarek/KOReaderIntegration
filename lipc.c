#include "openlipc.h"

void LipcClose(LIPC* lipc0) { return; }
LIPC *LipcOpenEx(const char *service, LIPCcode *code) { *code=0; return 0; };
LIPCcode LipcRegisterStringProperty(LIPC *lipc, const char *property,
                                    LipcPropCallback getter,
                                    LipcPropCallback setter, void *data) {
  return 0;
}
