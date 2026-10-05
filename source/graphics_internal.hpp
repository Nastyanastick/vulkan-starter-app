#pragma once

#include <cstdint>

#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

struct GLFWwindow;

namespace graphics::internal {

struct Context {
	VkPhysicalDevice physical_device;
	VkDevice device; // графическое устройство

	VmaAllocator allocator; // управление видеопамятью

	VkQueue graphics_queue; // очередь графических команд
	uint32_t graphics_queue_index;

	VkFormat swapchain_format; // формат изображения
	VkExtent2D swapchain_extent; // размер изображения

	VkRenderPass render_pass; // проход отрисовки
};

struct FrameData { // запись команд для отрисовки текущего кадра
	VkFramebuffer framebuffer;
	VkCommandBuffer command_buffer;
};

extern Context context;

bool initialize(GLFWwindow* const window);
void shutdown();

void resize(uint32_t width, uint32_t height);

FrameData prepare();
void submitAndPresent();

} // namespace graphics::internal