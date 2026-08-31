#include "common/base.h"
#include "common/common.h"

#define VKTRY(expr, msg)                                                       \
  do {                                                                         \
    VkResult vk_result__ = (expr);                                             \
    if (vk_result__ != VK_SUCCESS) {                                           \
      fprintf(stderr, "%s: VkResult=%d\n", msg, vk_result__);                  \
      return -1;                                                               \
    }                                                                          \
  } while (0)

#ifdef VK_ENABLE_VALIDATION
static const bool is_validation_enabled = 1;
char const *validation_layer_names[] = {"VK_LAYER_KHRONOS_validation"};
const u32 validation_layer_count =
    sizeof(validation_layer_names) / sizeof(validation_layer_names[0]);
#else
static const bool is_validation_enabled = 0;
char const *validation_layer_names[0] = {};
const u32 validation_layer_count = 0;
#endif

static bool rd_init(Application *app);
static bool rd_create_instance(Application *app);
static void rd_cleanup(Application *app);
// static bool rd_resize(Application *app);

bool get_extensions(Arena *arena, u32 *extension_count,
                    const char ***extension_names);
bool has_layer(u32 actual_count, VkLayerProperties *actual_props,
               const char *const expected);
bool get_layers(Arena *arena, u32 *layer_count, const char ***layer_names);

static bool rd_init(Application *app) {
  app->renderer = {.arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE)};
  return true;
}

static bool rd_create_instance(Application *app) {
  Renderer *rd = &app->renderer;
  ArenaTemp scratch = arena_temp_begin(app->scratch);
  u32 extension_count;
  const char **extension_names = NULL;
  if (!get_extensions(scratch.arena, &extension_count, &extension_names)) {
    fprintf(stderr, "Vulkan error: failed to resolve instance extensions\n");
    rd_cleanup(app);
    return false;
  }

  u32 layer_count;
  const char **layer_names = NULL;
  if (!get_layers(scratch.arena, &layer_count, &layer_names)) {
    fprintf(stderr, "Vulkan error: failed to resolve validation layers\n");
    rd_cleanup(app);
    return false;
  }
  const VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Hello Triangle",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_3};

  const VkInstanceCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = layer_count,
      .ppEnabledLayerNames = layer_names,
      .enabledExtensionCount = extension_count,
      .ppEnabledExtensionNames = extension_names,
  };

  VKTRY(vkCreateInstance(&createInfo, NULL, &rd->instance),
        "Vulkan error: failed to create instance");

  printf("vk::Instance created\n");
  arena_temp_end(scratch);
  return true;
}

static void rd_cleanup(Application *app) {
  Renderer rd = app->renderer;
  //  if (app->device != VK_NULL_HANDLE)
  //    vkDeviceWaitIdle(app->device);
  //
  //  if (app->image_available_semas != NULL) {
  //    for (u32 i = 0; i < app->inflight_count; i++) {
  //      vkDestroySemaphore(app->device, app->image_available_semas[i], NULL);
  //    }
  //  }
  //  if (app->render_finish_semas != NULL) {
  //    for (u32 i = 0; i < app->swapchain_images_count; i++) {
  //      vkDestroySemaphore(app->device, app->render_finish_semas[i], NULL);
  //    }
  //  }
  //  if (app->draw_fences != NULL) {
  //    for (u32 i = 0; i < app->inflight_count; i++) {
  //      vkDestroyFence(app->device, app->draw_fences[i], NULL);
  //    }
  //  }
  //
  //  if (app->geometry_buffer != VK_NULL_HANDLE)
  //    vkDestroyBuffer(app->device, app->geometry_buffer, NULL);
  //  if (app->geometry_memory != VK_NULL_HANDLE)
  //    vkFreeMemory(app->device, app->geometry_memory, NULL);
  //
  //  if (app->instance_mapped_array != NULL) {
  //    vkUnmapMemory(app->device, app->instance_memory);
  //    app->instance_mapped_array = NULL;
  //  }
  //  if (app->instance_buffer != VK_NULL_HANDLE)
  //    vkDestroyBuffer(app->device, app->instance_buffer, NULL);
  //  if (app->instance_memory != VK_NULL_HANDLE)
  //    vkFreeMemory(app->device, app->instance_memory, NULL);
  //
  //  if (app->texture_image != VK_NULL_HANDLE)
  //    vkDestroyImage(app->device, app->texture_image, NULL);
  //  if (app->texture_memory != VK_NULL_HANDLE)
  //    vkFreeMemory(app->device, app->texture_memory, NULL);
  //  if (app->texture_view != VK_NULL_HANDLE)
  //    vkDestroyImageView(app->device, app->texture_view, NULL);
  //  if (app->texture_sampler != VK_NULL_HANDLE)
  //    vkDestroySampler(app->device, app->texture_sampler, NULL);
  //
  //  if (app->uniform_mapped_arrays != NULL) {
  //    for (u32 i = 0; i < app->inflight_count; i++)
  //      vkUnmapMemory(app->device, app->uniform_memories[i]);
  //    app->uniform_mapped_arrays = NULL;
  //  }
  //  if (app->uniform_buffers != NULL)
  //    for (u32 i = 0; i < app->inflight_count; i++)
  //      vkDestroyBuffer(app->device, app->uniform_buffers[i], NULL);
  //  if (app->uniform_memories != NULL)
  //    for (u32 i = 0; i < app->inflight_count; i++)
  //      vkFreeMemory(app->device, app->uniform_memories[i], NULL);
  //
  //  if (app->graphic_command_buffers != NULL)
  //    vkFreeCommandBuffers(app->device, app->graphic_command_pool,
  //                         app->inflight_count, app->graphic_command_buffers);
  //
  //  if (app->graphic_command_pool != VK_NULL_HANDLE)
  //    vkDestroyCommandPool(app->device, app->graphic_command_pool, NULL);
  //  if (app->transfer_command_buffer != NULL)
  //    vkFreeCommandBuffers(app->device, app->transfer_command_pool, 1,
  //                         &app->transfer_command_buffer);
  //  if (app->transfer_command_pool != VK_NULL_HANDLE)
  //    vkDestroyCommandPool(app->device, app->transfer_command_pool, NULL);
  //
  //  if (app->pipeline_layout != VK_NULL_HANDLE)
  //    vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);
  //  if (app->pipeline != VK_NULL_HANDLE)
  //    vkDestroyPipeline(app->device, app->pipeline, NULL);
  //
  //  if (app->descriptor_sets != NULL)
  //    vkFreeDescriptorSets(app->device, app->descriptor_pool,
  //    app->inflight_count,
  //                         app->descriptor_sets);
  //  if (app->descriptor_pool != VK_NULL_HANDLE)
  //    vkDestroyDescriptorPool(app->device, app->descriptor_pool, NULL);
  //  if (app->descriptor_set_layout != VK_NULL_HANDLE)
  //    vkDestroyDescriptorSetLayout(app->device, app->descriptor_set_layout,
  //    NULL);
  //
  //  swapchain_cleanup(app);
  //  depth_buffer_cleanup(app);
  //
  //  if (app->device != VK_NULL_HANDLE)
  //    vkDestroyDevice(app->device, NULL);
  //
  //  if (app->surface != VK_NULL_HANDLE) {
  //    vkDestroySurfaceKHR(app->instance, app->surface, NULL);
  //  }

  if (rd.instance != VK_NULL_HANDLE)
    vkDestroyInstance(rd.instance, NULL);

  arena_destroy(rd.arena);
}

