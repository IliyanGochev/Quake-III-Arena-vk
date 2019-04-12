#include "vk_common.h"
#include "vk_driver.h"
#include "vk_state.h"
#include "../win32/win_vulkan.h"

#include <cmath>
struct vkImage_t {
	VkImage textureImage;
	VkImageView textureImageView;
	VkDeviceMemory textureImageMemory;
	VkSampler sampler;
	VkFormat format;
	int width;
	int height;
	int frameUsed;
	qboolean dynamic;
};

static vkImage_t g_vkImages[MAX_DRAWIMAGES];
void VkSetupVideoConfig()
{
	// TODO: Fix me!
	const char* vulkanVersionStr = "";
}

void VkInitDrawState()
{
	// Init images
	Com_Memset(g_vkImages, 0, sizeof(g_vkImages));
}

VK_PUBLIC void VKDrv_DriverInit(void)
{
	GFX_Shutdown						= VKDrv_Shutdown;
	GFX_UnbindResources					= VKDrv_UnbindResources;
	GFX_LastError						= VKDrv_LastError;
	GFX_ReadPixels						= VKDrv_ReadPixels;
	GFX_ReadDepth						= VKDrv_ReadDepth;
	GFX_ReadStencil						= VKDrv_ReadStencil;
	GFX_CreateImage						= VKDrv_CreateImage;
	GFX_DeleteImage						= VKDrv_DeleteImage;
	GFX_UpdateCinematic					= VKDrv_UpdateCinematic;
	GFX_DrawImage						= VKDrv_DrawImage;
	GFX_GetImageFormat					= VKDrv_GetImageFormat;
	GFX_SetGamma						= VKDrv_SetGamma;
	GFX_GetFrameImageMemoryUsage		= VKDrv_SumOfUsedImages;
	GFX_GraphicsInfo					= VKDrv_GfxInfo;
	GFX_Clear							= VKDrv_Clear;
	GFX_SetProjectionMatrix				= VKDrv_SetProjection;
	GFX_GetProjectionMatrix				= VKDrv_GetProjection;
	GFX_SetModelViewMatrix				= VKDrv_SetModelView;
	GFX_GetModelViewMatrix				= VKDrv_GetModelView;
	GFX_SetViewport						= VKDrv_SetViewport;
	GFX_Flush							= VKDrv_Flush;
	GFX_SetState						= VKDrv_SetState;
	GFX_ResetState2D					= VKDrv_ResetState2D;
	GFX_ResetState3D					= VKDrv_ResetState3D;
	GFX_SetPortalRendering				= VKDrv_SetPortalRendering;
	GFX_SetDepthRange					= VKDrv_SetDepthRange;
	GFX_SetDrawBuffer					= VKDrv_SetDrawBuffer;
	GFX_EndFrame						= VKDrv_EndFrame;
	GFX_MakeCurrent						= VKDrv_MakeCurrent;
	GFX_ShadowSilhouette				= VKDrv_ShadowSilhouette;
	GFX_ShadowFinish					= VKDrv_ShadowFinish;
	GFX_DrawSkyBox						= VKDrv_DrawSkyBox;
	GFX_DrawBeam						= VKDrv_DrawBeam;
	GFX_DrawStageGeneric				= VKDrv_DrawStageGeneric;
	GFX_DrawStageVertexLitTexture		= VKDrv_DrawStageVertexLitTexture;
	GFX_DrawStageLightmappedMultitexture= VKDrv_DrawStageLightmappedMultitexture;
	GFX_DebugDrawAxis					= VKDrv_DebugDrawAxis;
	GFX_DebugDrawTris					= VKDrv_DebugDrawTris;
	GFX_DebugDrawNormals				= VKDrv_DebugDrawNormals;
	GFX_DebugSetOverdrawMeasureEnabled	= VKDrv_DebugSetOverdrawMeasureEnabled;
	GFX_DebugSetTextureMode				= VKDrv_DebugSetTextureMode;
	GFX_DebugDrawPolygon				= VKDrv_DebugDrawPolygon;

	// Init VK Window
	VKWnd_Init();
	// Init Draw State
	VkInitDrawState();
	//Setup vdConfig global
	VkSetupVideoConfig();
}

void VKDrv_Shutdown()
{
	VKWnd_Shutdown();
}

void VKDrv_UnbindResources()
{
}

size_t VKDrv_LastError()
{
	return -1;
}

void VKDrv_ReadPixels(int x, int y, int width, int height, imageFormat_t requestedFormat, void* dest)
{
}

void VKDrv_ReadDepth(int x, int y, int width, int height, float* dest)
{
}

