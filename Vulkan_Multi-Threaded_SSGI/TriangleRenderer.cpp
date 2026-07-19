#define _CRT_SECURE_NO_WARNINGS

#include "TriangleRenderer.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

TriangleRenderer::TriangleRenderer(VulkanContext& _context)
    : mContext(_context)
{
    auto shaderDir = std::filesystem::current_path() / "Resources" / "Shaders";
    mVertShaderGlslPath = shaderDir / "triangle.vert";
    mFragShaderGlslPath = shaderDir / "triangle.frag";
    mVertShaderSpvPath = shaderDir / "triangle.vert.spv";
    mFragShaderSpvPath = shaderDir / "triangle.frag.spv";

    mLastVertTimestamp = std::filesystem::last_write_time(mVertShaderGlslPath);
    mLastFragTimestamp = std::filesystem::last_write_time(mFragShaderGlslPath);

    CreateGraphicsPipeline();
    CreateFramebuffers();
    CreateCommandPool();

    vertices = {
    {{0.0f, -0.5f}},
    {{0.5f, 0.5f}},
    {{-0.5f, 0.5f}} };

    CreateVertexBuffer();
    CreateCommandBuffers();
    CreateSyncObjects();
}

TriangleRenderer::~TriangleRenderer()
{
    if (mContext.GetDevice() != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mContext.GetDevice());
    }

    for (auto framebuffer : mFramebuffers)
    {
        if (framebuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(mContext.GetDevice(), framebuffer, nullptr);
    }

    if (mInFlightFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(mContext.GetDevice(), mInFlightFence, nullptr);
    }

	if (mRenderFinishedSemaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(mContext.GetDevice(), mRenderFinishedSemaphore, nullptr);
	}

	if (mImageAvailableSemaphore != VK_NULL_HANDLE) 
    {
		vkDestroySemaphore(mContext.GetDevice(), mImageAvailableSemaphore, nullptr);
	}

	if (mCommandPool != VK_NULL_HANDLE) 
    { 
        vkDestroyCommandPool(mContext.GetDevice(), mCommandPool, nullptr); 
    }

	if (mPipeline != VK_NULL_HANDLE) 
    { 
        vkDestroyPipeline(mContext.GetDevice(), mPipeline, nullptr); 
    }

	if (mPipelineLayout != VK_NULL_HANDLE) 
    { 
        vkDestroyPipelineLayout(mContext.GetDevice(), mPipelineLayout, nullptr); 
    }

    if (mVertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mContext.GetDevice(), mVertexBuffer, nullptr);
    }

    if (mVertexBufferMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(mContext.GetDevice(), mVertexBufferMemory, nullptr);
    }
}

void TriangleRenderer::Update()
{
    // Make sure paths are not empty
    if (mVertShaderGlslPath.empty() || mFragShaderGlslPath.empty())
        return;

    // Check shader timestamps
    if (!CheckShaderTimestamps())
    {
        return;
    }
    
    std::cout << "[HotReload] Change detected. Compiling shaders...\n";

    if (!CompileShaders())
    {
        std::cerr << "[HotReload] Compile failed. Keeping existing pipeline.\n";
        return;
    }

    vkDeviceWaitIdle(mContext.GetDevice());

    ReloadShadersAndPipeline();

    // Update timestamps after successful reload
    std::error_code errorCode;
    mLastVertTimestamp = std::filesystem::last_write_time(mVertShaderGlslPath, errorCode);
    mLastFragTimestamp = std::filesystem::last_write_time(mFragShaderGlslPath, errorCode);

    std::cout << "[HotReload] Shaders reloaded successfully.\n";
}

