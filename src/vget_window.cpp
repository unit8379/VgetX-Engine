#include "vget_window.hpp"

// std
#include <stdexcept>

namespace vget
{
	VgetWindow::VgetWindow(int w, int h, std::string name) : width{ w }, height{ h }, windowName{ name }
	{
		initWindow();
	}

	VgetWindow::~VgetWindow()
	{
		// Уничтожение окна и его контекста. Освобождение всех остальных ресурсов библиотеки GLFW.
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void VgetWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface");
		}
	}

	void VgetWindow::initWindow()
	{
		glfwInit();  // GLFW library initialization

		// Настройки контекста окна GLFW перед его созданием.
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // Не создавать контекст графического API при создании окна.
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);  // Включить возможность изменять размер окна.

		// Создание окна и его контекста.
		window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);  // Сопряжение указателя GLFWwindow* и указателя на текущий экземпляр VgetWindow*.
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);  // установка callback функции на изменение размера окна (буфера кадра)
	}

	void VgetWindow::framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		/* Используя приведение reinterpret_cast(), получаем из указателя на окно GLFWwindow*
		   указатель на пользовательский тип окна VgetWindow*, которые были сопряжены ранее функцией */
		auto vgetWindow = reinterpret_cast<VgetWindow*>(glfwGetWindowUserPointer(window));

		vgetWindow->framebufferResized = true;
		vgetWindow->width = width;
		vgetWindow->height = height;
	}
}