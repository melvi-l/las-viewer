#include "common/common.h"
#include <cstdio>
#include <vulkan/vulkan_core.h>

#define MAX_FRAMES_IN_FLIGHT 2

#define VKTRY(expr, msg)                                            \
    do {                                                            \
        VkResult vk_result__ = (expr);                              \
        if (vk_result__ != VK_SUCCESS) {                            \
            fprintf(stderr, "%s: VkResult=%d\n", msg, vk_result__); \
            return false;                                           \
        }                                                           \
    } while (0)

#define VK_ARENA_ENUMERATE_1(arena, T, fn, arg)        \
    ({                                                 \
        u32 count = 0;                                 \
        fn((arg), &count, NULL);                       \
        T *data = ARENA_PUSH_ARRAY((arena), count, T); \
        fn((arg), &count, data);                       \
        (struct { T *data; u32 count; }){data, count};                     \
    })

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

// static void rd_destroy(Renderer *rd);

bool has_extension(u32 actual_count, VkExtensionProperties *actual_props, const char *const expected);
bool get_extensions(Arena *arena, PlatformApi plat_api, u32 *extension_count, const char ***extension_names);
bool has_layer(u32 actual_count, VkLayerProperties *actual_props, const char *const expected);
bool get_layers(Arena *arena, u32 *layer_count, const char ***layer_names);

static bool rd_init(Renderer *rd) {
    rd->arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE);
    rd->inflight_count = MAX_FRAMES_IN_FLIGHT;
    return true;
}

static bool rd_create_surface(Renderer *rd, PlatformApi plat_api) {
    VKTRY(plat_api.create_surface(plat_api.context, rd->instance, &rd->surface),
          "[VK] surface creation failed");
    return true;
}

static bool rd_create_instance(Renderer *rd, PlatformApi plat_api, Arena *scratch) {
    ArenaTemp temp = arena_temp_begin(scratch);
    u32 extension_count;
    const char **extension_names = NULL;

    printf("[VK] instance extension:\n");
    if (!get_extensions(temp.arena, plat_api, &extension_count, &extension_names)) {
        fprintf(stderr, "[VK] failed to resolve instance extensions\n");
        return false;
    }

    u32 layer_count;
    const char **layer_names = NULL;
    printf("[VK] instance layers:\n");
    if (!get_layers(temp.arena, &layer_count, &layer_names)) {
        fprintf(stderr, "[VK] failed to resolve validation layers\n");
        return false;
    }
    const VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Point cloud viewer",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "rocamadour",
      .engineVersion = VK_MAKE_VERSION(0, 0, 1),
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
          "[VK] failed to create instance");

    printf("[VK]::Instance\n");
    arena_temp_end(temp);
    return true;
}

VkPhysicalDeviceVulkan11Features vulkan11_features = {
  .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
  .shaderDrawParameters = true};
VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state_features = {
  .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
  .pNext = &vulkan11_features};
VkPhysicalDeviceVulkan13Features vulkan13_features = {
  .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
  .pNext = &extended_dynamic_state_features};
VkPhysicalDeviceFeatures2 device_features = {
  .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
  .pNext = &vulkan13_features,
  .features = {.samplerAnisotropy = true},
};

