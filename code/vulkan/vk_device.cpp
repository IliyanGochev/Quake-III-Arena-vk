#include "../win32/win_vulkan.h"
#include "vk_common.h"
#include <vector>



// TODO: Cleanup into a state struct?
VkInstance							g_vkInstance					= nullptr;
// Device-specific
VkDevice							g_vkDevice						= nullptr;
VkPhysicalDevice					g_vkPhysicaDevice				= nullptr;
VkPhysicalDeviceProperties			g_vkPhysicalDeviceProperties	{};
VkPhysicalDeviceMemoryProperties	g_vkPhysicalDeviceMemoryProperties{};
uint32_t							g_vkGraphicsFamilyIndex			= 0;
uint32_t							g_vkPresenterFamilyIndex		= 0;
// Swapchain-specific
VkSwapchainKHR						g_vkSwapchain					{};
VkFence								g_vkSwapchainFence				= VK_NULL_HANDLE;
uint32_t							g_vkSwapchainImageCount			= 2; // Double buffering
std::vector<VkImage>				g_vkSwapchainImages				{};
std::vector<VkImageView>			g_vkSwapchainImageViews			{};
// Surface-specific
VkSurfaceKHR						g_vkSurface						{};
VkSurfaceCapabilitiesKHR			g_vkSurfaceCapabilites			{};
VkSurfaceFormatKHR					g_vkSurfaceFormat				{};

// Sync-primitives
VkSemaphore							g_vkSemaphore					= VK_NULL_HANDLE;
VkCommandPool						g_vkCommandPool					= VK_NULL_HANDLE;
// Commands & Queries
VkQueryPool							g_vkQueryPool					= VK_NULL_HANDLE;
VkCommandBuffer						g_vkCmdBuffer					= VK_NULL_HANDLE;
VkQueue								g_vkQueue						= VK_NULL_HANDLE;
VkPipeline							g_vkPipeline					= VK_NULL_HANDLE;
VkRenderPass						g_vkRenderPass					= VK_NULL_HANDLE;

// Depth & Stencil
VkImage								g_vkDepthStencilImage			= VK_NULL_HANDLE;
VkImageView							g_vkDepthStencilImageView		= VK_NULL_HANDLE;
VkDeviceMemory						g_vkDepthStencilImageMemory		= VK_NULL_HANDLE;
VkFormat							g_vkDepthStencilFormat			= VK_FORMAT_UNDEFINED;
bool								g_vkStencilAvailable			= false;

std::vector<const char*>			instanceEnabledLayers			= {};
std::vector<const char*>			instanceEnabledExtensionLayers	= {};
std::vector<const char*>			deviceEnabledLayers				= {};
std::vector<const char*>			deviceEnabledExtensionLayers	= {};

VkDebugReportCallbackEXT			debugPrintHandle				= {};
VkDebugReportCallbackCreateInfoEXT	debugReportCreateCallbackInfo{};

