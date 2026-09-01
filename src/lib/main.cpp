#include <vulkan/vulkan_core.h>
#define BASE_IMPLEMENTATION
#include "common/api.h"

#include "lib/cloud.cpp"
#include "lib/renderer.cpp"

void lib_load(Application *app) {
    (void)app;
    if (app->renderer.is_init) {
        printf("load not first\n");
        rd_create_pipeline(&app->renderer, app->scratch);
    } else {
        app->renderer = {};
        rd_init(&app->renderer, app->platform.api, app->scratch);
        app->cloud = {};
        cd_init(&app->cloud);
        printf("Hello, Lib !\n");
        app->renderer.is_init = true;
    }
    cd_update(&app->cloud);
    memcpy(app->renderer.point_buffer.mapped_memory, app->cloud.points, app->renderer.point_buffer.count);
}
void lib_unload(Application *app) {
    (void)app;
    printf("Goodbye, Lib !\n");
}
void lib_update(Application *app) {
    rd_update(&app->renderer, app->platform.api, app->scratch);

    u32 image_index = rd_begin_frame(&app->renderer);
    rd_render_cloud(&app->renderer);
    rd_end_frame(&app->renderer, image_index);
}

extern "C" LibAPI lib_get_api(void) {
    return LibAPI{
        .load = lib_load,
        .unload = lib_unload,
        .update = lib_update,
    };
}