const char *required_device_extension_names[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
const u32 required_device_extension_count = sizeof(required_device_extension_names) /
                                            sizeof(required_device_extension_names[0]);

static bool rd_create_physical_device(Renderer *rd, Arena *scratch) {
    ArenaTemp temp = arena_temp_begin(scratch);

    u32 physical_device_count = 0;
    VKTRY(vkEnumeratePhysicalDevices(rd->instance, &physical_device_count, NULL),
          "[VK] failed to enumerate physical devices.");
    VkPhysicalDevice *physical_devices = ARENA_PUSH_ARRAY(temp.arena, physical_device_count, VkPhysicalDevice);

    VKTRY(vkEnumeratePhysicalDevices(rd->instance, &physical_device_count, physical_devices),
          "[VK] failed to retrieve physical devices");
    rd->physical_device = physical_devices[0];

    // verify 1.3 support
    VkPhysicalDeviceProperties2 physical_device_properties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(rd->physical_device, &physical_device_properties);
    if (physical_device_properties.properties.apiVersion < VK_API_VERSION_1_3) {
        fprintf(stderr, "[VK] physical device do not support 1.3.");
        return false;
    }

    // verify swapchain extension
    u32 extension_count = 0;
    VKTRY(vkEnumerateDeviceExtensionProperties(rd->physical_device, NULL, &extension_count, NULL),
          "[VK] failed to enumrate device extension");
    VkExtensionProperties *extension_properties = ARENA_PUSH_ARRAY(temp.arena, extension_count, VkExtensionProperties);
    vkEnumerateDeviceExtensionProperties(rd->physical_device, NULL, &extension_count, extension_properties);

    bool supports_swapchain = false;
    for (u32 i = 0; i < required_device_extension_count; i++) {
        printf("[VK] swapchain extension:\n");
        if (has_extension(extension_count, extension_properties, required_device_extension_names[i])) {
            supports_swapchain = true;
            break;
        }
    }
    if (!supports_swapchain) {
        fprintf(stderr, "[VK] physical device does not support swapchain\n");
        return false;
    }

    // verify dynamic rendering feature
    vkGetPhysicalDeviceFeatures2(rd->physical_device, &device_features);
    if (!vulkan13_features.dynamicRendering ||
        !extended_dynamic_state_features.extendedDynamicState) {
        fprintf(stderr, "[VK] physical device does not dynamic rendering\n");
        return false;
    }

    printf("[VK]::PhysicalDevice\n");

    arena_temp_end(temp);
    return true;
}

static bool rd_create_queue(Renderer *rd, Arena *scratch) {
    ArenaTemp temp = arena_temp_begin(scratch);

    bool found_graphic = false;
    RdQueue graphic_queue = {
      .handle = VK_NULL_HANDLE,
      .family_index = UINT32_MAX,
    };
    bool found_present = false;
    RdQueue present_queue = {
      .handle = VK_NULL_HANDLE,
      .family_index = UINT32_MAX,
    };

    u32 family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(rd->physical_device, &family_count, NULL);
    VkQueueFamilyProperties *families = ARENA_PUSH_ARRAY(temp.arena, family_count, VkQueueFamilyProperties);
    vkGetPhysicalDeviceQueueFamilyProperties(rd->physical_device, &family_count, families);

    // graphic queue index
    VkBool32 can_present = VK_FALSE;
    for (u32 i = 0; i < family_count; ++i) {
        VkQueueFamilyProperties *family = &families[i];
        VkResult present_result = vkGetPhysicalDeviceSurfaceSupportKHR(rd->physical_device, i, rd->surface, &can_present);
        if (present_result != VK_SUCCESS) {
            fprintf(stderr, "[VK] Unable to verify if family %i can present on surface.", i);
            continue;
        }
        bool supports_graphic = family->queueCount != 0 && family->queueFlags & VK_QUEUE_GRAPHICS_BIT;
        if (supports_graphic && !found_graphic) {
            graphic_queue.family_index = i;
            found_graphic = true;
        }
        if (can_present && !found_present) {
            present_queue.family_index = i;
            found_present = true;
        }
        if (supports_graphic && can_present) {
            graphic_queue.family_index = i;
            present_queue.family_index = i;
            break;
        }
    }

    if (graphic_queue.family_index == UINT32_MAX) {
        fprintf(stderr, "[VK] physical device does not support graphic queue\n");
        return false;
    }
    if (present_queue.family_index == UINT32_MAX) {
        fprintf(stderr, "[VK] physical device does not support prsentation queue\n");
        return false;
    }

    rd->graphic_queue = graphic_queue;
    rd->present_queue = present_queue;

    return true;
    // TODO transfer queue
}

static bool rd_create_logical_device(Renderer *rd) {
    f32 queue_priority = 1.f;
    VkDeviceQueueCreateInfo device_queue_info[2] = {
      {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = rd->graphic_queue.family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
      },
      {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
       .queueFamilyIndex = rd->present_queue.family_index,
       .queueCount = 1,
       .pQueuePriorities = &queue_priority}
      // TODO transfer
    };
    u32 queue_creation_info_count = 1 + (rd->present_queue.family_index != rd->graphic_queue.family_index);
    VkDeviceCreateInfo device_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &device_features,
      .queueCreateInfoCount = queue_creation_info_count,
      .pQueueCreateInfos = device_queue_info,
      .enabledExtensionCount = required_device_extension_count,
      .ppEnabledExtensionNames = required_device_extension_names};
    VKTRY(vkCreateDevice(rd->physical_device, &device_info, NULL, &rd->device),
          "[VK] failed to create logical device");
    printf("[VK]::LogicalDevice\n");
    vkGetDeviceQueue(rd->device, rd->graphic_queue.family_index, 0, &rd->graphic_queue.handle);
    printf("[VK]::GraphicQueue\n");
    vkGetDeviceQueue(rd->device, rd->present_queue.family_index, 0, &rd->present_queue.handle);
    printf("[VK]::PresentQueue\n");

    return true;
}

