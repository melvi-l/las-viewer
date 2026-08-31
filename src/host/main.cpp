#define BASE_IMPLEMENTATION
#include "common/api.h"

#include "host/reload.cpp"
static Reload r = {};

#include "vulkan.cpp"

void init(Application *app) {
  app->scratch = arena_create(ARENA_DEFAULT_BLOCK_SIZE);

  printf("Hello, Host !\n");
  r.path = "build/libapp.so", r_init(&r, app);

  rd_init(app);
  rd_create_instance(app);
}

void destroy(Application *app) {
  (void)app;
  printf("Goodbye, Host !\n");
}

void update(Application *app) {
  r_poll(&r, app);
  r.api.update(NULL);
}

int main() {
  Application app = {};
  init(&app);
  for (;;) {
    update(&app);
  }
  destroy(&app);
  return 0;
}
