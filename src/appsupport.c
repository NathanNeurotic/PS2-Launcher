#include "opl.h"
#include "include/appsupport.h"

int oplPath2Mode(const char *path);

void appInit(item_list_t *itemList)
{
    oplPath2Mode(NULL);
}
item_list_t *appGetObject(int initOnly) { return NULL; }
void appPostUpdateCallback(int mode) {}
