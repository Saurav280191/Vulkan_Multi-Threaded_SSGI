#include "TriangleRenderer.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace Helper
{
	bool LoadShaderModule(VkDevice _device, const std::filesystem::path& _spvPath, VkShaderModule& _outModule)
	{
        if (_spvPath.extension() != ".spv")
        {
            std::cout << "Invalid .spv file!" << std::endl;
        }

		std::ifstream file(_spvPath, std::ios::binary | std::ios::ate);
		if (!file.is_open())
			return false;

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);
		std::vector<char> buffer(static_cast<size_t>(size));
		if (!file.read(buffer.data(), size))
			return false;

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = buffer.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());
		return vkCreateShaderModule(device, &createInfo, nullptr, &outModule) == VK_SUCCESS;
	}
}

TriangleRenderer::TriangleRenderer(VulkanContext& _context)
    : mContext(_context)
{
    CreateGraphicsPipeline();
    CreateFramebuffers();
    CreateCommandPool();
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
}

bool TriangleRenderer::CreateGraphicsPipeline()
{
    std::filesystem::path shaderDir = std::filesystem::current_path() / "Resources/Shaders";
    
    if (!std::filesystem::exists(shaderDir))
    {
        std::filesystem::create_directories(shaderDir);
    }

	std::filesystem::path vertShaderPath = shaderDir / "triangle.vert.spv";
	std::filesystem::path fragShaderPath = shaderDir / "triangle.frag.spv";

    VkShaderModule vertModule = VK_NULL_HANDLE;
    if (!Helper::LoadShaderModule(mContext.GetDevice(), vertShaderPath, vertModule))
        return false;

    VkShaderModule fragModule = VK_NULL_HANDLE;
    if (!Helper::LoadShaderModule(mContext.GetDevice(), fragShaderPath, fragModule))
    {
        vkDestroyShaderModule(mContext.GetDevice(), vertModule, nullptr);
        return false;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (vkCreatePipelineLayout(mContext.GetDevice(), &pipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
        return false;

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertModule;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragModule;
    shaderStages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

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

    vkDestroyShaderModule(mContext.GetDevice(), fragModule, nullptr);
    vkDestroyShaderModule(mContext.GetDevice(), vertModule, nullptr);

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
