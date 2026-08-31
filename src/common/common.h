#ifndef COMMON_H
#define COMMON_H

#define BASE_IMPLEMENTATION
#include "base.h"

#include <vulkan/vulkan_core.h>

#include <GLFW/glfw3.h>

typedef struct Renderer {
  Arena *arena;
  VkInstance instance;
  VkPhysicalDevice physical_device;
} Renderer;

typedef GLFWwindow PlatWindow;
typedef struct Platform {
  PlatWindow *window;
} Platform;

typedef struct Application {
  Arena *scratch;
  Renderer renderer;
  Platform platform;
} Application;

#endif
