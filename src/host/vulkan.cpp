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

static bool rd_create_physical_device(Application *app) {
  Renderer *rd = &app->renderer;

  rd->graphic_queue_index = UINT32_MAX;
  rd->inflight_count = MAX_FRAMES_IN_FLIGHT;

  // @physical device
  VkPhysicalDeviceVulkan11Features vulkan11_features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .shaderDrawParameters = true};
  VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state_features =
      {.sType =
           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
       .pNext = &vulkan11_features};
  VkPhysicalDeviceVulkan13Features vulkan13_features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &extended_dynamic_state_features};
  VkPhysicalDeviceFeatures2 device_features = (VkPhysicalDeviceFeatures2){
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &vulkan13_features,
      .features = {.samplerAnisotropy = true},
  };
  const char *required_device_extension_names[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  const u32 required_device_extension_count =
      sizeof(required_device_extension_names) /
      sizeof(required_device_extension_names[0]);
  {
    ArenaTemp scratch = arena_temp_begin(app->scratch_arena);
    u32 physical_device_count = 0;
    VKTRY(
        vkEnumeratePhysicalDevices(app->instance, &physical_device_count, NULL),
        "Vulkan error: no physical device found");
    VkPhysicalDevice *physical_devices = ARENA_PUSH_ARRAY(
        scratch.arena, physical_device_count, VkPhysicalDevice);

    VKTRY(vkEnumeratePhysicalDevices(app->instance, &physical_device_count,
                                     physical_devices),
          "Vulkan error: failed to enumerate physical devices");
    app->physical_device = physical_devices[0];

    // verify 1.3 support
    VkPhysicalDeviceProperties2 physical_device_properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(app->physical_device,
                                   &physical_device_properties);
    if (physical_device_properties.properties.apiVersion < VK_API_VERSION_1_3) {
      fprintf(stderr, "Vulkan error: physical device do not support 1.3.");
      rd_cleanup(app);
      return false;
    }

    // verify graphic queue
    u32 physical_device_queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        app->physical_device, &physical_device_queue_count, NULL);
    VkQueueFamilyProperties *physical_device_queue_properties =
        ARENA_PUSH_ARRAY(scratch.arena, physical_device_queue_count,
                         VkQueueFamilyProperties);
    vkGetPhysicalDeviceQueueFamilyProperties(app->physical_device,
                                             &physical_device_queue_count,
                                             physical_device_queue_properties);

    // graphic queue index
    VkBool32 supports_surface = false;
    for (u32 i = 0; i < physical_device_queue_count; ++i) {
      VKTRY(vkGetPhysicalDeviceSurfaceSupportKHR(
                app->physical_device, i, app->surface, &supports_surface),
            "Vulkan error: unable to test if physical device");
      if (physical_device_queue_properties[i].queueFlags &
              VK_QUEUE_GRAPHICS_BIT &&
          supports_surface) {
        app->graphic_queue_index = i;
        break;
      }
    }
    if (app->graphic_queue_index == UINT32_MAX) {
      fprintf(stderr,
              "Vulkan error: physical device does not support graphic queue\n");
      rd_cleanup(app);
      return false;
    }

    // transfer queue index
    for (u32 i = 0; i < physical_device_queue_count; ++i) {
      if (i != app->graphic_queue_index &&
          physical_device_queue_properties[i].queueFlags &
              VK_QUEUE_TRANSFER_BIT) {
        app->transfer_queue_index = i;
        break;
      }
    }
    if (app->transfer_queue_index == UINT32_MAX) {
      printf("Unable to find another queue than graphic for transfer.\n");
      app->transfer_queue_index = app->graphic_queue_index;
    }

    // verify swapchain extension
    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(app->physical_device, NULL,
                                         &extension_count, NULL);
    VkExtensionProperties *extension_properties =
        ARENA_PUSH_ARRAY(scratch.arena, extension_count, VkExtensionProperties);
    vkEnumerateDeviceExtensionProperties(
        app->physical_device, NULL, &extension_count, extension_properties);

    bool supports_swapchain = false;
    for (u32 i = 0; i < required_device_extension_count; i++) {
      if (has_extension(extension_count, extension_properties,
                        required_device_extension_names[i])) {
        supports_swapchain = true;
        break;
      }
    }
    if (!supports_swapchain) {
      fprintf(stderr,
              "Vulkan error: physical device does not support swapchain\n");
      rd_cleanup(app);
      return false;
    }

    // verify dynamic rendering feature
    vkGetPhysicalDeviceFeatures2(app->physical_device, &device_features);

    if (!vulkan13_features.dynamicRendering ||
        !extended_dynamic_state_features.extendedDynamicState) {
      fprintf(stderr,
              "Vulkan error: physical device does not dynamic rendering\n");
      rd_cleanup(app);
      return false;
    }

    printf("vk::PhysicalDevice created\n");
    arena_temp_end(scratch);
  }

  // @logical device
  {
    f32 queue_priority = 1.f;
    VkDeviceQueueCreateInfo device_queue_info[2] = {
        {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
         .queueCount = 1,
         .pQueuePriorities = &queue_priority,
         .queueFamilyIndex = app->graphic_queue_index},
        {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
         .queueCount = 1,
         .pQueuePriorities = &queue_priority,
         .queueFamilyIndex = app->transfer_queue_index},
    };
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &device_features,
        .queueCreateInfoCount =
            app->graphic_queue_index == app->transfer_queue_index ? 1 : 2,
        .pQueueCreateInfos = device_queue_info,
        .enabledExtensionCount = required_device_extension_count,
        .ppEnabledExtensionNames = required_device_extension_names};
    VKTRY(
        vkCreateDevice(app->physical_device, &device_info, NULL, &app->device),
        "Vulkan error: failed to create logical device");
    vkGetDeviceQueue(app->device, app->graphic_queue_index, 0,
                     &app->graphic_queue);
    vkGetDeviceQueue(app->device, app->transfer_queue_index, 0,
                     &app->transfer_queue);

    printf("vk::LogicalDevice (and queue handle) created\n");
  }

  // @swapchain
  ArenaTemp swapchain_scratch = arena_temp_begin(app->scratch_arena);
  swapchain_init(swapchain_scratch.arena, app);
  arena_temp_end(swapchain_scratch);
  printf("vk::Swapchain (and images) created\n");
  printf("vk::ImageView (for swapchain) created\n");

  // @descriptor set layout
  {
    VkDescriptorSetLayoutBinding bindings[2] = {
        {.binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT},
        {.binding = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}};
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings};
    VKTRY(vkCreateDescriptorSetLayout(app->device, &layout_info, NULL,
                                      &app->descriptor_set_layout),
          "Vulkan error: Failed to create ubo descriptor set layout");
  }

  // @depth
  depth_buffer_init(app);

  // @render pipeline

  // @command pool & buffer
  {
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = app->graphic_queue_index};
    VKTRY(vkCreateCommandPool(app->device, &pool_info, NULL,
                              &app->graphic_command_pool),
          "Vulkan error: Failed to create graphic command pool");

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->graphic_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = app->inflight_count};

    app->graphic_command_buffers = ARENA_PUSH_ARRAY(
        app->vulkan_arena, app->inflight_count, VkCommandBuffer);
    VKTRY(vkAllocateCommandBuffers(app->device, &alloc_info,
                                   app->graphic_command_buffers),
          "Vulkan error: Failed to allocate graphic command buffers");

    VkCommandPoolCreateInfo transfer_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = app->transfer_queue_index};
    VKTRY(vkCreateCommandPool(app->device, &transfer_pool_info, NULL,
                              &app->transfer_command_pool),
          "Vulkan error: Failed to create transfer command pool");

    VkCommandBufferAllocateInfo transfer_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->transfer_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1};
    VKTRY(vkAllocateCommandBuffers(app->device, &transfer_alloc_info,
                                   &app->transfer_command_buffer),
          "Vulkan error: Failed to allocate transfer command buffer");
  }

  // @buffer (vertex & index & instance)
  {
    // device local
    ArenaTemp temp = arena_temp_begin(app->scratch_arena);
    VkDeviceSize vertex_size = sizeof(vertices[0]) * vertices_count;
    VkDeviceSize index_size = sizeof(indices[0]) * indices_count;
    VkDeviceSize geometry_size = vertex_size + index_size;

    app->vertex_offset = 0;
    app->index_offset = vertex_size;

    u8 *geometry_array = ARENA_PUSH_ARRAY(temp.arena, geometry_size, u8);
    memcpy(geometry_array + app->vertex_offset, vertices, vertex_size);
    memcpy(geometry_array + app->index_offset, indices, index_size);

    if (!upload_device_local_array(app, geometry_array, geometry_size,
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                   &app->geometry_buffer,
                                   &app->geometry_memory)) {
      return false;
    }

    // host visible + keep map memory
    VkDeviceSize instance_size =
        sizeof(app->quad_list.data[0]) * app->quad_list.capacity;
    app->instance_mapped_array =
        ARENA_PUSH_ARRAY(app->vulkan_arena, instance_size, u8);

    if (!create_buffer(app, instance_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       VK_SHARING_MODE_EXCLUSIVE, 0, 0, &app->instance_buffer,
                       &app->instance_memory)) {
      return false;
    }

    // TODO(melvil): maybe create a x2 buffer to swap without race condition
    VKTRY(vkMapMemory(app->device, app->instance_memory, 0, instance_size, 0,
                      &app->instance_mapped_array),
          "Vulkan error: Failed to map memory for vertex buffer");
    arena_temp_end(temp);
  }

  // Descriptor

  // @buffer (uniform)
  {
    VkDeviceSize uniform_size = sizeof(UniformBufferObject);

    app->uniform_buffers =
        ARENA_PUSH_ARRAY(app->vulkan_arena, app->inflight_count, VkBuffer);
    app->uniform_memories = ARENA_PUSH_ARRAY(
        app->vulkan_arena, app->inflight_count, VkDeviceMemory);
    app->uniform_mapped_arrays =
        ARENA_PUSH_ARRAY(app->vulkan_arena, app->inflight_count, void *);

    for (u32 i = 0; i < app->inflight_count; i++) {
      create_buffer(app, uniform_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
                    &app->uniform_buffers[i], &app->uniform_memories[i]);
      VKTRY(vkMapMemory(app->device, app->uniform_memories[i], 0, uniform_size,
                        0, &app->uniform_mapped_arrays[i]),
            "Vulkan error: Failed to map memory for uniform buffer");
    }
  }

  // @sync object
  {
    app->image_available_semas =
        ARENA_PUSH_ARRAY(app->vulkan_arena, app->inflight_count, VkSemaphore);
    app->draw_fences =
        ARENA_PUSH_ARRAY(app->vulkan_arena, app->inflight_count, VkFence);
    for (u32 i = 0; i < app->inflight_count; i++) {
      VKTRY(vkCreateSemaphore(
                app->device,
                &(VkSemaphoreCreateInfo){
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                },
                NULL, &app->image_available_semas[i]),
            "Vulkan error: Failed to create present semaphore");
      VKTRY(vkCreateFence(app->device,
                          &(VkFenceCreateInfo){
                              .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                              .flags = VK_FENCE_CREATE_SIGNALED_BIT},
                          NULL, &app->draw_fences[i]),
            "Vulkan error: Failed to create draw fence");
    }
    app->render_finish_semas = ARENA_PUSH_ARRAY(
        app->vulkan_arena, app->swapchain_images_count, VkSemaphore);
    for (u32 i = 0; i < app->swapchain_images_count; i++) {
      VKTRY(vkCreateSemaphore(
                app->device,
                &(VkSemaphoreCreateInfo){
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                },
                NULL, &app->render_finish_semas[i]),
            "Vulkan error: Failed to create a render semaphore");
    }
  }

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
