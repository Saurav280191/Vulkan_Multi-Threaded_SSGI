#include "Window.h"
#include "VulkanContext.h"
#include "TriangleRenderer.h"

int main()
{
	Window window(1280, 720, "Vulkan_Multi-threaded_SSGI");

	if (!window.Init())
	{
		return -1;
	}

	VulkanContext vulkanContext;
	if (!vulkanContext.Init(window))
	{
		return -1;
	}

	TriangleRenderer renderer(vulkanContext);

	while (!window.ShouldClose())
	{
		window.PollEvents();
		renderer.DrawFrame();
	}

	vulkanContext.WaitIdle();
	return 0;
}