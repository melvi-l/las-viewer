#ifndef PLATFORM_H
#define PLATFORM_H

#include "common/base.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// @platform-vulkan
typedef struct PlatformApi {
    void *context;

    const char **(*get_required_instance_extensions)(
        void *platform,
        u32 *count);

    Size (*get_framebuffer_size)(void *platform);

    VkResult (*create_surface)(
        void *platform,
        VkInstance instance,
        VkSurfaceKHR *surface);
} PlatformApi;

typedef GLFWwindow PlatWindow;
typedef Size PlatFramebufferSize;
typedef struct {
    PlatWindow *window;
    PlatFramebufferSize framebuffer_size;

    void *user_data;

    PlatformApi api;
} Platform;

#endif // PLATFORM_H
