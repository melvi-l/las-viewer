#include "plateform.h"

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

static Size plat_get_framebuffer_size(void *context) {
    Platform *platform = (Platform *)context;
    return {.width = (u32)platform->framebuffer_size.width, .height = (u32)platform->framebuffer_size.height};
}

void plat_update_framebuffer_size(Platform *plat) {
    int w, h;
    glfwGetFramebufferSize(plat->window, &w, &h);
    plat->framebuffer_size.width = (u32)w;
    plat->framebuffer_size.height = (u32)h;
}

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
    plat_update_framebuffer_size(plat);

    plat->api = {
        .context = plat,
        .get_required_instance_extensions = platform_get_required_instance_extensions,
        .get_framebuffer_size = plat_get_framebuffer_size,
        .create_surface = platform_create_surface,
    };

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

void plat_poll_events(Platform *plat) {
    glfwPollEvents();
    plat_update_framebuffer_size(plat);
}
