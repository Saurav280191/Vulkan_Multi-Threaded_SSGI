#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include "Window.h"

class VulkanContext
{
public:
	bool Init(Window& _window);
	void WaitIdle();
	
	bool CreateBuffer(VkDeviceSize _size, 
		VkBufferUsageFlags _usage,
		VkMemoryPropertyFlags _properties, 
		VkBuffer& _buffer, 
		VkDeviceMemory& _bufferMemory);

#pragma region Getters
	VkDevice GetDevice() const { return mDevice; }
	VkQueue GetGraphicsQueue() const { return mGraphicsQueue; }
	VkQueue GetPresentQueue() const { return mPresentQueue; }
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
#pragma endregion

private:
	bool CreateInstance();
	bool CreateSurface(Window& _window);
	bool PickPhysicalDevice();
	bool CreateDevice();
	bool CreateSwapchain();
	bool CreateRenderPass();
	
	uint32_t FindMemoryType(uint32_t _typeFilter, VkMemoryPropertyFlags _properties);
	VkInstance mInstance = VK_NULL_HANDLE;
	VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
	VkDevice mDevice = VK_NULL_HANDLE;
	VkQueue mGraphicsQueue = VK_NULL_HANDLE;
	VkQueue mPresentQueue = VK_NULL_HANDLE;
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

