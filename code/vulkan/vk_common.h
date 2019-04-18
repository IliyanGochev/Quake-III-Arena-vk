#ifndef __VK_COMMON_H__
#define __VK_COMMON_H__

extern "C" {
#   include "../renderer/tr_local.h"
#   include "../renderer/tr_layer.h"
#   include "../qcommon/qcommon.h"
}
#define VK_USE_PLATFORM_WIN32_KHR  1
#include <vulkan/vulkan.h>

#define VK_PUBLIC extern "C"

#define MAX_FRAMES_IN_FLIGHT 2

void VkCheckError(VkResult result);

uint32_t FindMemoryTypeIndex(VkPhysicalDeviceMemoryProperties* properties,
	const VkMemoryRequirements* memoryRequirements, 
	VkMemoryPropertyFlags requiredMemoryProperties);

void VkCopyBuffer(VkCommandBuffer& commandBuffer, VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size);
void VkCopyBufferToImage(VkCommandBuffer& commandBuffer, VkBuffer& buffer, VkImage& image, uint32_t width, uint32_t height);
uint32_t AcquireNextSwapchainImage(VkDevice& device, VkSwapchainKHR& swapchain, VkSemaphore& fence);
#endif