void VKDrv_ReadStencil(int x, int y, int width, int height, byte* dest)
{
}

void TransitionImageLayout(VkImage& image, VkCommandBuffer& commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout ) {	

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // ???
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else {
		ri.Error(ERR_FATAL,"Unsupported layout transition");
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}

void CreateVêImage(const image_t* image, int mipLevels, const byte* pic, qboolean isLightmap) {
	vkImage_t* vkImg = &g_vkImages[image->index];
	Com_Memset(vkImg, 0, sizeof(vkImage_t));

	// We are defaulting to RGBA8 images for now
	vkImg->width	= image->width;
	vkImg->height	= image->height;
	vkImg->format	= VK_FORMAT_R8G8B8A8_UNORM;

	if (Q_strncmp(image->imgName, "*scratch", sizeof(image->imgName))) {
		vkImg->dynamic = qtrue;
	}

	size_t imageSizeBytes = image->width * image->height * sizeof(UINT);
	void* lightscaledCopy = ri.Hunk_AllocateTempMemory((int)imageSizeBytes);
	memcpy(lightscaledCopy, pic, imageSizeBytes);
	R_LightScaleTexture((unsigned int*)lightscaledCopy, 
		image->width, 
		image->height, 
		(qboolean)(mipLevels == 1));

	VkDeviceSize imageDeviceSize = vkImg->width * vkImg->height * 4;
	// Allocate staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	{
		VkBufferCreateInfo createInfo{};
		createInfo.sType		= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		createInfo.size			= imageDeviceSize;
		createInfo.usage		= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		createInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

		VkCheckError(vkCreateBuffer(g_vkDevice, &createInfo, nullptr, &stagingBuffer));

		VkMemoryRequirements memoryRequirements{};
		vkGetBufferMemoryRequirements(g_vkDevice, stagingBuffer, &memoryRequirements);

		VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		VkMemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = FindMemoryTypeIndex(
			&g_vkPhysicalDeviceMemoryProperties,
			&memoryRequirements, propertyFlags);

		// Could result in lots of allocations
		VkCheckError(vkAllocateMemory(g_vkDevice, &memoryAllocateInfo, nullptr, &stagingBufferMemory));
		VkCheckError(vkBindBufferMemory(g_vkDevice, stagingBuffer, stagingBufferMemory, 0));

		void* data;
		VkCheckError(vkMapMemory(g_vkDevice, stagingBufferMemory, 0, imageDeviceSize, 0, &data));
		memcpy(data, pic, static_cast<size_t> (imageDeviceSize));
		vkUnmapMemory(g_vkDevice, stagingBufferMemory);
	}
	// Create the image
	{
		VkImageCreateInfo createInfo{};
		createInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		createInfo.imageType		= VK_IMAGE_TYPE_2D;
		createInfo.extent.width		= image->width;
		createInfo.extent.height	= image->height;
		createInfo.extent.depth		= 1;
		createInfo.mipLevels		= mipLevels;
		createInfo.arrayLayers		= 1;
		createInfo.format			= vkImg->format;
		createInfo.tiling			= VK_IMAGE_TILING_OPTIMAL;
		createInfo.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
		createInfo.usage			= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		createInfo.sharingMode		= VK_SHARING_MODE_EXCLUSIVE;
		createInfo.samples			= VK_SAMPLE_COUNT_1_BIT; // TODO: Change this ?

		VkCheckError(vkCreateImage(g_vkDevice, &createInfo, nullptr, &vkImg->textureImage));

		VkMemoryRequirements memoryRequirements{};
		vkGetImageMemoryRequirements(g_vkDevice, vkImg->textureImage, &memoryRequirements);
		VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		VkMemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = FindMemoryTypeIndex(
			&g_vkPhysicalDeviceMemoryProperties,
			&memoryRequirements, propertyFlags);
		VkCheckError(vkAllocateMemory(g_vkDevice, &memoryAllocateInfo, nullptr, &vkImg->textureImageMemory));
		vkBindImageMemory(g_vkDevice, vkImg->textureImage, vkImg->textureImageMemory, 0);
	}
	// Copy the data from staging to image
	
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VkCheckError(vkBeginCommandBuffer(g_vkCmdBuffer, &beginInfo));

		TransitionImageLayout(vkImg->textureImage, g_vkCmdBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		VkCopyBufferToImage(g_vkCmdBuffer, stagingBuffer, vkImg->textureImage, vkImg->width, vkImg->height);
		TransitionImageLayout(vkImg->textureImage, g_vkCmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		
		VkCheckError(vkEndCommandBuffer(g_vkCmdBuffer));

		VkSubmitInfo submitInfo{};
		submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount	= 1;
		submitInfo.pCommandBuffers		= &g_vkCmdBuffer;

		VkCheckError(vkQueueSubmit(g_vkQueue, 1, &submitInfo, VK_NULL_HANDLE));
		vkQueueWaitIdle(g_vkQueue);

	}
	// free the staging buffers
	vkDestroyBuffer(g_vkDevice, stagingBuffer, nullptr);
	vkFreeMemory(g_vkDevice, stagingBufferMemory, nullptr);
}

void VKDrv_CreateImage(const image_t* image, const byte* pic, qboolean isLightmap)
{
	int mipLevels = 1;
	if (image->mipmap) {		
		mipLevels = static_cast<int>( 
			std::floor(std::log2(max(image->width, image->height)))) + 1;
	}
	CreateVêImage(image, mipLevels, pic, isLightmap);
}

void VKDrv_DeleteImage(const image_t* image)
{
	vkImage_t* vkImg = &g_vkImages[image->index];

	vkFreeMemory(g_vkDevice, vkImg->textureImageMemory, nullptr);
	vkDestroyImageView(g_vkDevice, vkImg->textureImageView, nullptr);
	vkDestroyImage(g_vkDevice, vkImg->textureImage, nullptr);
}

void VKDrv_UpdateCinematic(const image_t* image, const byte* pic, int cols, int rows, qboolean dirty)
{
	// TODO:(.plan) To have cinematics we need to implement:
	//		VKDrv_SetViewport
	//		VKDrv_SetProjection
	//		VKDrv_ResetState2D ?
	//		VKDrv_DrawImage

}

void VKDrv_DrawImage(const image_t* image, const float* coords, const float* texcoords, const float* color)
{
}

imageFormat_t VKDrv_GetImageFormat(const image_t* image)
{	
	// For now we re supporting RGBA8 only
	return IMAGEFORMAT_RGBA;
}

void VKDrv_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256])
{
}

