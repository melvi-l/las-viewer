#include "common/base.h"
#include <vulkan/vulkan_core.h>

typedef struct RdQueue {
    VkQueue handle;
    u32 family_index;
    // u32 queue_index;
} RdQueue;

// presented
typedef struct RdSwapchain {
    VkSwapchainKHR handle;

    VkSurfaceFormatKHR surface_format;
    VkExtent2D extent;

    u32 image_count;
    VkImage *images;
    VkImageView *image_views;

    VkSemaphore *present_semas;
} RdSwapchain;

// drawn
typedef struct RdFrameContext {
    VkCommandPool pool;
    VkCommandBuffer cmd_buffer;
    VkFence render_fence;
    VkSemaphore acquire_sema;
} RdFrameContext;

typedef struct RdPipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
} RdPipeline;

typedef struct Renderer {
    Arena *arena;
    VkInstance instance;
    VkSurfaceKHR surface;

    VkPhysicalDevice physical_device;
    RdQueue graphic_queue;
    RdQueue present_queue;
    VkDevice device;

    RdSwapchain swapchain;

    u32 inflight_count;
    u32 frame_index;
    RdFrameContext *frames;

    RdPipeline pipeline;
} Renderer;