VKAPI_ATTR VkBool32 VKAPI_CALL
VkDebugCallback(
	VkDebugReportFlagsEXT		flags,
	VkDebugReportObjectTypeEXT	objectType,
	uint64_t					sourceObject,
	size_t						location,
	int32_t						messageCode,
	const char*					layerPrefix,
	const char*					message,
	void*						userData
)
{
	if(flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
	{
		ri.Printf(PRINT_ALL, "VKDBG_INFO: '%s' \n" , message);
	}
	if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
	{
		ri.Printf(PRINT_WARNING, "VKDBG_WARNING: '%s' \n", message);
	}
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
	{
		ri.Printf(PRINT_DEVELOPER, "VKDBG_PERF:'%s' \n", message);
	}
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
	{
		//ri.Printf(PRINT_ERROR, "VKDBG_ERROR: '%s' \n", message);
		ri.Error(ERR_FATAL, "VKDBG_ERROR: '%s' \n", message);
	}
	if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
	{
		ri.Printf(PRINT_DEVELOPER, "VKDBG_DEBUG: '%s' \n", message);
	}
	
	//MessageBox();
	return false;
}

uint32_t surfaceSizeX = 0;
uint32_t surfaceSizeY = 0;

uint32_t FindMemoryTypeIndex(const VkMemoryRequirements* memoryRequirements, VkMemoryPropertyFlags requiredMemoryProperties) {
	for (uint32_t i = 0; g_vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++i) {
		if (memoryRequirements->memoryTypeBits & (1 << i)) {
			// TODO: think more about this checks
			if ((g_vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & requiredMemoryProperties) == requiredMemoryProperties) {
				return i;
				break;
			}
		}
	}
	assert(0 && "No sutable memory type found");
	return UINT32_MAX;
}

void VkCreateSurface() {
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType		= VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hinstance	= VKWnd_GetInstance();
	createInfo.hwnd			= VKWnd_GetWindowHandle();	

	VkCheckError( vkCreateWin32SurfaceKHR(g_vkInstance, &createInfo, nullptr, &g_vkSurface));

	VkBool32 supported;
	VkCheckError(vkGetPhysicalDeviceSurfaceSupportKHR(g_vkPhysicaDevice, g_vkGraphicsFamilyIndex, g_vkSurface, &supported));

	if (!supported) {
		assert(0 && "Surface format not supported!");
	}

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vkPhysicaDevice, g_vkSurface, &g_vkSurfaceCapabilites);
	if (g_vkSurfaceCapabilites.currentExtent.width < UINT32_MAX) {
		surfaceSizeX = g_vkSurfaceCapabilites.currentExtent.width;
		surfaceSizeY = g_vkSurfaceCapabilites.currentExtent.height;
	}
	{
		// TODO: Move it out?
		uint32_t formatCount = 0;
		
		vkGetPhysicalDeviceSurfaceFormatsKHR(g_vkPhysicaDevice, g_vkSurface, &formatCount, nullptr);
		if (formatCount == 0) {
			assert(0 && "Surface formats missing");
			exit(-1);
		}

		std::vector<VkSurfaceFormatKHR> formats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(g_vkPhysicaDevice, g_vkSurface, &formatCount, formats.data());
		if (formats[0].format == VK_FORMAT_UNDEFINED) {
			g_vkSurfaceFormat.format		= VK_FORMAT_B8G8R8A8_UNORM;
			g_vkSurfaceFormat.colorSpace	= VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		}else {
			g_vkSurfaceFormat = formats[0];
		}
	}
}

void VkDestroySurface() {
	vkDestroySurfaceKHR(g_vkInstance, g_vkSurface, nullptr);
	g_vkSurface = VK_NULL_HANDLE;
}

void CheckRequiredExtensionsAvailability() {
	
};