void TriangleRenderer::DrawFrame()
{
    vkWaitForFences(mContext.GetDevice(), 1, &mInFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(mContext.GetDevice(), 1, &mInFlightFence);

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(mContext.GetDevice(), mContext.GetSwapchain(), UINT64_MAX, mImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        return;
    }

    vkResetCommandBuffer(mCommandBuffers[0], 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(mCommandBuffers[0], &beginInfo);

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = mContext.GetRenderPass();
    renderPassInfo.framebuffer = mFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = mContext.GetSwapchainExtent();
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(mCommandBuffers[0], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(mCommandBuffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
    
    VkBuffer buffers[] = { mVertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(mCommandBuffers[0], 0, 1, buffers, offsets);

    vkCmdDraw(mCommandBuffers[0], 3, 1, 0, 0);
    vkCmdEndRenderPass(mCommandBuffers[0]);
    vkEndCommandBuffer(mCommandBuffers[0]);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = { mImageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = mCommandBuffers.data();
    VkSemaphore signalSemaphores[] = { mRenderFinishedSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(mContext.GetGraphicsQueue(), 1, &submitInfo, mInFlightFence);

    VkSwapchainKHR swapchain = mContext.GetSwapchain();

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(mContext.GetPresentQueue(), &presentInfo);
}

bool TriangleRenderer::CreateGraphicsPipeline()
{
    if (!Helper::Vulkan::Shader::LoadShaderModule(mContext.GetDevice(), mVertShaderSpvPath, vertModule))
    {
        vertModule = VK_NULL_HANDLE;
        return false;
    }

    if (!Helper::Vulkan::Shader::LoadShaderModule(mContext.GetDevice(), mFragShaderSpvPath, fragModule))
    {
		if (vertModule != VK_NULL_HANDLE)
		{
			vkDestroyShaderModule(mContext.GetDevice(), vertModule, nullptr);
            vertModule = VK_NULL_HANDLE;
		}
		return false;
    }

    // Destroy old pipeline layout if we will recreate it
    if (mPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(mContext.GetDevice(), mPipelineLayout, nullptr);
        mPipelineLayout = VK_NULL_HANDLE;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (vkCreatePipelineLayout(mContext.GetDevice(), &pipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
    {
        // Cleanup shader modules on failure
        if (vertModule != VK_NULL_HANDLE) 
        { 
            vkDestroyShaderModule(mContext.GetDevice(), vertModule, nullptr); 
            vertModule = VK_NULL_HANDLE; 
        }
        
        if (fragModule != VK_NULL_HANDLE) 
        { 
            vkDestroyShaderModule(mContext.GetDevice(), fragModule, nullptr); 
            fragModule = VK_NULL_HANDLE; 
        }

        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertModule;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragModule;
    shaderStages[1].pName = "main";

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDesc{};
    attributeDesc.binding = 0;
    attributeDesc.location = 0;
    attributeDesc.format = VK_FORMAT_R32G32_SFLOAT;
    attributeDesc.offset = offsetof(Vertex, pos);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = &attributeDesc;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(mContext.GetSwapchainExtent().width);
    viewport.height = static_cast<float>(mContext.GetSwapchainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = mContext.GetSwapchainExtent();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = mPipelineLayout;
    pipelineInfo.renderPass = mContext.GetRenderPass();
    pipelineInfo.subpass = 0;

    VkResult pipelineResult = vkCreateGraphicsPipelines(mContext.GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipeline);

    return pipelineResult == VK_SUCCESS;
}

bool TriangleRenderer::CreateFramebuffers()
{
    mFramebuffers.resize(mContext.GetSwapchainImageViews().size());

    for (size_t i = 0; i < mContext.GetSwapchainImageViews().size(); ++i)
    {
        VkImageView attachments[] = { mContext.GetSwapchainImageViews()[i] };
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = mContext.GetRenderPass();
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = mContext.GetSwapchainExtent().width;
        framebufferInfo.height = mContext.GetSwapchainExtent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(mContext.GetDevice(), &framebufferInfo, nullptr, &mFramebuffers[i]) != VK_SUCCESS)
            return false;
    }

    return true;
}

bool TriangleRenderer::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = mContext.GetGraphicsQueueFamily();

    return vkCreateCommandPool(mContext.GetDevice(), &poolInfo, nullptr, &mCommandPool) == VK_SUCCESS;
}

bool TriangleRenderer::CreateCommandBuffers()
{
    mCommandBuffers.resize(1);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());

    return vkAllocateCommandBuffers(mContext.GetDevice(), &allocInfo, mCommandBuffers.data()) == VK_SUCCESS;
}

bool TriangleRenderer::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    return vkCreateSemaphore(mContext.GetDevice(), &semaphoreInfo, nullptr, &mImageAvailableSemaphore) == VK_SUCCESS
        && vkCreateSemaphore(mContext.GetDevice(), &semaphoreInfo, nullptr, &mRenderFinishedSemaphore) == VK_SUCCESS
        && vkCreateFence(mContext.GetDevice(), &fenceInfo, nullptr, &mInFlightFence) == VK_SUCCESS;
}

bool TriangleRenderer::CreateVertexBuffer()
{
    VkDevice device = mContext.GetDevice();
    if (vertices.empty())
    {
        return false;
    }
    
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // Create staging buffer (CPU-Visible)
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (!mContext.CreateBuffer(bufferSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory))
    {
        return false;
    }

    // Map and copy vertex data
    void* data = nullptr;
    vkMapMemory(device, stagingMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingMemory);

    // Create device-local vertex buffer
    if (!mContext.CreateBuffer(bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        mVertexBuffer,
        mVertexBufferMemory))
    {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        return false;
    }

    // Copy staging -> vertex buffer using a one-time command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(device, &allocInfo, &cmd) != VK_SUCCESS)
    {
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(cmd, stagingBuffer, mVertexBuffer, 1, &copyRegion);
    
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(mContext.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(mContext.GetGraphicsQueue());

    vkFreeCommandBuffers(device, mCommandPool, 1, &cmd);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    return true;
}

bool TriangleRenderer::CompileShaders()
{
    std::string out;
    auto absVert = std::filesystem::absolute(mVertShaderGlslPath).string();
    auto absFrag = std::filesystem::absolute(mFragShaderGlslPath).string();
    auto absVertSpv = std::filesystem::absolute(mVertShaderSpvPath).string();
    auto absFragSpv = std::filesystem::absolute(mFragShaderSpvPath).string();
    
    const char* sdkEnv = std::getenv("VULKAN_SDK");
    if (!sdkEnv)
    {
        std::cerr << "[HotReload] ERROR: VULKAN_SDK environment variable not set.\n";
        return false;
    }

    std::string glslang = std::string(sdkEnv) + "\\Bin\\glslangValidator.exe";

    // Vertex shader
    std::string cmdVert = "\"" + glslang + "\" -V \"" + absVert + "\" -o \"" + absVertSpv + "\"";

    int rc = Helper::Vulkan::RunCommandCaptureOutput(cmdVert, out);
    if (rc != 0) 
    { 
        std::cerr << "[HotReload] Vertex compile failed:\n" << out << "\n"; 
        return false;
    }

    // Fragment shader
    std::string cmdFrag = "\"" + glslang + "\" -V \"" + absFrag + "\" -o \"" + absFragSpv + "\"";
    rc = Helper::Vulkan::RunCommandCaptureOutput(cmdFrag, out);
    if (rc != 0) 
    { 
        std::cerr << "[HotReload] Fragment compile failed:\n" << out << "\n"; 
        return false; 
    }

    return true;
}

bool TriangleRenderer::CheckShaderTimestamps()
{
    std::error_code errorCode;
    auto vertWriteTime = std::filesystem::last_write_time(mVertShaderGlslPath, errorCode);
    if (errorCode)
    {
        return false;
    }

    auto fragWriteTime = std::filesystem::last_write_time(mFragShaderGlslPath, errorCode);
    if (errorCode)
    {
        return false;
    }

    return (vertWriteTime != mLastVertTimestamp) || (fragWriteTime != mLastFragTimestamp);
}

void TriangleRenderer::ReloadShadersAndPipeline()
{
    // Wait for GPU
    vkDeviceWaitIdle(mContext.GetDevice());

    // Destroy old pipeline
    if (mPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(mContext.GetDevice(), mPipeline, nullptr);
        mPipeline = VK_NULL_HANDLE;
    }

    // Destroy old shader modules (only here)
    if (vertModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(mContext.GetDevice(), vertModule, nullptr);
        vertModule = VK_NULL_HANDLE;
    }
    if (fragModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(mContext.GetDevice(), fragModule, nullptr);
        fragModule = VK_NULL_HANDLE;
    }

    // Destroy pipeline layout. CreateGraphicsPipeline will recreate it
    if (mPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(mContext.GetDevice(), mPipelineLayout, nullptr);
        mPipelineLayout = VK_NULL_HANDLE;
    }

    // Recreate pipeline (this will load the new .spv into vertModule/fragModule)
    if (!CreateGraphicsPipeline())
    {
        std::cerr << "[HotReload] Failed to recreate pipeline after shader compile\n";
        return;
    }

    // Re-record command buffers
    vkFreeCommandBuffers(mContext.GetDevice(), 
        mCommandPool, 
        static_cast<uint32_t>(mCommandBuffers.size()), 
        mCommandBuffers.data());

    CreateCommandBuffers();

    // Write timestamps on successful reload
    std::error_code errorCode;
    mLastVertTimestamp = std::filesystem::last_write_time(mVertShaderGlslPath, errorCode);
    mLastFragTimestamp = std::filesystem::last_write_time(mFragShaderGlslPath, errorCode);
}
