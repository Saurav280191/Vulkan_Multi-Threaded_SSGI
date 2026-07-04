#pragma once
#include <string>
#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>

class Window
{
public:
	Window(int _width, int _height, const std::string& _title);
	
	bool Init();
	void PollEvents();
	bool ShouldClose() const;

	GLFWwindow* GetHandle() const 
	{
		return mWindowPtr;
	}

private:
	int mWidth;
	int mHeight;
	std::string mTitle;
	GLFWwindow* mWindowPtr = nullptr;
};