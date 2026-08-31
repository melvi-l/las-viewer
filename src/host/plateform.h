#ifndef PLATFORM_H
#define PLATFORM_H

#include "common/base.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef void (*PlatResizeCallback)(u32 width, u32 height, void *user_data);
typedef void (*PlatCharCallback)(u32 c, void *user_data);
typedef void (*PlatKeyCallback)(i32 key, i32 scancode, i32 action, i32 mods, void *user_data);

typedef enum {
    PLAT_MOUSE_MOVE,
    PLAT_MOUSE_BUTTON,
    PLAT_MOUSE_SCROLL,
    PLAT_MOUSE_ENTER,
} PlatMouseKind;

typedef struct {
    PlatMouseKind kind;
    union {
        struct {
            f64 x, y;
        } move;
        struct {
            i32 button, action, mods;
        } button;
        struct {
            f64 xoff, yoff;
        } scroll;
        struct {
            bool entered;
        } enter;
    } u;
} PlatMouseEvent;

typedef void (*PlatMouseCallback)(const PlatMouseEvent *ev, void *user_data);

typedef GLFWwindow PlatWindow;
typedef struct {
    PlatWindow *window;

    void *user_data;
} Platform;

// @platform-vulkan
typedef struct PlatformApi {
    Platform *context;

    const char **(*get_required_instance_extensions)(
      void *platform,
      u32 *count);

    void (*get_framebuffer_size)(void *platform, u32 *width, u32 *height);

    VkResult (*create_surface)(
      void *platform,
      VkInstance instance,
      VkSurfaceKHR *surface);
} PlatformApi;

#endif // PLATFORM_H
