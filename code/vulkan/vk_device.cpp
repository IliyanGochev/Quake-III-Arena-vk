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
uint32_t					g_vkGraphicsFamilyIndex			= 0;

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

void VkCreateSurface() {
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType		= VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hinstance	= VKWnd_GetInstance();
	createInfo.hwnd			= VKWnd_GetWindowHandle();	

	VkCheckError( vkCreateWin32SurfaceKHR(g_vkInstance, &createInfo, nullptr, &g_vkSurface));
}

void VkDestroySurface() {
	vkDestroySurfaceKHR(g_vkInstance, g_vkSurface, nullptr);
	g_vkSurface = VK_NULL_HANDLE;
}

void VkCreateSemaphores() {
	// TODO: Create VkSemaphoreCreateInfo depending on the needs?
	
}

void VkDestroySemaphores() {
	// TODO: Destroy the semaphores
}

void SetupDebugLayers()
{
	instanceEnabledLayers.push_back("VK_LAYER_LUNARG_standard_validation");
	instanceEnabledExtensionLayers.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	instanceEnabledExtensionLayers.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	instanceEnabledExtensionLayers.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

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
	
}

void VkDestroyQueryPool() {
	
}

void VkCreateCommandPool() {
	
}

void VkDestroyCommandPool() {
	
}

void VkCreateCommandBuffer() {
	
}

void VkDestroyCommandBuffer() {
	
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

void VkCreateSwapChain() {
	VkSwapchainCreateInfoKHR swapchainCreateInfo{};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = g_vkSurface;
	swapchainCreateInfo.minImageCount = 2; // Double buffering


	VkCheckError(vkCreateSwapchainKHR(g_vkDevice, &swapchainCreateInfo, nullptr, &g_vkSwapchain));
}
void VkDestroySwapChain() {
	vkDestroySwapchainKHR(g_vkDevice, g_vkSwapchain, nullptr);
}

void VkCreateInstance()
{
	SetupDebugLayers();	
	
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

	// CreateSemaphores();

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
	VkDestroySemaphores();	
	VkDestroySwapChain();
	VkDestroySurface();
	VkDestroyDevice();
	DestroyVkDebugCallback();

	vkDestroyInstance(g_vkInstance, nullptr);
	g_vkInstance = nullptr;
}
