#include "plateform.h"

// refresh framebuffer size + content scale (physical per logical).
// call after window creation and on every resize.
// static void plat_update_size_scale(Platform *plat) {
//    int fw, fh, ww, wh;
//    glfwGetFramebufferSize(plat->window, &fw, &fh);
//    glfwGetWindowSize(plat->window, &ww, &wh);
//    // plat->fb_w = (u32)fw;
//    // plat->fb_h = (u32)fh;
//    // plat->scale_x = (ww > 0) ? (f32)fw / (f32)ww : 1.0f;
//    // plat->scale_y = (wh > 0) ? (f32)fh / (f32)wh : 1.0f;
//}

// void plat_resize(GLFWwindow *window, int width, int height) {
//     Platform *plat = glfwGetWindowUserPointer(window);
//     assert(plat != NULL);
//
//     (void)width;
//     (void)height; // framebuffer size; recompute from glfw instead
//
//     plat_update_size_scale(plat);
//
//     // callback receives LOGICAL (window) size — app works in logical units.
//     int ww, wh;
//     glfwGetWindowSize(window, &ww, &wh);
//     printf("[GLFW] resize: (fb=%ux%u; win=%dx%d; scale=%.3f,%.3f)\n",
//            plat->fb_w,
//            plat->fb_h,
//            ww,
//            wh,
//            (double)plat->scale_x,
//            (double)plat->scale_y);
//
//     if (plat->resize_callback != NULL) {
//         plat->resize_callback((u32)ww, (u32)wh, plat->user_data);
//     }
// }

bool plat_init(Platform *plat, void *user_data) {
    assert(plat != NULL);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    plat->window = glfwCreateWindow(1000, 1000, "Vulkan", NULL, NULL);
    assert(plat->window);

    plat->user_data = user_data;
    glfwSetWindowUserPointer(plat->window, plat);
    // plat_update_size_scale(plat);

    printf("[GLFW]::Window\n");
    return true;
}

void plat_destroy(Platform *plat) {
    if (plat->window != NULL) {
        glfwDestroyWindow(plat->window);
        plat->window = NULL;
    }
    glfwTerminate();
}

bool plat_should_close(Platform *plat) {
    return glfwWindowShouldClose(plat->window);
}

void plat_poll_events() {
    glfwPollEvents();
}

// @plateform_vulkan
static const char **platform_get_required_instance_extensions(void *context, u32 *count) {
    (void)context;
    // Platform *platform = (Platform *)context;
    return glfwGetRequiredInstanceExtensions(count);
}

static VkResult platform_create_surface(void *context, VkInstance instance, VkSurfaceKHR *surface) {
    assert(instance != NULL);
    Platform *platform = (Platform *)context;
    printf("[GLFW]::VkSurface\n");
    return glfwCreateWindowSurface(
      instance,
      platform->window,
      nullptr,
      surface);
}

static void plat_get_framebuffer_size(void *context, u32 *width, u32 *height) {
    Platform *platform = (Platform *)context;
    int w, h;
    glfwGetFramebufferSize(platform->window, &w, &h);
    *width = (u32)w;
    *height = (u32)h;
}

static PlatformApi plat_get_api(Platform *plat) {
    return {
      .context = plat,
      .get_required_instance_extensions = platform_get_required_instance_extensions,
      .get_framebuffer_size = plat_get_framebuffer_size,
      .create_surface = platform_create_surface,
    };
}