int VKDrv_SumOfUsedImages()
{
	int total = 0;
	for (int i = 0; i < tr.numImages; i++) {
		const vkImage_t* vkImg = &g_vkImages[tr.images[i]->index];
		if (vkImg->frameUsed == tr.frameCount) {
			total += vkImg->width * vkImg->height;
		}
	}
	return total;
}

void VKDrv_GfxInfo()
{
	ri.Printf(PRINT_ALL, "----- Vulkan 1.1 -----\n");
}

void VKDrv_Clear(unsigned long bits, const float* clearCol, unsigned long stencil, float depth)
{
}

void VKDrv_SetProjection(const float* projMatrix)
{
}

void VKDrv_GetProjection(float* projMatrix)
{
}

void VKDrv_SetModelView(const float* modelViewMatrix)
{
}

void VKDrv_GetModelView(float* modelViewMatrix)
{
}

void VKDrv_SetViewport(int left, int top, int width, int height)
{
}

void VKDrv_Flush()
{
}

void VKDrv_SetState(unsigned long stateMask)
{
}

void VKDrv_ResetState2D()
{
}

void VKDrv_ResetState3D()
{
}

void VKDrv_SetPortalRendering(qboolean enabled, const float* flipMatrix, const float* plane)
{
}

void VKDrv_SetDepthRange(float minRange, float maxRange)
{
}

void VKDrv_SetDrawBuffer(int buffer)
{
}

void VKDrv_EndFrame()
{
}

void VKDrv_MakeCurrent(qboolean current)
{
}

void VKDrv_ShadowSilhouette(const float* edges, int edgeCount)
{
}

void VKDrv_ShadowFinish()
{
}

void VKDrv_DrawSkyBox(const skyboxDrawInfo_t* skybox, const float* eye_origin, const float* colorTint)
{
}

void VKDrv_DrawBeam(const image_t* image, const float* color, const vec3_t startPoints[], const vec3_t endPoints[],
	int segs)
{
}

void VKDrv_DrawStageGeneric(const shaderCommands_t* input)
{
}

void VKDrv_DrawStageVertexLitTexture(const shaderCommands_t* input)
{
}

void VKDrv_DrawStageLightmappedMultitexture(const shaderCommands_t* input)
{
}

void VKDrv_DebugDrawAxis()
{
}

void VKDrv_DebugDrawTris(const shaderCommands_t* input)
{
}

void VKDrv_DebugDrawNormals(const shaderCommands_t* input)
{
}

void VKDrv_DebugSetOverdrawMeasureEnabled(qboolean enabled)
{
}

void VKDrv_DebugSetTextureMode(const char* mode)
{
}

void VKDrv_DebugDrawPolygon(int color, int numPoints, const float* points)
{
}
