#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include "Window.h"

class VulkanContext
{
public:
	bool Init(Window& _window);
	void WaitIdle();

	VkDevice GetDevice() const { return m_Device; }
	VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
	VkQueue GetPresentQueue() const { return m_PresentQueue; }
	VkSurfaceKHR GetSurface() const { return mSurface; }
	VkSwapchainKHR GetSwapchain() const { return mSwapChain; }
	VkRenderPass GetRenderPass() const { return mRenderPass; }
	VkPhysicalDevice GetPhysicalDevice() const { return mPhysicalDevice; }
	VkExtent2D GetSwapchainExtent() const { return mSwapchainExtent; }
	VkFormat GetSwapchainFormat() const { return mSwapchainFormat; }
	const std::vector<VkImage>& GetSwapchainImages() const { return mSwapchainImages; }
	const std::vector<VkImageView>& GetSwapchainImageViews() const { return mSwapchainImageViews; }
	uint32_t GetGraphicsQueueFamily() const { return mGraphicsQueueFamily; }
	uint32_t GetPresentQueueFamily() const { return mPresentQueueFamily; }

private:
	bool CreateInstance();
	bool CreateSurface(Window& _window);
	bool PickPhysicalDevice();
	bool CreateDevice();
	bool CreateSwapchain();
	bool CreateRenderPass();

	VkInstance mInstance = VK_NULL_HANDLE;
	VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
	VkDevice m_Device = VK_NULL_HANDLE;
	VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
	VkQueue m_PresentQueue = VK_NULL_HANDLE;
	VkSurfaceKHR mSurface = VK_NULL_HANDLE;
	VkSwapchainKHR mSwapChain = VK_NULL_HANDLE;
	VkRenderPass mRenderPass = VK_NULL_HANDLE;
	VkExtent2D mSwapchainExtent{};
	VkFormat mSwapchainFormat = VK_FORMAT_UNDEFINED;
	std::vector<VkImage> mSwapchainImages;
	std::vector<VkImageView> mSwapchainImageViews;
	uint32_t mGraphicsQueueFamily = 0;
	uint32_t mPresentQueueFamily = 0;
};

