#pragma once
#include <vulkan/vulkan.h>
#include <vector>

extern VkDevice								g_vkDevice;
extern VkPhysicalDeviceMemoryProperties		g_vkPhysicalDeviceMemoryProperties;
extern VkCommandPool						g_vkCommandPool;
extern std::vector<VkCommandBuffer>			g_vkCommandBuffers;
extern VkQueue								g_vkQueue;
extern uint32_t								g_vkCurrentSwapchainImageIndex;
extern VkSwapchainKHR						g_vkSwapchain;
extern VkRenderPass							g_vkRenderPass;
extern std::vector<VkFramebuffer>			g_vkFramebuffers;
extern VkSurfaceCapabilitiesKHR				g_vkSurfaceCapabilites;
extern std::vector<VkSemaphore>				g_vkImageAvailableSemaphores;
extern std::vector<VkSemaphore>				g_vkRenderFinishedSemaphores;
extern std::vector<VkFence>					g_vkFencesInFlight;
extern std::vector<VkImage>					g_vkSwapchainImages;