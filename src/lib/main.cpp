#include "common/api.h"
#include <cstdio>

void lib_load(Application *app) {
  (void)app;
  printf("Hello, Lib !\n");
}
void lib_unload(Application *app) {
  (void)app;
  printf("Goodbye, Lib !\n");
}
void lib_update(Application *app) {
  (void)app;
  printf("Update Lib !\n");
}

extern "C" LibAPI lib_get_api(void) {
  return LibAPI{
      .load = lib_load,
      .unload = lib_unload,
      .update = lib_update,
  };
}
