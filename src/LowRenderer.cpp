#include "LowRenderer.h"

namespace {
#ifdef _DEBUG
	const bool EnableValidationLayers = true;
#else
	const bool EnableValidationLayers = false;
#endif

	const std::vector<const char*> ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	bool CheckValidationLayersSupport() {
		std::uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

#ifdef _DEBUG
		WLogln(L"Available Layers:");
		for (const auto& layer : availableLayers) {
			Logln("\t ", layer.layerName);
		}
#endif

		std::vector<const char*> unsupportedLayers;
		bool status = true;

		for (auto layerName : ValidationLayers) {
			bool layerFound = false;

			for (const auto& layer : availableLayers) {
				if (std::strcmp(layerName, layer.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				unsupportedLayers.push_back(layerName);
				status = false;
			}
		}

		if (!status) {
			std::wstringstream wsstream;

			for (const auto& layerName : unsupportedLayers)
				wsstream << layerName << L' ';
			wsstream << L"is/are not supported";

			ReturnFalse(wsstream.str());
		}

		return true;
	}

	void GetRequiredExtensions(std::vector<const char*>& outExtensions) {
		std::uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

#ifdef _DEBUG
		WLogln(L"Required extensions by GLFW:");
		for (std::uint32_t i = 0; i < glfwExtensionCount; ++i) {
			Logln("\t ", glfwExtensions[i]);
		}
#endif

		outExtensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (EnableValidationLayers) {
			outExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData) {
		Logln(pCallbackData->pMessage);

		return VK_FALSE;
	}

	VkResult CreateDebugUtilsMessengerEXT(
			const VkInstance& instance,
			const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
			const VkAllocationCallbacks* pAllocator,
			VkDebugUtilsMessengerEXT* pDebugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		else return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	void DestroyDebugUtilsMessengerEXT(
			const VkInstance& instance,
			VkDebugUtilsMessengerEXT debugMessenger,
			const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) func(instance, debugMessenger, pAllocator);
	}

	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& inCreateInfo) {
		inCreateInfo = {};
		inCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		inCreateInfo.messageSeverity = 
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		inCreateInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		inCreateInfo.pfnUserCallback = DebugCallback;
		inCreateInfo.pUserData = nullptr;
	}

	bool SetUpDebugMessenger(const VkInstance& inInstance, VkDebugUtilsMessengerEXT& inDebugMessenger) {
		if (!EnableValidationLayers) {
			return true;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		PopulateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(inInstance, &createInfo, nullptr, &inDebugMessenger) != VK_SUCCESS) {
			ReturnFalse(L"Failed to create debug messenger");
		}

		return true;
	}

	bool CheckDeviceExtensionsSupport(const VkPhysicalDevice& inPhysicalDevice) {
		std::uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(inPhysicalDevice, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(inPhysicalDevice, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface) {
		SwapChainSupportDetails details = {};

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(inPhysicalDevice, inSurface, &details.Capabilities);

		std::uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(inPhysicalDevice, inSurface, &formatCount, nullptr);
		if (formatCount != 0) {
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(inPhysicalDevice, inSurface, &formatCount, details.Formats.data());
		}

		std::uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(inPhysicalDevice, inSurface, &presentModeCount, nullptr);
		if (presentModeCount != 0) {
			details.PresentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(inPhysicalDevice, inSurface, &presentModeCount, details.PresentModes.data());
		}

		return details;
	}

	bool IsDeviceSuitable(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface) {
		QueueFamilyIndices indices = FindQueueFamilies(inPhysicalDevice, inSurface);

		bool extensionSupported = CheckDeviceExtensionsSupport(inPhysicalDevice);

		bool swapChainAdequate = false;
		if (extensionSupported) {
			SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(inPhysicalDevice, inSurface);

			swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
		}

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(inPhysicalDevice, &supportedFeatures);

		return indices.IsComplete() && extensionSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
	}

	int RateDeviceSuitability(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface) {
		int score = 0;

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(inPhysicalDevice, &deviceProperties);

		WLogln(L"Physical device properties:");
		Logln("\t Device name: ", deviceProperties.deviceName);
		WLog(L"\t Device type: ");
		switch (deviceProperties.deviceType) {
		case VK_PHYSICAL_DEVICE_TYPE_OTHER:
			WLogln(L"Other type");
			score += 1000;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			WLogln(L"Integrated GPU");
			score += 500;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			WLogln(L"Discrete GPU");
			score += 250;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			WLogln(L"Virtual GPU");
			score += 100;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			WLogln(L"CPU type");
			score += 50;
			break;
		}
		{
			std::uint32_t variant = VK_API_VERSION_VARIANT(deviceProperties.driverVersion);
			std::uint32_t major = VK_API_VERSION_MAJOR(deviceProperties.driverVersion);
			std::uint32_t minor = VK_API_VERSION_MINOR(deviceProperties.driverVersion);
			std::uint32_t patch = VK_API_VERSION_PATCH(deviceProperties.driverVersion);
			WLogln(L"\t Device version: ", 
				std::to_wstring(variant), L".", 
				std::to_wstring(major), L".",
				std::to_wstring(minor), L".",
				std::to_wstring(patch));
		}
		{
			std::uint32_t variant = VK_API_VERSION_VARIANT(deviceProperties.apiVersion);
			std::uint32_t major = VK_API_VERSION_MAJOR(deviceProperties.apiVersion);
			std::uint32_t minor = VK_API_VERSION_MINOR(deviceProperties.apiVersion);
			std::uint32_t patch = VK_API_VERSION_PATCH(deviceProperties.apiVersion);
			WLogln(L"\t API version: ",
				std::to_wstring(variant), L".",
				std::to_wstring(major), L".",
				std::to_wstring(minor), L".",
				std::to_wstring(patch));
		}

		score += deviceProperties.limits.maxImageDimension2D;

		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(inPhysicalDevice, &deviceFeatures);

		if (!deviceFeatures.geometryShader || !IsDeviceSuitable(inPhysicalDevice, inSurface)) {
			WLogln(L"\t This device is not suitable");
			return 0;
		}

		return score;
	}

	VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice inPhysicalDevice) {
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(inPhysicalDevice, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
		if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
		if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
		if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
		if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
		if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;

		return VK_SAMPLE_COUNT_1_BIT;
	}

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& inAvailableFormats) {
		for (const auto& availableFormat : inAvailableFormats) {
			if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return inAvailableFormats[0];
	}

	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& inAvailablePresentMods) {
		for (const auto& availablePresentMode : inAvailablePresentMods) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D ChooseSwapExtent(GLFWwindow* pWnd, const VkSurfaceCapabilitiesKHR& inCapabilities) {
		if (inCapabilities.currentExtent.width != UINT32_MAX) {
			return inCapabilities.currentExtent;
		}
		
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(pWnd, &width, &height);

		VkExtent2D actualExtent = { static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) };

		actualExtent.width = std::max(inCapabilities.minImageExtent.width, std::min(inCapabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(inCapabilities.minImageExtent.height, std::min(inCapabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

std::uint32_t QueueFamilyIndices::GetGraphicsFamilyIndex() {
	return GraphicsFamily.value();
}

std::uint32_t QueueFamilyIndices::GetPresentFamilyIndex() {
	return PresentFamily.value();
}

bool QueueFamilyIndices::IsComplete() {
	return GraphicsFamily.has_value() && PresentFamily.has_value();
}

QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface) {
	QueueFamilyIndices indices = {};

	std::uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(inPhysicalDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(inPhysicalDevice, &queueFamilyCount, queueFamilies.data());

	std::uint32_t i = 0;
	for (const auto& queueFamiliy : queueFamilies) {
		if (queueFamiliy.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.GraphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(inPhysicalDevice, i, inSurface, &presentSupport);

		if (presentSupport) indices.PresentFamily = i;
		if (indices.IsComplete()) break;

		++i;
	}

	return indices;
}

LowRenderer::~LowRenderer() {
	if (!bIsCleanedUp) {
		CleanUp();
	}
}

bool LowRenderer::Initialize(int inClientWidth, int inClientHeight, GLFWwindow* pWnd) {
	mClientWidth = inClientWidth;
	mClientHeight = inClientHeight;
	mGLFWWindow = pWnd;

	CheckReturn(CreateInstance());
	CheckReturn(SetUpDebugMessenger(mInstance, mDebugMessenger));
	CheckReturn(CreateSurface());
	CheckReturn(SelectPhysicalDevice());
	CheckReturn(CreateLogicalDevice());
	CheckReturn(CreateSwapChain());

	return true;
}

void LowRenderer::CleanUp() {
	vkDestroyDevice(mDevice, nullptr);

	vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

	if (EnableValidationLayers)
		DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);

	vkDestroyInstance(mInstance, nullptr);

	bIsCleanedUp = true;
}

void LowRenderer::OnResize(int inClientWidth, int inClientHeight) {
	mClientWidth = inClientWidth;
	mClientHeight = inClientHeight;
}

bool LowRenderer::RecreateSwapChain() {
	CheckReturn(CreateSwapChain());

	return true;
}

void LowRenderer::CleanUpSwapChain() {
	vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
}

bool LowRenderer::CreateInstance() {
	if (EnableValidationLayers) CheckReturn(CheckValidationLayersSupport());

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "GameMath";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.pEngineName = "Game Math Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	std::uint32_t availableExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

	std::vector<const char*> requiredExtensions;
	GetRequiredExtensions(requiredExtensions);

	std::vector<const char*> missingExtensions;
	bool status = true;

	for (const auto& requiredExt : requiredExtensions) {
		bool supported = false;

		for (const auto& availableExt : availableExtensions) {
			if (std::strcmp(requiredExt, availableExt.extensionName) == 0) {
				supported = true;
				break;
			}
		}

		if (!supported) {
			missingExtensions.push_back(requiredExt);
			status = false;
		}
	}

	if (!status) {
		WLogln(L"Upsupported extensions:");
		for (const auto& missingExt : missingExtensions) {
			Logln("\t ", missingExt);
		}

		return false;
	}

	createInfo.enabledExtensionCount = static_cast<std::uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (EnableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<std::uint32_t>(ValidationLayers.size());
		createInfo.ppEnabledLayerNames = ValidationLayers.data();

		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create instance");
	}

	return true;
}

bool LowRenderer::CreateSurface() {
	if (glfwCreateWindowSurface(mInstance, mGLFWWindow, nullptr, &mSurface) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create surface");
	}

	return true;
}

bool LowRenderer::SelectPhysicalDevice() {
	std::uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		ReturnFalse(L"Failed to find GPU(s) with Vulkan support");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

	std::multimap<int, VkPhysicalDevice> candidates;

	for (const auto& device : devices) {
		int score = RateDeviceSuitability(device, mSurface);
		candidates.insert(std::make_pair(score, device));
	}

	if (candidates.rbegin()->first > 0) {
		auto physicalDevice = candidates.rbegin()->second;
		mPhysicalDevice = physicalDevice;
		mMSAASamples = GetMaxUsableSampleCount(physicalDevice);
		WLogln(L"Multi sample counts: ", std::to_wstring(mMSAASamples));
	}
	else {
		ReturnFalse(L"Failed to find a suitable GPU");
	}

	return true;
}

bool LowRenderer::CreateLogicalDevice() {
	QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice, mSurface);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<std::uint32_t> uniqueQueueFamilies = {
		indices.GetGraphicsFamilyIndex(),
		indices.GetPresentFamilyIndex()
	};

	float queuePriority = 1.0f;
	for (std::uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.sampleRateShading = VK_TRUE;

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<std::uint32_t>(DeviceExtensions.size());
	createInfo.ppEnabledExtensionNames = DeviceExtensions.data();

	if (EnableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<std::uint32_t>(ValidationLayers.size());
		createInfo.ppEnabledLayerNames = ValidationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create logical device");
	}

	vkGetDeviceQueue(mDevice, indices.GetGraphicsFamilyIndex(), 0, &mGraphicsQueue);
	vkGetDeviceQueue(mDevice, indices.GetPresentFamilyIndex(), 0, &mPresentQueue);

	return true;
}

bool LowRenderer::CreateSwapChain() {
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(mPhysicalDevice, mSurface);

	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.Formats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.PresentModes);
	VkExtent2D extent = ChooseSwapExtent(mGLFWWindow, swapChainSupport.Capabilities);

	std::uint32_t imageCount = SwapChainImageCount;

	if (imageCount < swapChainSupport.Capabilities.minImageCount) {
		imageCount = swapChainSupport.Capabilities.minImageCount;
	}
	else if (swapChainSupport.Capabilities.maxImageCount > 0 && imageCount > swapChainSupport.Capabilities.maxImageCount) {
		imageCount = swapChainSupport.Capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = mSurface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice, mSurface);
	std::uint32_t queueFamilyIndices[] = {
		indices.GetGraphicsFamilyIndex(),
		indices.GetPresentFamilyIndex()
	};

	if (indices.GraphicsFamily != indices.PresentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 1;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.preTransform = swapChainSupport.Capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapChain) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create swap chain");
	}

	vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, nullptr);
	mSwapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, mSwapChainImages.data());

	mSwapChainImageFormat = surfaceFormat.format;
	mSwapChainExtent = extent;

	return true;
}
