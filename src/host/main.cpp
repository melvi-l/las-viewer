#define BASE_IMPLEMENTATION
#include "common/api.h"

#include "host/reload.cpp"
static Reload r = {};

#include "plateform.cpp"
#include "renderer.cpp"

void init(Application *app) {
    app->scratch = arena_create(ARENA_DEFAULT_BLOCK_SIZE);

    printf("Hello, Host !\n");

    app->platform = {};
    plat_init(&app->platform, &app);
    PlatformApi plat_api = plat_get_api(&app->platform);

    app->renderer = {};
    rd_init(&app->renderer);
    rd_create_instance(&app->renderer, plat_api, app->scratch);
    rd_create_surface(&app->renderer, plat_api);
    rd_create_physical_device(&app->renderer, app->scratch);
    rd_create_queue(&app->renderer, app->scratch);
    rd_create_logical_device(&app->renderer);
    rd_create_swapchain(&app->renderer, plat_api, app->scratch);
    rd_create_frame_context(&app->renderer);
    rd_create_pipeline(&app->renderer, app->scratch);
}

void destroy(Application *app) {
    plat_destroy(&app->platform);
    printf("Goodbye, Host !\n");
}

bool update(Application *app) {
    if (plat_should_close(&app->platform)) {
        return false;
    }
    plat_poll_events();

    u32 image_index = rd_begin_frame(&app->renderer);
    rd_render_triangle(&app->renderer);
    rd_end_frame(&app->renderer, image_index);
    r.api.update(NULL);
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
