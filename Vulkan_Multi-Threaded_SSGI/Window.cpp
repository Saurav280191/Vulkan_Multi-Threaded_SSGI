#include "Window.h"

Window::Window(int _width, int _height, const std::string& _title) :
	mWidth(_width),
	mHeight(_height),
	mTitle(_title)
{

}

bool Window::Init()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	mWindowPtr = glfwCreateWindow(mWidth, mHeight, mTitle.c_str(), nullptr, nullptr);
	
	return (mWindowPtr != nullptr);
}

void Window::PollEvents()
{
	glfwPollEvents();
}

bool Window::ShouldClose() const
{
	return glfwWindowShouldClose(mWindowPtr);
}
