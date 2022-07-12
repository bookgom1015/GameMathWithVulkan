#pragma once

#include "LowRenderer.h"

struct Vertex {
	glm::vec3 mPos;
	glm::vec3 mColor;
	glm::vec2 mTexCoord;

	static VkVertexInputBindingDescription GetBindingDescription();
	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();

	bool operator==(const Vertex& other) const {
		return mPos == other.mPos && mColor == other.mColor && mTexCoord == other.mTexCoord;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.mPos) ^
				(hash<glm::vec3>()(vertex.mColor) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.mTexCoord) << 1);
		}
	};
}

class Renderer : LowRenderer {
protected:
	struct UniformBufferObject {
		alignas(16) glm::mat4 mModel;
		alignas(16) glm::mat4 mView;
		alignas(16) glm::mat4 mProj;
	};

public:
	Renderer() = default;
	virtual ~Renderer();

private:
	Renderer(const Renderer& inRef) = delete;
	Renderer(Renderer&& inRVal) = delete;
	Renderer& operator=(const Renderer& inRef) = delete;
	Renderer& operator=(Renderer&& inRVal) = delete;

public:
	virtual bool Initialize(int inClientWidth, int inClientHeight, GLFWwindow* pWnd) override;
	virtual void CleanUp() override;

	virtual void OnResize(int inClientWidth, int inClientHeight) override;

	bool AddModel(const std::string& inFilePath);

	bool Update(const GameTimer& gt);
	bool Draw();

protected:
	virtual bool RecreateSwapChain() override;
	virtual void CleanUpSwapChain() override;

private:
	bool UpdateUniformBuffer(const GameTimer& gt);

	bool CreateVertexBuffer(const std::string& inFilePath);
	bool CreateIndexBuffer(const std::string& inFilePath);

	bool CreateImageViews();
	bool CreateRenderPass();
	bool CreateCommandPool();
	bool CreateColorResources();
	bool CreateDepthResources();
	bool CreateFramebuffers();
	bool CreateTextureImages();
	bool CreateTextureImageViews();
	bool CreateTextureSamplers();
	bool CreateDescriptorSetLayout();
	bool CreateGraphicsPipeline();
	bool CreateUniformBuffers();
	bool CreateDescriptorPool();
	bool CreateDescriptorSets();
	bool CreateCommandBuffers();
	bool CreateSyncObjects();

private:
	bool bIsCleanedUp = false;

protected:
	std::vector<VkImageView> mSwapChainImageViews;
	VkRenderPass mRenderPass;
	std::vector<VkFramebuffer> mSwapChainFramebuffers;

	VkCommandPool mCommandPool;
	std::vector<VkCommandBuffer> mCommandBuffers;

	VkImage mColorImage;
	VkDeviceMemory mColorImageMemory;
	VkImageView mColorImageView;

	VkImage mDepthImage;
	VkDeviceMemory mDepthImageMemory;
	VkImageView mDepthImageView;

	std::uint32_t mMipLevels;
	VkImage mTextureImage;
	VkDeviceMemory mTextureImageMemory;
	VkImageView mTextureImageView;

	VkSampler mTextureSampler;

	VkDescriptorSetLayout mDescriptorSetLayout;
	VkDescriptorPool mDescriptorPool;
	std::vector<VkDescriptorSet> mDescriptorSets;

	VkPipelineLayout mPipelineLayout;
	VkPipeline mGraphicsPipeline;

	std::string mModelFilePath;
	std::unordered_map<std::string, std::unordered_map<Vertex, std::uint32_t>> mObjUniqueVerticesSet;
	std::unordered_map<std::string, std::vector<Vertex>> mObjVerticesSet;
	std::unordered_map<std::string, std::vector<std::uint32_t>> mObjIndicesSet;

	VkBuffer mObjVertexBuffer;
	VkDeviceMemory mObjVertexBufferMemory;

	VkBuffer mObjIndexBuffer;
	VkDeviceMemory mObjIndexBufferMemory;

	std::vector<VkBuffer> mUniformBuffers;
	std::vector<VkDeviceMemory> mUniformBufferMemories;

	std::vector<VkSemaphore> mImageAvailableSemaphores;
	std::vector<VkSemaphore> mRenderFinishedSemaphores;
	std::vector<VkFence> mInFlightFences;
	std::vector<VkFence> mImagesInFlight;
	std::uint32_t mCurentImageIndex = 0;
	size_t mCurrentFrame = 0;

	bool bFramebufferResized = false;
};