// @swapchain
static bool rd_create_swapchain(Renderer *rd, PlatformApi plat_api, Arena *scratch) {
    ArenaTemp temp = arena_temp_begin(scratch);
    RdSwapchain s = {};

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rd->physical_device, rd->surface, &capabilities);
    if (capabilities.currentExtent.width == UINT32_MAX) {
        u32 width, height;
        plat_api.get_framebuffer_size(plat_api.context, &width, &height);
        s.extent = {.width = CLAMP(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                    .height = CLAMP(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
    } else {
        s.extent = capabilities.currentExtent;
    }

    u32 requested_image_count = MAX(3u, capabilities.minImageCount);
    if (0 < capabilities.maxImageCount) { // maxImageCount == 0 => no max
        requested_image_count = MIN(requested_image_count, capabilities.maxImageCount);
    }

    u32 formats_count;
    VKTRY(vkGetPhysicalDeviceSurfaceFormatsKHR(rd->physical_device, rd->surface, &formats_count, NULL),
          "[VK] Unable to enumerate surface format");
    if (NEVER(formats_count == 0)) {
        fprintf(stderr, "[VK] no surface format available\n");
        return false;
    }
    VkSurfaceFormatKHR *formats = ARENA_PUSH_ARRAY(temp.arena, formats_count, VkSurfaceFormatKHR);
    vkGetPhysicalDeviceSurfaceFormatsKHR(rd->physical_device, rd->surface, &formats_count, formats);

    u32 format_index = 0;
    for (u32 i = 0; i < formats_count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format_index = i;
            break;
        }
    }
    s.surface_format = formats[format_index];

    u32 present_modes_count;
    VKTRY(vkGetPhysicalDeviceSurfacePresentModesKHR(rd->physical_device, rd->surface, &present_modes_count, NULL),
          "[VK] Unable to enumerate present mode");
    if (NEVER(present_modes_count == 0)) {
        fprintf(stderr, "[VK] no present mode available\n");
        return false;
    }
    VkPresentModeKHR *present_modes = ARENA_PUSH_ARRAY(temp.arena, present_modes_count, VkPresentModeKHR);
    vkGetPhysicalDeviceSurfacePresentModesKHR(rd->physical_device, rd->surface, &present_modes_count, present_modes);

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < present_modes_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }

    VkSwapchainCreateInfoKHR swapchain_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = rd->surface,
      .minImageCount = requested_image_count,
      .imageFormat = s.surface_format.format,
      .imageColorSpace = s.surface_format.colorSpace,
      .imageExtent = s.extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = present_mode,
      .clipped = true};

    u32 qfi[] = {rd->graphic_queue.family_index, rd->present_queue.family_index};
    if (qfi[0] == qfi[1]) {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    } else {
        swapchain_info.queueFamilyIndexCount = 2;
        swapchain_info.pQueueFamilyIndices = qfi;
        swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    }

    // TODO maybe test composite alpha

    VKTRY(vkCreateSwapchainKHR(rd->device, &swapchain_info, NULL, &s.handle),
          "[VK] Unable to create swapchain");

    s.image_count = 0;
    VKTRY(vkGetSwapchainImagesKHR(rd->device, s.handle, &s.image_count, NULL),
          "Vulkan error: unable to get swapchain image count");

    // allocate only on initialization (suppose count will not change)
    if (rd->swapchain.image_count == 0) {
        s.images = ARENA_PUSH_ARRAY(rd->arena, s.image_count, VkImage);
        s.image_views = ARENA_PUSH_ARRAY(rd->arena, s.image_count, VkImageView);
        s.present_semas = ARENA_PUSH_ARRAY(rd->arena, s.image_count, VkSemaphore);

        printf("[VK]::Swapchain (image, image_views and presentation_sema)\n");

    } else {
        if (NEVER(s.image_count != rd->swapchain.image_count)) {
            // TODO;
            return false;
        };
        s.images = rd->swapchain.images;
        s.image_views = rd->swapchain.image_views;
        s.present_semas = rd->swapchain.present_semas;
        printf("[VK] recreating swapchain...");
    }

    vkGetSwapchainImagesKHR(rd->device, s.handle, &s.image_count, s.images);
    if (NEVER(s.image_count == 0)) {
        fprintf(stderr, "[VK] unable to create any swapchain image\n");
        return false;
    }

    VkImageViewCreateInfo image_view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = s.surface_format.format,
      .components = {
        .r = VK_COMPONENT_SWIZZLE_R,
        .g = VK_COMPONENT_SWIZZLE_G,
        .b = VK_COMPONENT_SWIZZLE_B,
        .a = VK_COMPONENT_SWIZZLE_A,
      },
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseMipLevel = 0, // only mip 0 => image view for swapchain
                           .levelCount = 1,
                           .baseArrayLayer = 0, // only layer 0 => texture 2D normal (not cubemap or multiviewVR)
                           .layerCount = 1},
      // TODO experiment with swizzle
    };

    VkSemaphoreCreateInfo present_sema_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (u32 i = 0; i < s.image_count; i++) {
        image_view_info.image = s.images[i];
        VKTRY(vkCreateImageView(rd->device, &image_view_info, NULL, &s.image_views[i]),
              "[VK] Failed to created image view for a swapchain image");
        VKTRY(vkCreateSemaphore(rd->device, &present_sema_info, NULL, &s.present_semas[i]),
              "[VK] Failed to create a present semaphore for swapchain image");
    }

    rd->swapchain = s;

    arena_temp_end(temp);
    return true;
}

