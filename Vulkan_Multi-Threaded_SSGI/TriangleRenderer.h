#pragma once
#include <filesystem>
#include <vector>
#include "VulkanContext.h"

struct Vertex
{
    float pos[2];
    // TODO: Add color later
    // float color[3];
};

class TriangleRenderer
{
public:
    explicit TriangleRenderer(VulkanContext& _context);
    ~TriangleRenderer();

    void DrawFrame();

private:
    bool CreateGraphicsPipeline();
    bool CreateFramebuffers();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();
    bool CreateVertexBuffer();

    void ReloadShadersAndPipeline();

    VulkanContext&                  mContext;
    VkPipelineLayout                mPipelineLayout = VK_NULL_HANDLE;
    VkPipeline                      mPipeline = VK_NULL_HANDLE;
    VkCommandPool                   mCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer>    mCommandBuffers;
    std::vector<VkFramebuffer>      mFramebuffers;
    VkSemaphore                     mImageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore                     mRenderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence                         mInFlightFence = VK_NULL_HANDLE;

    VkBuffer                        mVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory                  mVertexBufferMemory = VK_NULL_HANDLE;

    std::vector<Vertex> vertices = {};

    // Variables for hot reload
    VkShaderModule vertModule = VK_NULL_HANDLE;
    VkShaderModule fragModule = VK_NULL_HANDLE;
    std::filesystem::path mVertShaderPath;
    std::filesystem::path mFragShaderPath;
    std::filesystem::file_time_type vertTimestamp;
    std::filesystem::file_time_type fragTimestamp;
};
