#include "vk_common.h"
#include "vk_state.h"

void VkCheckError(VkResult result)
{
	if (result < 0)
	{
		switch (result) {
		case VK_SUCCESS: break;

		case VK_NOT_READY:
			ri.Error(ERR_FATAL, "VKDBG: VK_NOT_READY!\n");
			break;
		case VK_TIMEOUT:
			ri.Error(ERR_FATAL, "VKDBG: VK_TIMEOUT!\n");
			break;
		case VK_EVENT_SET:
			ri.Error(ERR_FATAL, "VKDBG: VK_EVENT_SET!\n");
			break;
		case VK_EVENT_RESET:
			ri.Error(ERR_FATAL, "VKDBG: VK_EVENT_RESET!\n");
			break;
		case VK_INCOMPLETE:
			ri.Error(ERR_FATAL, "VKDBG: VK_INCOMPLETE!\n");
			break;
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_OUT_OF_HOST_MEMORY!\n");
			break;
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_OUT_OF_DEVICE_MEMORY!\n");
			break;
		case VK_ERROR_INITIALIZATION_FAILED:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_INITIALIZATION_FAILED!\n");
			break;
		case VK_ERROR_DEVICE_LOST:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_DEVICE_LOST!\n");
			break;
		case VK_ERROR_MEMORY_MAP_FAILED:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_MEMORY_MAP_FAILED!\n");
			break;
		case VK_ERROR_LAYER_NOT_PRESENT:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_LAYER_NOT_PRESENT!\n");
			break;
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_EXTENSION_NOT_PRESENT!\n");
			break;
		case VK_ERROR_FEATURE_NOT_PRESENT:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_FEATURE_NOT_PRESENT!\n");
			break;
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_INCOMPATIBLE_DRIVER!\n");
			break;
		case VK_ERROR_TOO_MANY_OBJECTS:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_TOO_MANY_OBJECTS!\n");
			break;
		case VK_ERROR_FORMAT_NOT_SUPPORTED:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_FORMAT_NOT_SUPPORTED!\n");
			break;
		case VK_ERROR_FRAGMENTED_POOL:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_FRAGMENTED_POOL!\n");
			break;
		case VK_ERROR_OUT_OF_POOL_MEMORY:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_OUT_OF_POOL_MEMORY!\n");
			break;
		case VK_ERROR_INVALID_EXTERNAL_HANDLE:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_INVALID_EXTERNAL_HANDLE!\n");
			break;
		case VK_ERROR_SURFACE_LOST_KHR:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_SURFACE_LOST_KHR!\n");
			break;
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_NATIVE_WINDOW_IN_USE_KHR!\n");
			break;
		case VK_SUBOPTIMAL_KHR:
			ri.Error(ERR_FATAL, "VKDBG: VK_SUBOPTIMAL_KHR!\n");
			break;
		case VK_ERROR_OUT_OF_DATE_KHR:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_OUT_OF_DATE_KHR!\n");
			break;
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_INCOMPATIBLE_DISPLAY_KHR!\n");
			break;
		case VK_ERROR_VALIDATION_FAILED_EXT:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_VALIDATION_FAILED_EXT!\n");
			break;
		case VK_ERROR_INVALID_SHADER_NV:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_INVALID_SHADER_NV!\n");
			break;
		case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT!\n");
			break;
		case VK_ERROR_FRAGMENTATION_EXT:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_FRAGMENTATION_EXT!\n");
			break;
		case VK_ERROR_NOT_PERMITTED_EXT:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_NOT_PERMITTED_EXT!\n");
			break;
		case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:
			ri.Error(ERR_FATAL, "VKDBG: VK_ERROR_INVALID_DEVICE_ADDRESS_EXT!\n");
			break;
		default:;
		}
		assert(0 && "Error occurred!");
	}
}

uint32_t FindMemoryTypeIndex(VkPhysicalDeviceMemoryProperties* memoryProperties,
							const VkMemoryRequirements* memoryRequirements,
							VkMemoryPropertyFlags requiredMemoryProperties) {
	for (uint32_t i = 0; memoryProperties->memoryTypeCount; ++i) {
		if (memoryRequirements->memoryTypeBits & (1 << i)) {
			// TODO: think more about this checks
			if ((memoryProperties->memoryTypes[i].propertyFlags & requiredMemoryProperties) == requiredMemoryProperties) {
				return i;
				break;
			}
		}
	}
	assert(0 && "No sutable memory type found");
	return UINT32_MAX;
}

void VkCopyBuffer(VkCommandBuffer& commandBuffer, VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size) {
	VkBufferCopy copyRegion = {};	
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);	
}

void VkCopyBufferToImage(VkCommandBuffer& commandBuffer, VkBuffer& buffer, VkImage& image, uint32_t width, uint32_t height) {	
	VkBufferImageCopy region = {};
	region.bufferOffset						= 0;
	region.bufferRowLength					= width;
	region.bufferImageHeight				= height;
	region.imageSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel		= 0;
	region.imageSubresource.baseArrayLayer	= 0;
	region.imageSubresource.layerCount		= 1;
	region.imageOffset						= { 0, 0, 0 };
	region.imageExtent						= {width,height,1};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);	
}

uint32_t AcquireNextSwapchainImage(VkDevice &device, VkSwapchainKHR &swapchain, VkSemaphore& semaphore) {
	uint32_t currentSwapchainImageIndex;

	VkCheckError(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,	 semaphore,
									VK_NULL_HANDLE, &currentSwapchainImageIndex));
	return  currentSwapchainImageIndex;
}