// // @descriptor set layout
static bool rd_create_frame_context(Renderer *rd) {
    u32 frame_count = rd->inflight_count;
    RdFrameContext *frames = ARENA_PUSH_ARRAY(rd->arena, frame_count, RdFrameContext);

    for (u32 i = 0; i < frame_count; i++) {
        VkCommandPoolCreateInfo pool_info = {
          .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
          .queueFamilyIndex = rd->graphic_queue.family_index};
        VKTRY(vkCreateCommandPool(rd->device, &pool_info, NULL, &frames[i].pool),
              "[VK] Unable to create graphic command pool");

        VkCommandBufferAllocateInfo alloc_info = {
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = frames[i].pool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1};
        VKTRY(vkAllocateCommandBuffers(rd->device, &alloc_info, &frames[i].cmd_buffer),
              "[VK] Failed to allocate graphic command buffers");

        VkFenceCreateInfo render_fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        VKTRY(vkCreateFence(rd->device, &render_fence_info, NULL, &frames[i].render_fence),
              "[VK] Failed to create draw fence");

        VkSemaphoreCreateInfo acquire_sema_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VKTRY(vkCreateSemaphore(rd->device, &acquire_sema_info, NULL, &frames[i].acquire_sema),
              "[VK] Failed to create acquire semaphore");

        printf("[VK]::frameContext %i (cmd_pool, cmd_buffer, render_fence, acquire_sema)\n", i);
    }

    rd->frames = frames;
    return true;
}

