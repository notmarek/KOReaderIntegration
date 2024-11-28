#include "openlipc.h"
#include <stdio.h>

void LipcClose(LIPC* lipc) { return; }
LIPC *LipcOpenEx(const char *service, LIPCcode *code) {
    *code = LIPC_OK;
    printf("\"Opening\" a LIPC handle for %s\n", service);
    return (LIPC *)1;


};
LIPCcode LipcRegisterStringProperty(LIPC *lipc, const char *property,
                                    LipcPropCallback getter,
                                    LipcPropCallback setter, void *data) {
    printf("Registering string property %s\n", property);
    return LIPC_OK;
}