// @utils
bool has_extension(u32 actual_count, VkExtensionProperties *actual_props,
                   const char *const expected) {
  for (u32 j = 0; j < actual_count; ++j) {
    if (strcmp(expected, actual_props[j].extensionName) == 0) {
      printf("extension %s found.\n", expected);
      return true;
    }
  }
  fprintf(stderr, "Required extension not supported: %s\n", expected);
  return false;
}
bool get_extensions(Arena *arena, u32 *extension_count,
                    const char ***extension_names) {
  // actual ext of vk
  u32 rd_available_extension_count = 0;

  vkEnumerateInstanceExtensionProperties(NULL, &rd_available_extension_count,
                                         NULL);
  VkExtensionProperties *rd_available_extension_properties = ARENA_PUSH_ARRAY(
      arena, rd_available_extension_count, VkExtensionProperties);
  vkEnumerateInstanceExtensionProperties(NULL, &rd_available_extension_count,
                                         rd_available_extension_properties);

  // expected ext by glfw
  u32 glfw_extension_count = 0; // TODO change
  const char **glfw_extension_names =
      glfwGetRequiredInstanceExtensions(&glfw_extension_count);
  if (glfw_extension_names == NULL) {
    fprintf(stderr, "Failed to get GLFW required extensions\n");
    return false;
  }
  for (u32 i = 0; i < glfw_extension_count; ++i) {
    if (!has_extension(rd_available_extension_count,
                       rd_available_extension_properties,
                       (glfw_extension_names)[i])) {
      return false;
    }
  }

  // extra
  int extra_extension_count = 0;
  if (is_validation_enabled) {
    extra_extension_count++;
    if (!has_extension(rd_available_extension_count,
                       rd_available_extension_properties,
                       VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
      return false;
    }
  }

  *extension_count = glfw_extension_count + (u32)extra_extension_count;
  *extension_names = ARENA_PUSH_ARRAY(arena, *extension_count, const char *);
  memcpy(*extension_names, glfw_extension_names,
         sizeof(*glfw_extension_names) * glfw_extension_count);
  if (is_validation_enabled) {
    (*extension_names)[glfw_extension_count] =
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  }

  return true;
}

bool has_layer(u32 actual_count, VkLayerProperties *actual_props,
               const char *const expected) {
  for (u32 i = 0; i < actual_count; i++) {
    if (strcmp(actual_props[i].layerName, expected) == 0) {
      printf("layer %s found.\n", expected);
      return true;
    }
  }
  fprintf(stderr, "Required layer do not exist: %s\n", expected);
  return false;
}
bool get_layers(Arena *arena, u32 *layer_count, const char ***layer_names) {
  u32 required_layer_count = 0;
  const char **required_layer_names = NULL;
  if (is_validation_enabled) {
    required_layer_count = validation_layer_count;
    required_layer_names =
        ARENA_PUSH_ARRAY(arena, required_layer_count, const char *);

    memcpy(required_layer_names, validation_layer_names,
           sizeof(validation_layer_names) * validation_layer_count);
  }

  u32 rd_layer_count;
  vkEnumerateInstanceLayerProperties(&rd_layer_count, NULL);
  VkLayerProperties *rd_layer_properties =
      ARENA_PUSH_ARRAY(arena, rd_layer_count, VkLayerProperties);
  vkEnumerateInstanceLayerProperties(&rd_layer_count, rd_layer_properties);

  for (u32 i = 0; i < required_layer_count; i++) {
    if (!has_layer(rd_layer_count, rd_layer_properties,
                   required_layer_names[i])) {
      return false;
    }
  }
  *layer_count = required_layer_count;
  *layer_names = required_layer_names;

  return true;
}