static bool rd_create_pipeline(Renderer *rd, Arena *scratch) {
    ArenaTemp temp = arena_temp_begin(scratch);
    RdPipeline p = {};

    Str shader_code = {0};
    read_file(temp.arena, S("./build/shaders/triangle.spv"), &shader_code);
    if (NEVER(shader_code.length % 4 != 0)) {
        fprintf(stderr, "Shader byte code is not multiple of 4\n");
        return false;
    }
    VkShaderModule shader_module;
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader_code.length,
        .pCode = (const u32 *)shader_code.data};
    VKTRY(vkCreateShaderModule(rd->device, &createInfo, NULL, &shader_module),
          "Vulkan error: Failed to create shader module");
    VkPipelineShaderStageCreateInfo vertex_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = shader_module,
        .pName = "vertexMain"};
    VkPipelineShaderStageCreateInfo fragment_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = shader_module,
        .pName = "fragmentMain"};
    VkPipelineShaderStageCreateInfo shader_stages[] = {vertex_stage_info, fragment_stage_info};

    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states};

    // VkVertexInputBindingDescription bindings[] = {};
    // VkVertexInputAttributeDescription attributes[] = {};
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        // .vertexBindingDescriptionCount = __,
        // .pVertexBindingDescriptions = bindings,
        //.vertexAttributeDescriptionCount = __,
        // .pVertexAttributeDescriptions = attributes,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1};

    VkPipelineRasterizationStateCreateInfo raterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .rasterizerDiscardEnable = false,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f};

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo color_blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment};

    if (rd->pipeline.layout == VK_NULL_HANDLE) {
        VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            // .setLayoutCount = 1,
            // .pSetLayouts = &app->descriptor_set_layout,
            .pushConstantRangeCount = 0};
        VKTRY(vkCreatePipelineLayout(rd->device, &pipeline_layout_info, NULL, &p.layout),
              "Vulkan error: Failed to create pipeline layout");
        printf("[VK]::PipelineLayout\n");
    } else {
        printf("[VK] recreating pipeline layout...\n");
    }
    VkPipelineRenderingCreateInfo pipeline_rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &rd->swapchain.surface_format.format};
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_info,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_state,
        .pRasterizationState = &raterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &color_blend_info,
        .pDynamicState = &dynamic_state,
        .layout = p.layout,
        .renderPass = NULL,
    };
    if (rd->pipeline.handle != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(rd->device);
        vkDestroyPipeline(rd->device, p.handle, NULL);
        printf("[VK] recreating pipeline...\n");
    } else {
        printf("[VK]::Pipeline\n");
    }

    VKTRY(vkCreateGraphicsPipelines(rd->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &p.handle),
          "Vulkan error: Failed to create graphics pipeline");
    vkDestroyShaderModule(rd->device, shader_module, NULL);

    rd->pipeline = p;
    arena_temp_end(temp);
    return true;
}

void transition_image_layout(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkAccessFlags2 src_access_mask, VkAccessFlags2 dst_access_mask, VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask, u32 src_queue_family_index, u32 dst_queue_family_index, VkImageAspectFlagBits image_aspect);
static u32 rd_begin_frame(Renderer *rd) {
    RdFrameContext *frame = &rd->frames[rd->frame_index];

    VKTRY(vkWaitForFences(rd->device, 1, &frame->render_fence, true, UINT64_MAX),
          "Draw frame error: unable to wait for draw fence");
    VKTRY(vkResetFences(rd->device, 1, &frame->render_fence),
          "Draw frame error: unable to reset draw fence");

    u32 image_index = 0;
    VKTRY(vkAcquireNextImageKHR(rd->device, rd->swapchain.handle, UINT64_MAX, frame->acquire_sema, NULL, &image_index),
          "Draw frame error: unable to acquire next image from swapchain");
    VKTRY(vkResetCommandPool(rd->device, frame->pool, 0),
          "Draw frame error: unable to reset the command buffer");

    VkCommandBuffer *cmd_buffer = &rd->frames[rd->frame_index].cmd_buffer;

    VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(*cmd_buffer, &begin_info);

    transition_image_layout(
      *cmd_buffer,
      rd->swapchain.images[image_index],
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      {},
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      VK_IMAGE_ASPECT_COLOR_BIT);
    VkRenderingAttachmentInfo color_attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = rd->swapchain.image_views[image_index],
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {.color = {.float32 = {0.f, 0.f, 0.f, 1.f}}}};
    VkRenderingInfo rendering_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.offset = {0, 0}, .extent = rd->swapchain.extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_info};

    vkCmdBeginRendering(*cmd_buffer, &rendering_info);

    return image_index;
}