void EnableExtensionLayers() {

	CheckRequiredExtensionsAvailability();
	instanceEnabledExtensionLayers.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	instanceEnabledExtensionLayers.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	instanceEnabledExtensionLayers.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

	deviceEnabledExtensionLayers.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

void SetupDebugLayers()
{
	instanceEnabledLayers.push_back("VK_LAYER_LUNARG_standard_validation");
	deviceEnabledLayers.push_back("VK_LAYER_LUNARG_standard_validation");

	debugReportCreateCallbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	debugReportCreateCallbackInfo.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
		VK_DEBUG_REPORT_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_ERROR_BIT_EXT |
		VK_DEBUG_REPORT_DEBUG_BIT_EXT;
	debugReportCreateCallbackInfo.pfnCallback = &VkDebugCallback;	

}
PFN_vkCreateDebugReportCallbackEXT	fvkCreateDebugReportCallbackEXT		= nullptr;
PFN_vkDestroyDebugReportCallbackEXT	fvkDestroyDebugReportCallbackEXT	= nullptr;

void CreateVkDebugCallback()
{
	fvkCreateDebugReportCallbackEXT  = (PFN_vkCreateDebugReportCallbackEXT)	vkGetInstanceProcAddr(g_vkInstance, "vkCreateDebugReportCallbackEXT"); 
	fvkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(g_vkInstance, "vkDestroyDebugReportCallbackEXT");
	if (nullptr == fvkCreateDebugReportCallbackEXT || nullptr == fvkDestroyDebugReportCallbackEXT)
	{
		assert(0 && "Error obtaining ProcAddresses!");
		exit(-1);
	}
	fvkCreateDebugReportCallbackEXT(g_vkInstance, &debugReportCreateCallbackInfo, nullptr, &debugPrintHandle);	
}

void DestroyVkDebugCallback()
{
	fvkDestroyDebugReportCallbackEXT(g_vkInstance, debugPrintHandle, nullptr);
	debugPrintHandle = VK_NULL_HANDLE;
}

void VkCreateQueryPool() {
	VkQueryPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	createInfo.queryCount = 16; // TODO: Pull out	
	// TODO: do we need query pools for each frame?
	VkCheckError(vkCreateQueryPool(g_vkDevice, &createInfo, nullptr, &g_vkQueryPool));
}

void VkDestroyQueryPool() {
	vkDestroyQueryPool(g_vkDevice, g_vkQueryPool, nullptr);
}

void VkCreateCommandPool() {
	VkCommandPoolCreateInfo createInfo{};
	createInfo.sType			= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = g_vkGraphicsFamilyIndex;
	createInfo.flags			= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	
	VkCheckError( vkCreateCommandPool(g_vkDevice, &createInfo, nullptr, &g_vkCommandPool));
}

void VkDestroyCommandPool() {
	vkDestroyCommandPool(g_vkDevice, g_vkCommandPool, nullptr);
}

void VkCreateCommandBuffer() {
	VkCommandBufferAllocateInfo cbAllocateInfo{};
	cbAllocateInfo.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocateInfo.commandPool			= g_vkCommandPool;
	cbAllocateInfo.commandBufferCount	= 1;
	cbAllocateInfo.level				= VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCheckError(vkAllocateCommandBuffers(g_vkDevice, &cbAllocateInfo, &g_vkCmdBuffer));
}

void VkDestroyCommandBuffer() {
	vkFreeCommandBuffers(g_vkDevice, g_vkCommandPool, 1, &g_vkCmdBuffer);
}


void VkCreateDevice()
{
	// Get the count of GPUs
	{
		uint32_t devicesCount = 0;
		vkEnumeratePhysicalDevices(g_vkInstance, &devicesCount, nullptr);
		std::pmr::vector<VkPhysicalDevice> deviceList(devicesCount);
		vkEnumeratePhysicalDevices(g_vkInstance, &devicesCount, deviceList.data());
		// TODO: Better selection of GPU
		if (!deviceList.empty()) { g_vkPhysicaDevice = deviceList[0]; }
		else
		{
			assert(0 && "No physical device found!");
			exit(-1);
		}
		vkGetPhysicalDeviceProperties(g_vkPhysicaDevice, &g_vkPhysicalDeviceProperties);
		vkGetPhysicalDeviceMemoryProperties(g_vkPhysicaDevice, &g_vkPhysicalDeviceMemoryProperties);
	}

	//N.B! For now we will use 1 Queue!
	{
		uint32_t queuesCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(g_vkPhysicaDevice, &queuesCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamiliesList(queuesCount);
		vkGetPhysicalDeviceQueueFamilyProperties(g_vkPhysicaDevice, &queuesCount, queueFamiliesList.data());

		bool found = false;
		for (size_t i = 0; i < queueFamiliesList.size(); ++i)
		{
			if (queueFamiliesList[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				found = true;
				g_vkGraphicsFamilyIndex = i;
			}
		}
		if (!found)
		{
			assert(0 && "Vulkan ERROR: Queue family supporting graphics not found");
			exit(-1);
		}
	}

	float queuePriorities[]{ 1.0f };
	VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.queueCount = 1;
	deviceQueueCreateInfo.queueFamilyIndex = g_vkGraphicsFamilyIndex;
	deviceQueueCreateInfo.pQueuePriorities = queuePriorities;

	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
	deviceCreateInfo.enabledLayerCount = deviceEnabledLayers.size();
	deviceCreateInfo.ppEnabledLayerNames = deviceEnabledLayers.data();
	deviceCreateInfo.enabledExtensionCount = deviceEnabledExtensionLayers.size();
	deviceCreateInfo.ppEnabledExtensionNames = deviceEnabledExtensionLayers.data();

	VkCheckError(vkCreateDevice(g_vkPhysicaDevice, &deviceCreateInfo, nullptr, &g_vkDevice));

	vkGetDeviceQueue(g_vkDevice, g_vkGraphicsFamilyIndex, 0, &g_vkQueue);
}

void VkDestroyDevice()
{
	vkDestroyDevice(g_vkDevice, nullptr);
	g_vkDevice = nullptr;
}


void VkCreateSwapChain() {

	if (g_vkSwapchainImageCount > g_vkSurfaceCapabilites.maxImageCount) 
		g_vkSwapchainImageCount = g_vkSurfaceCapabilites.maxImageCount;
	if (g_vkSwapchainImageCount < g_vkSurfaceCapabilites.minImageCount + 1)
		g_vkSwapchainImageCount = g_vkSurfaceCapabilites.minImageCount + 1;

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	{
		uint32_t pmCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(g_vkPhysicaDevice, g_vkSurface, &pmCount, nullptr);
		std::vector<VkPresentModeKHR> presentModes(pmCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(g_vkPhysicaDevice, g_vkSurface, &pmCount, presentModes.data());

		// V-Sync by default
		for (auto& m: presentModes) {
			if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
				presentMode = m;
				break;
			}
		}
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface					= g_vkSurface;
	swapchainCreateInfo.minImageCount			= g_vkSwapchainImageCount; // TODO: Change to SMP_FRAMES?
	swapchainCreateInfo.imageFormat				= g_vkSurfaceFormat.format;
	swapchainCreateInfo.imageColorSpace			= g_vkSurfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent				= g_vkSurfaceCapabilites.currentExtent;
	swapchainCreateInfo.imageArrayLayers		= 1; // 2 for stereoscopic
	swapchainCreateInfo.imageUsage				= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	swapchainCreateInfo.imageSharingMode		= VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.preTransform			= VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.compositeAlpha			= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // TODO: check for more info
	swapchainCreateInfo.presentMode				= presentMode;
	swapchainCreateInfo.clipped					= VK_TRUE;	
	swapchainCreateInfo.oldSwapchain			= VK_NULL_HANDLE;

	VkCheckError(vkCreateSwapchainKHR(g_vkDevice, &swapchainCreateInfo, nullptr, &g_vkSwapchain));

	VkCheckError(vkGetSwapchainImagesKHR(g_vkDevice, g_vkSwapchain, &g_vkSwapchainImageCount, nullptr));
	assert(g_vkSwapchainImageCount && "vkGetSwapchainImagesKHR returned zero image count");
	g_vkSwapchainImages.resize(g_vkSwapchainImageCount);
	VkCheckError(vkGetSwapchainImagesKHR(g_vkDevice, g_vkSwapchain, &g_vkSwapchainImageCount, g_vkSwapchainImages.data()));

	g_vkSwapchainImageViews.resize(g_vkSwapchainImageCount);
	for (uint32_t i = 0; i< g_vkSwapchainImageCount; i++) {
		VkImageViewCreateInfo createInfo{};
		createInfo.sType							= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image							= g_vkSwapchainImages[i];
		createInfo.viewType							= VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format							= g_vkSurfaceFormat.format;
		createInfo.components.r						= VK_COMPONENT_SWIZZLE_R;
		createInfo.components.g						= VK_COMPONENT_SWIZZLE_G;
		createInfo.components.b						= VK_COMPONENT_SWIZZLE_B;
		createInfo.components.a						= VK_COMPONENT_SWIZZLE_A;
		createInfo.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel	= 0;
		createInfo.subresourceRange.levelCount		= 1;
		createInfo.subresourceRange.baseArrayLayer	= 0;
		createInfo.subresourceRange.layerCount		= 1;

		VkCheckError(vkCreateImageView(g_vkDevice, &createInfo, nullptr, &g_vkSwapchainImageViews[i]));
	}
}
void VkDestroySwapChain() {
	for (auto iv : g_vkSwapchainImageViews) {
		vkDestroyImageView(g_vkDevice, iv, nullptr);
	}
	vkDestroySwapchainKHR(g_vkDevice, g_vkSwapchain, nullptr);
}

void CreateSyncPrimitives() {
	{
		VkFenceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VkCheckError(vkCreateFence(g_vkDevice, &createInfo, nullptr, &g_vkSwapchainFence));
	}
	{
		VkSemaphoreCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkCheckError(vkCreateSemaphore(g_vkDevice, &createInfo, nullptr, &g_vkSemaphore));
	}
}

void VkDestroySyncPrimitives() {
	vkDestroySemaphore(g_vkDevice, g_vkSemaphore, nullptr);
	vkDestroyFence(g_vkDevice, g_vkSwapchainFence, nullptr);
}

void VkCreateRenderPass() {
	VkAttachmentDescription attachments[2];

	// Depth
	attachments[0].flags			= 0;
	attachments[0].format			= g_vkDepthStencilFormat;
	attachments[0].samples			= VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp			= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	// Color
	attachments[1].flags			= 0;
	attachments[1].format			= g_vkSurfaceFormat.format;
	attachments[1].samples			= VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR; // Could be don't care
	attachments[1].storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference subpassDepthAttachmentRef{};
	subpassDepthAttachmentRef.attachment	= 0;
	subpassDepthAttachmentRef.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference subpassColorAttachmentRef{};
	subpassColorAttachmentRef.attachment	= 1;
	subpassColorAttachmentRef.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount	= 0;
	subpass.pInputAttachments		= nullptr;
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &subpassColorAttachmentRef;
	subpass.pDepthStencilAttachment = &subpassDepthAttachmentRef;
	
	VkRenderPassCreateInfo createInfo{};
	createInfo.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount		= 2;
	createInfo.pAttachments			= attachments;
	createInfo.subpassCount			= 1;
	createInfo.pSubpasses			= &subpass;

	VkCheckError( vkCreateRenderPass(g_vkDevice, &createInfo, nullptr, &g_vkRenderPass));
}

void VkDestroyRenderPas() {
	vkDestroyRenderPass(g_vkDevice, g_vkRenderPass, nullptr);
}


void VkCreateDepthStencilImage() {

	{
		{
			std::vector<VkFormat> expectedFormats{
				VK_FORMAT_D32_SFLOAT_S8_UINT,
				VK_FORMAT_D24_UNORM_S8_UINT,
				VK_FORMAT_D16_UNORM_S8_UINT,
				VK_FORMAT_D32_SFLOAT,
				VK_FORMAT_D16_UNORM
			};

			for (auto f : expectedFormats) {
				VkFormatProperties formatProperties{};
				vkGetPhysicalDeviceFormatProperties(g_vkPhysicaDevice, f, &formatProperties);

				if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
					g_vkDepthStencilFormat = f;
					break;
				}
			}
			if (g_vkDepthStencilFormat == VK_FORMAT_UNDEFINED) {
				assert(0 && "Depth stencil format not selected");
				std::exit(-1);
			}
			if (g_vkDepthStencilFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
				g_vkDepthStencilFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
				g_vkDepthStencilFormat == VK_FORMAT_D16_UNORM_S8_UINT) {
				g_vkStencilAvailable = true;
			}
		}


		VkImageCreateInfo createInfo{};
		createInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		createInfo.flags					=  0;
		createInfo.imageType				= VK_IMAGE_TYPE_2D;
		createInfo.format					= g_vkDepthStencilFormat;
		createInfo.extent.width				= g_vkSurfaceCapabilites.currentExtent.width;
		createInfo.extent.height			= g_vkSurfaceCapabilites.currentExtent.height;
		createInfo.extent.depth				= 1;
		createInfo.mipLevels				= 1;
		createInfo.arrayLayers				= 1;
		createInfo.samples					= VK_SAMPLE_COUNT_1_BIT;
		createInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;
		createInfo.usage					= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		createInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount	= VK_QUEUE_FAMILY_IGNORED;
		createInfo.pQueueFamilyIndices		= nullptr;
		createInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;

		VkCheckError(vkCreateImage(g_vkDevice, &createInfo, nullptr, &g_vkDepthStencilImage));

		VkMemoryRequirements memoryRequirements{};
		vkGetImageMemoryRequirements(g_vkDevice, g_vkDepthStencilImage, &memoryRequirements);

		auto requiredMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		uint32_t memoryIndex = FindMemoryTypeIndex(&memoryRequirements, requiredMemoryProperties);

		VkMemoryAllocateInfo memoryAllocateInfo{};
		memoryAllocateInfo.sType			= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.allocationSize	= memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex	= memoryIndex;

		vkAllocateMemory(g_vkDevice, &memoryAllocateInfo, nullptr, &g_vkDepthStencilImageMemory);
		vkBindImageMemory(g_vkDevice, g_vkDepthStencilImage, g_vkDepthStencilImageMemory, 0);
	}

	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType							= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image							= g_vkDepthStencilImage;
		createInfo.viewType							= VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format							= g_vkDepthStencilFormat;
		createInfo.components.r						= VK_COMPONENT_SWIZZLE_R;
		createInfo.components.g						= VK_COMPONENT_SWIZZLE_G;
		createInfo.components.b						= VK_COMPONENT_SWIZZLE_B;
		createInfo.components.a						= VK_COMPONENT_SWIZZLE_A;
		createInfo.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_DEPTH_BIT | (g_vkStencilAvailable ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
		createInfo.subresourceRange.baseMipLevel	= 0;
		createInfo.subresourceRange.levelCount		= 1;
		createInfo.subresourceRange.baseArrayLayer	= 0;
		createInfo.subresourceRange.layerCount		= 1;

		VkCheckError(vkCreateImageView(g_vkDevice, &createInfo, nullptr, &g_vkDepthStencilImageView));
	}
}

void VkDestroyDepthStencilImage() {
	vkDestroyImageView(g_vkDevice, g_vkDepthStencilImageView, nullptr);
	vkFreeMemory(g_vkDevice, g_vkDepthStencilImageMemory, nullptr);
	vkDestroyImage(g_vkDevice, g_vkDepthStencilImage, nullptr);
}

void VkCreateInstance()
{
	SetupDebugLayers();	
	EnableExtensionLayers();

	VkApplicationInfo applicationInfo{};
	applicationInfo.sType				= VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pApplicationName	= "Quake 3 Arena (Vulkan)";
	applicationInfo.pEngineName			= "idTech 3.5";
	applicationInfo.engineVersion		= 1;
	applicationInfo.apiVersion			= VK_API_VERSION_1_1;

	

	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo			= &applicationInfo;
	instanceCreateInfo.enabledLayerCount		= instanceEnabledLayers.size();
	instanceCreateInfo.ppEnabledLayerNames		= instanceEnabledLayers.data();
	instanceCreateInfo.enabledExtensionCount	= instanceEnabledExtensionLayers.size();
	instanceCreateInfo.ppEnabledExtensionNames	= instanceEnabledExtensionLayers.data();
	instanceCreateInfo.pNext					= &debugReportCreateCallbackInfo;

	VkCheckError(vkCreateInstance(&instanceCreateInfo, nullptr, &g_vkInstance));
	
	CreateVkDebugCallback();

	VkCreateDevice();
	VkCreateSurface();

	VkCreateSwapChain();

	VkCreateDepthStencilImage();

	CreateSyncPrimitives();

	// Create Query Pool
	VkCreateQueryPool();

	// Create Command Pool
	VkCreateCommandPool();

	// Create Command Buffer
	VkCreateCommandBuffer();
}

void VkDestroyInstance()
{
	VkDestroyCommandBuffer();
	VkDestroyCommandPool();
	VkDestroyQueryPool();
	VkDestroySyncPrimitives();
	VkDestroyDepthStencilImage();
	VkDestroySwapChain();
	VkDestroySurface();
	VkDestroyDevice();
	DestroyVkDebugCallback();

	vkDestroyInstance(g_vkInstance, nullptr);
	g_vkInstance = nullptr;
}
