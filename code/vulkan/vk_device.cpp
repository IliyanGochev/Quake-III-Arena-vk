#include "../win32/win_vulkan.h"
#include "vk_common.h"
#include <vector>



// TODO: Cleanup into a state struct?
VkInstance					g_vkInstance					= nullptr;
VkDevice					g_vkDevice						= nullptr;
VkPhysicalDevice			g_vkPhysicaDevice				= nullptr;
VkPhysicalDeviceProperties	g_vkPhysicalDeviceProperties	= {};
VkSwapchainKHR				g_vkSwapchain					= {};
VkSurfaceKHR				g_vkSurface						= {};
VkSurfaceCapabilitiesKHR	g_vkSurfaceCapabilites			= {};
VkSurfaceFormatKHR			g_vkSurfaceFormat				= {};
uint32_t					g_vkGraphicsFamilyIndex			= 0;
VkFence						g_vkSwapchainFence				= VK_NULL_HANDLE;
VkSemaphore					g_vkSemaphore					= VK_NULL_HANDLE;
VkCommandPool				g_vkCommandPool					= VK_NULL_HANDLE;
VkQueryPool					g_vkQueryPool					= VK_NULL_HANDLE;
VkCommandBuffer				g_vkCmdBuffer					= VK_NULL_HANDLE;

std::vector<const char*>	instanceEnabledLayers			= {};
std::vector<const char*>	instanceEnabledExtensionLayers	= {};
std::vector<const char*>	deviceEnabledLayers				= {};
std::vector<const char*>	deviceEnabledExtensionLayers	= {};

VkDebugReportCallbackEXT	debugPrintHandle				= {};
VkDebugReportCallbackCreateInfoEXT debugReportCreateCallbackInfo{};

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
	// TODO: Check the other fields of createInfo
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
}

void VkDestroyDevice()
{
	vkDestroyDevice(g_vkDevice, nullptr);
	g_vkDevice = nullptr;
}
// Double buffering
uint32_t g_vkSwapchainImageCount = 2;

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
	swapchainCreateInfo.minImageCount			= g_vkSwapchainImageCount; 
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
}
void VkDestroySwapChain() {
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
	VkDestroySwapChain();
	VkDestroySurface();
	VkDestroyDevice();
	DestroyVkDebugCallback();

	vkDestroyInstance(g_vkInstance, nullptr);
	g_vkInstance = nullptr;
}