static bool rd_render_triangle(Renderer *rd) {
    VkCommandBuffer *cmd_buffer = &rd->frames[rd->frame_index].cmd_buffer;

    vkCmdBindPipeline(*cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rd->pipeline.handle);

    VkViewport vp = {0., 0., (f32)rd->swapchain.extent.width, (f32)rd->swapchain.extent.width, 0., 1.};
    VkRect2D scissor = {{0, 0}, rd->swapchain.extent};
    vkCmdSetViewport(*cmd_buffer, 0., 1., &vp);
    vkCmdSetScissor(*cmd_buffer, 0., 1., &scissor);

    vkCmdDraw(*cmd_buffer, 3, 1, 0, 0);
    return true;
}

static bool rd_end_frame(Renderer *rd, u32 image_index) {
    RdFrameContext *frame = &rd->frames[rd->frame_index];
    VkCommandBuffer *cmd_buffer = &rd->frames[rd->frame_index].cmd_buffer;

    vkCmdEndRendering(*cmd_buffer);

    transition_image_layout(
      *cmd_buffer,
      rd->swapchain.images[image_index],
      VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      {},
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      VK_IMAGE_ASPECT_COLOR_BIT);

    vkEndCommandBuffer(*cmd_buffer);

    // submit command buffer to queue
    const VkPipelineStageFlags wait_dst_stage_mask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frame->acquire_sema,
      .pWaitDstStageMask = &wait_dst_stage_mask,
      .commandBufferCount = 1,
      .pCommandBuffers = cmd_buffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &rd->swapchain.present_semas[image_index]};
    vkQueueSubmit(rd->graphic_queue.handle, 1, &submit_info, frame->render_fence);

    const VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &rd->swapchain.present_semas[image_index],
      .swapchainCount = 1,
      .pSwapchains = &rd->swapchain.handle,
      .pImageIndices = &image_index};

    VKTRY(vkQueuePresentKHR(rd->present_queue.handle, &present_info),
          "Draw frame error: unable to present image");

    rd->frame_index = (rd->frame_index + 1) % rd->inflight_count;
    return true;
}

