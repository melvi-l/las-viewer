#include "common/api.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

typedef LibAPI (*LibGetApiFn)(void);
typedef struct Reload {
  const char *path;
  void *handle;
  LibAPI api;
  time_t last_mtime;
} Reload;

bool r_load(Reload *r, Application *app) {
  r->handle = dlopen(r->path, RTLD_NOW | RTLD_LOCAL);
  if (r->handle == NULL) {
    fprintf(stderr, "reload: dlopen failed: %s\n", dlerror());
    return false;
  }

  LibGetApiFn lib_get_api = (LibGetApiFn)dlsym(r->handle, "lib_get_api");
  if (lib_get_api == NULL) {
    fprintf(stderr, "reload: lib_get_api missing: %s\n", dlerror());
    dlclose(r->handle);
    r->handle = NULL;
    return false;
  }

  r->api = lib_get_api();
  r->api.load(app);
  return true;
}

bool r_init(Reload *r, Application *app) {
  struct stat st;
  if (stat(r->path, &st) != 0)
    return false;
  r->last_mtime = st.st_mtime;
  return r_load(r, app);
}

bool r_poll(Reload *r, Application *app) {
  printf("poll\n");
  struct stat st;
  if (stat(r->path, &st) != 0)
    return false;
  if (st.st_mtime == r->last_mtime)
    return false;

  printf("change %p\n", (void *)r->api.load);
  r->api.unload(app);
  dlclose(r->handle);
  r->handle = NULL;
  r->last_mtime = st.st_mtime;

  usleep(50000); // 50ms
  printf("reload: detected change, reloading %s\n", r->path);

  return r_load(r, app);
}

int main() {
  printf("Hello, Host !\n");

  Application app = {};

  Reload r = {};
  r.path = "build/libapp.so";

  r_init(&r, &app);

  for (;;) {
    r_poll(&r, &app);
    r.api.update(NULL);
    sleep(1);
  }
  printf("Goodbye, Host !\n");
  return 0;
}
