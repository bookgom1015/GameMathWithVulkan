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

enum RenderTypes {
	EOpaque = 0,
	EBlend,
	ENumTypes,
};

struct Mesh {
	VkBuffer VertexBuffer;
	VkDeviceMemory VertexBufferMemory;

	VkBuffer IndexBuffer;
	VkDeviceMemory IndexBufferMemory;

	std::unordered_map<Vertex, std::uint32_t> UniqueVertices;
	std::vector<Vertex> Vertices;
	std::vector<std::uint32_t> Indices;
};

struct RenderItem {
	std::vector<VkDescriptorSet> DescriptorSets;

	std::vector<VkBuffer> UniformBuffers;
	std::vector<VkDeviceMemory> UniformBufferMemories;

	std::string MeshName;
	std::string MatName;

	glm::vec3 Scale = glm::vec3(1.0f);
	glm::fquat Quat = glm::quat(0.0f, 0.0f, 0.0f, 1.0f);
	glm::vec3 Pos = glm::vec3(0.0f, 0.0f, 0.0f);
};

struct Material {
	VkImage TextureImage;
	VkDeviceMemory TextureImageMemory;
	VkImageView TextureImageView;
	VkSampler TextureSampler;

	std::uint32_t MipLevels;
};

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

	bool AddModel(
		const std::string& inFilePath, 
		const std::string& inTexFilePath,
		const std::string& inName, 
		RenderTypes inType = RenderTypes::EOpaque,
		bool bFlipped = false,
		glm::vec3 inScale = glm::vec3(1.0f),
		glm::fquat inQuat = glm::fquat(0.0f, 0.0f, 0.0f, 1.0f),
		glm::vec3 inPos = glm::vec3(0.0f));

	bool UpdateCamera(const glm::vec3& inPos, const glm::vec3& inTarget);
	bool UpdateModel(
		const std::string& inName,
		RenderTypes inType,
		glm::vec3 inScale = glm::vec3(1.0f),
		glm::fquat inQuat = glm::fquat(0.0f, 0.0f, 0.0f, 1.0f),
		glm::vec3 inPos = glm::vec3(0.0f));

	bool Update(const GameTimer& gt);
	bool Draw();

protected:
	virtual bool RecreateSwapChain() override;
	virtual void CleanUpSwapChain() override;

private:
	bool AddTexture(const std::string& inFilePath);

	bool UpdateUniformBuffer(const GameTimer& gt);
	bool UpdateDescriptorSet();

	bool CreateVertexBuffer(Mesh* ioMesh);
	bool CreateIndexBuffer(Mesh* ioMesh);
	bool CreateUniformBuffers(RenderItem* inRItem);
	bool RecreateUniformBuffers();
	bool CreateDescriptorSets(RenderItem* inRItem);
	bool RecreateDescriptorSets();
	bool CreateTextureImage(int inTexWidth, int inTexHeight, void* pData, Material* ioMaterial);
	bool CreateTextureImageView(Material* ioMaterial);
	bool CreateTextureSampler(Material* ioMaterial);

	bool CreateImageViews();
	bool CreateRenderPass();
	bool CreateCommandPool();
	bool CreateColorResources();
	bool CreateDepthResources();
	bool CreateFramebuffers();
	bool CreateDescriptorSetLayout();
	bool CreateGraphicsPipeline();
	bool CreateDescriptorPool();
	bool CreateCommandBuffers();
	bool CreateSyncObjects();

private:
	bool bIsCleanedUp = false;

public:
	VkFormat ImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

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

	std::unordered_map<std::string, std::unique_ptr<Mesh>> mMeshes;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;

	VkDescriptorSetLayout mDescriptorSetLayout;
	VkDescriptorPool mDescriptorPool;

	VkPipelineLayout mPipelineLayout;
	VkPipeline mGraphicsPipeline;

	std::string mModelFilePath;

	std::vector<std::unique_ptr<RenderItem>> mRItems;
	std::unordered_map<std::string, RenderItem*> mRItemRefs[RenderTypes::ENumTypes];
	std::multimap<float, RenderItem*> mOrderedRItemRefs;

	std::vector<VkSemaphore> mImageAvailableSemaphores;
	std::vector<VkSemaphore> mRenderFinishedSemaphores;
	std::vector<VkFence> mInFlightFences;
	std::vector<VkFence> mImagesInFlight;
	std::uint32_t mCurentImageIndex = 0;
	size_t mCurrentFrame = 0;

	glm::vec3 mCameraPos = ZeroVector;
	glm::vec3 mCameraTarget = ForwardVector;

	bool bFramebufferResized = false;
	bool bNeedToUpdateDescriptorSet = false;
};