// static void rd_destroy(Renderer *rd) {
//  if (rd->device != VK_NULL_HANDLE)
//    vkDeviceWaitIdle(rd->device);
//
//  if (rd->image_available_semas != NULL) {
//    for (u32 i = 0; i < rd->inflight_count; i++) {
//      vkDestroySemaphore(rd->device, rd->image_available_semas[i], NULL);
//    }
//  }
//  if (rd->render_finish_semas != NULL) {
//    for (u32 i = 0; i < rd->swapchain_images_count; i++) {
//      vkDestroySemaphore(rd->device, rd->render_finish_semas[i], NULL);
//    }
//  }
//  if (rd->draw_fences != NULL) {
//    for (u32 i = 0; i < rd->inflight_count; i++) {
//      vkDestroyFence(rd->device, rd->draw_fences[i], NULL);
//    }
//  }
//
//  if (rd->geometry_buffer != VK_NULL_HANDLE)
//    vkDestroyBuffer(rd->device, rd->geometry_buffer, NULL);
//  if (rd->geometry_memory != VK_NULL_HANDLE)
//    vkFreeMemory(rd->device, rd->geometry_memory, NULL);
//
//  if (rd->instance_mrded_array != NULL) {
//    vkUnmapMemory(rd->device, rd->instance_memory);
//    rd->instance_mrded_array = NULL;
//  }
//  if (rd->instance_buffer != VK_NULL_HANDLE)
//    vkDestroyBuffer(rd->device, rd->instance_buffer, NULL);
//  if (rd->instance_memory != VK_NULL_HANDLE)
//    vkFreeMemory(rd->device, rd->instance_memory, NULL);
//
//  if (rd->texture_image != VK_NULL_HANDLE)
//    vkDestroyImage(rd->device, rd->texture_image, NULL);
//  if (rd->texture_memory != VK_NULL_HANDLE)
//    vkFreeMemory(rd->device, rd->texture_memory, NULL);
//  if (rd->texture_view != VK_NULL_HANDLE)
//    vkDestroyImageView(rd->device, rd->texture_view, NULL);
//  if (rd->texture_sampler != VK_NULL_HANDLE)
//    vkDestroySampler(rd->device, rd->texture_sampler, NULL);
//
//  if (rd->uniform_mrded_arrays != NULL) {
//    for (u32 i = 0; i < rd->inflight_count; i++)
//      vkUnmapMemory(rd->device, rd->uniform_memories[i]);
//    rd->uniform_mrded_arrays = NULL;
//  }
//  if (rd->uniform_buffers != NULL)
//    for (u32 i = 0; i < rd->inflight_count; i++)
//      vkDestroyBuffer(rd->device, rd->uniform_buffers[i], NULL);
//  if (rd->uniform_memories != NULL)
//    for (u32 i = 0; i < rd->inflight_count; i++)
//      vkFreeMemory(rd->device, rd->uniform_memories[i], NULL);
//
//  if (rd->graphic_command_buffers != NULL)
//    vkFreeCommandBuffers(rd->device, rd->graphic_command_pool,
//                         rd->inflight_count, rd->graphic_command_buffers);
//
//  if (rd->graphic_command_pool != VK_NULL_HANDLE)
//    vkDestroyCommandPool(rd->device, rd->graphic_command_pool, NULL);
//  if (rd->transfer_command_buffer != NULL)
//    vkFreeCommandBuffers(rd->device, rd->transfer_command_pool, 1,
//                         &rd->transfer_command_buffer);
//  if (rd->transfer_command_pool != VK_NULL_HANDLE)
//    vkDestroyCommandPool(rd->device, rd->transfer_command_pool, NULL);
//
//  if (rd->pipeline_layout != VK_NULL_HANDLE)
//    vkDestroyPipelineLayout(rd->device, rd->pipeline_layout, NULL);
//  if (rd->pipeline != VK_NULL_HANDLE)
//    vkDestroyPipeline(rd->device, rd->pipeline, NULL);
//
//  if (rd->descriptor_sets != NULL)
//    vkFreeDescriptorSets(rd->device, rd->descriptor_pool,
//    rd->inflight_count,
//                         rd->descriptor_sets);
//  if (rd->descriptor_pool != VK_NULL_HANDLE)
//    vkDestroyDescriptorPool(rd->device, rd->descriptor_pool, NULL);
//  if (rd->descriptor_set_layout != VK_NULL_HANDLE)
//    vkDestroyDescriptorSetLayout(rd->device, rd->descriptor_set_layout,
//    NULL);
//
//    if (rd->device != VK_NULL_HANDLE)
//        vkDestroyDevice(rd->device, NULL);
//
//    if (rd->surface != VK_NULL_HANDLE) {
//        vkDestroySurfaceKHR(rd->instance, rd->surface, NULL);
//    }
//
//    if (rd->instance != VK_NULL_HANDLE)
//        vkDestroyInstance(rd->instance, NULL);
//
//    arena_destroy(rd->arena);
//}

