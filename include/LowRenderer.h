#pragma once

#include "Common.h"

struct QueueFamilyIndices {
	std::optional<std::uint32_t> GraphicsFamily;
	std::optional<std::uint32_t> PresentFamily;

	std::uint32_t GetGraphicsFamilyIndex();
	std::uint32_t GetPresentFamilyIndex();
	bool IsComplete();
};

QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& inPhysicalDevice, const VkSurfaceKHR& inSurface);

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR Capabilities;
	std::vector<VkSurfaceFormatKHR> Formats;
	std::vector<VkPresentModeKHR> PresentModes;
};

class LowRenderer {
public:
	LowRenderer() = default;
	virtual ~LowRenderer();

private:
	LowRenderer(const LowRenderer& inRef) = delete;
	LowRenderer(LowRenderer&& inRVal) = delete;
	LowRenderer& operator=(const LowRenderer& inRef) = delete;
	LowRenderer& operator=(LowRenderer&& inRVal) = delete;

public:
	virtual bool Initialize(int inClientWidth, int inClientHeight, GLFWwindow* pWnd);
	virtual void CleanUp();

	virtual void OnResize(int inClientWidth, int inClientHeight);

protected:
	virtual bool RecreateSwapChain();
	virtual void CleanUpSwapChain();

private:
	bool CreateInstance();
	bool CreateSurface();
	bool SelectPhysicalDevice();
	bool CreateLogicalDevice();
	bool CreateSwapChain();

public:
	static const std::uint32_t SwapChainImageCount = 2;

protected:
	GLFWwindow* mGLFWWindow;
	int mClientWidth;
	int mClientHeight;

private:
	bool bIsCleanedUp = false;

protected:
	VkInstance mInstance;
	VkDebugUtilsMessengerEXT mDebugMessenger;
	VkSurfaceKHR mSurface;
	VkPhysicalDevice mPhysicalDevice;
	VkDevice mDevice;
	VkSwapchainKHR mSwapChain;
	std::vector<VkImage> mSwapChainImages;

	VkQueue mGraphicsQueue;
	VkQueue mPresentQueue;

	VkFormat mSwapChainImageFormat;
	VkExtent2D mSwapChainExtent;

	VkSampleCountFlagBits mMSAASamples = VK_SAMPLE_COUNT_1_BIT;
};