#pragma once
#include <vector>
#include "VulkanContext.h"

class TriangleRenderer
{
public:
    explicit TriangleRenderer(VulkanContext& _context);
    ~TriangleRenderer();

    void DrawFrame();

private:
    VulkanContext& mContext;
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mPipeline = VK_NULL_HANDLE;
    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCommandBuffers;
    std::vector<VkFramebuffer> mFramebuffers;
    VkSemaphore mImageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore mRenderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence mInFlightFence = VK_NULL_HANDLE;

    bool CreateGraphicsPipeline();
    bool CreateFramebuffers();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();
};