// @utils
bool has_extension(u32 actual_count, VkExtensionProperties *actual_props, const char *const expected) {
    for (u32 j = 0; j < actual_count; ++j) {
        if (strcmp(expected, actual_props[j].extensionName) == 0) {
            printf("\t %s found.\n", expected);
            return true;
        }
    }
    fprintf(stderr, "Required extension not supported: %s\n", expected);
    return false;
}
bool get_extensions(Arena *arena, PlatformApi plat_api, u32 *extension_count, const char ***extension_names) {
    // actual ext of vk
    u32 rd_available_extension_count = 0;

    vkEnumerateInstanceExtensionProperties(NULL, &rd_available_extension_count, NULL);
    VkExtensionProperties *rd_available_extension_properties = ARENA_PUSH_ARRAY(
      arena, rd_available_extension_count, VkExtensionProperties);
    vkEnumerateInstanceExtensionProperties(NULL, &rd_available_extension_count, rd_available_extension_properties);

    // expected ext by glfw
    u32 plat_extension_count = 0; // TODO change
    const char **plat_extension_names = plat_api.get_required_instance_extensions(plat_api.context, &plat_extension_count);
    if (plat_extension_names == NULL) {
        fprintf(stderr, "[VK] Failed to get plateform required extensions\n");
        return false;
    }
    for (u32 i = 0; i < plat_extension_count; ++i) {
        if (!has_extension(rd_available_extension_count,
                           rd_available_extension_properties,
                           (plat_extension_names)[i])) {
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

    *extension_count = plat_extension_count + (u32)extra_extension_count;
    *extension_names = ARENA_PUSH_ARRAY(arena, *extension_count, const char *);
    memcpy(*extension_names, plat_extension_names, sizeof(*plat_extension_names) * plat_extension_count);
    if (is_validation_enabled) {
        (*extension_names)[plat_extension_count] =
          VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

    return true;
}

bool has_layer(u32 actual_count, VkLayerProperties *actual_props, const char *const expected) {
    for (u32 i = 0; i < actual_count; i++) {
        if (strcmp(actual_props[i].layerName, expected) == 0) {
            printf("\t%s found.\n", expected);
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

        memcpy(required_layer_names, validation_layer_names, sizeof(validation_layer_names) * validation_layer_count);
    }

    u32 rd_layer_count;
    vkEnumerateInstanceLayerProperties(&rd_layer_count, NULL);
    VkLayerProperties *rd_layer_properties =
      ARENA_PUSH_ARRAY(arena, rd_layer_count, VkLayerProperties);
    vkEnumerateInstanceLayerProperties(&rd_layer_count, rd_layer_properties);

    for (u32 i = 0; i < required_layer_count; i++) {
        if (!has_layer(rd_layer_count, rd_layer_properties, required_layer_names[i])) {
            return false;
        }
    }
    *layer_count = required_layer_count;
    *layer_names = required_layer_names;

    return true;
}

// @transition
void transition_image_layout(
  VkCommandBuffer command_buffer,
  VkImage image,
  VkImageLayout old_layout,
  VkImageLayout new_layout,
  VkAccessFlags2 src_access_mask,
  VkAccessFlags2 dst_access_mask,
  VkPipelineStageFlags2 src_stage_mask,
  VkPipelineStageFlags2 dst_stage_mask,
  u32 src_queue_family_index,
  u32 dst_queue_family_index,
  VkImageAspectFlagBits image_aspect) {

    VkImageMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = src_stage_mask,
      .srcAccessMask = src_access_mask,
      .dstStageMask = dst_stage_mask,
      .dstAccessMask = dst_access_mask,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = src_queue_family_index,
      .dstQueueFamilyIndex = dst_queue_family_index,
      .image = image,
      .subresourceRange = {.aspectMask = image_aspect,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};

    VkDependencyInfo dependency_info = {.sType =
                                          VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                        .dependencyFlags = 0,
                                        .imageMemoryBarrierCount = 1,
                                        .pImageMemoryBarriers = &barrier};

    vkCmdPipelineBarrier2(command_buffer, &dependency_info);
}
