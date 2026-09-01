#define BASE_IMPLEMENTATION
#include "common/api.h"

#include "host/reload.cpp"
static Reload r = {};

#include "plateform.cpp"

void init(Application *app) {
    app->scratch = arena_create(ARENA_DEFAULT_BLOCK_SIZE);

    printf("Hello, Host !\n");

    app->platform = {};
    plat_init(&app->platform, &app);
}

void destroy(Application *app) {
    plat_destroy(&app->platform);
    printf("Goodbye, Host !\n");
}

bool update(Application *app) {
    if (plat_should_close(&app->platform)) {
        return false;
    }
    plat_poll_events(&app->platform);

    r.api.update(app);
    return true;
}

int main() {
    Application app = {};
    init(&app);

    r.path = "build/libapp.so";
    r_init(&r, &app);
    for (;;) {
        r_poll(&r, &app);
        if (!update(&app)) break;
    }
    destroy(&app);
    return 0;
}
