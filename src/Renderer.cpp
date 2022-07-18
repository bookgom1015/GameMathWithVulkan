#include "Renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace {
	VkCommandBuffer BeginSingleTimeCommands(const VkDevice& inDevice, const VkCommandPool& inCommandPool) {
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = inCommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(inDevice, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void EndSingleTimeCommands(const VkDevice& inDevice, const VkQueue& inQueue, const VkCommandPool& inCommandPool, VkCommandBuffer& ioCommandBuffer) {
		vkEndCommandBuffer(ioCommandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &ioCommandBuffer;

		vkQueueSubmit(inQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(inQueue);

		vkFreeCommandBuffers(inDevice, inCommandPool, 1, &ioCommandBuffer);
	}

	std::uint32_t FindMemoryType(const VkPhysicalDevice& inPhysicalDevice, std::uint32_t inTypeFilter, const VkMemoryPropertyFlags& inProperties) {
		std::uint32_t index = std::numeric_limits<std::uint32_t>::max();

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(inPhysicalDevice, &memProperties);

		for (std::uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
			if ((inTypeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & inProperties) == inProperties) {
				index = i;
				break;
			}
		}

		return index;
	}

	bool CreateBuffer(
			const VkPhysicalDevice& inPhysicalDevice,
			const VkDevice& inDevice,
			VkDeviceSize inSize,
			const VkBufferUsageFlags& inUsage,
			const VkMemoryPropertyFlags& inProperties,
			VkBuffer& outBuffer,
			VkDeviceMemory& outBufferMemory) {
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = inSize;
		bufferInfo.usage = inUsage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.flags = 0;

		if (vkCreateBuffer(inDevice, &bufferInfo, nullptr, &outBuffer) != VK_SUCCESS) {
			ReturnFalse(L"Failed to create vertex buffer");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(inDevice, outBuffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(inPhysicalDevice, memRequirements.memoryTypeBits, inProperties);

		if (vkAllocateMemory(inDevice, &allocInfo, nullptr, &outBufferMemory) != VK_SUCCESS) {
			ReturnFalse(L"Failed to allocate vertex buffer memory");
		}

		vkBindBufferMemory(inDevice, outBuffer, outBufferMemory, 0);

		return true;
	}

	void CopyBuffer(
			const VkDevice& inDevice,
			const VkQueue& inQueue,
			const VkCommandPool& inCommandPool,
			const VkBuffer& inSrcBuffer,
			const VkBuffer& inDstBuffer,
			VkDeviceSize inSize) {
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = inSize;
		vkCmdCopyBuffer(commandBuffer, inSrcBuffer, inDstBuffer, 1, &copyRegion);

		EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);
	}

	void CopyBufferToImage(
			const VkDevice& inDevice,
			const VkQueue& inQueue,
			const VkCommandPool& inCommandPool,
			const VkBuffer& inBuffer,
			const VkImage& inImage,
			std::uint32_t inWidth,
			std::uint32_t inHeight) {
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { inWidth, inHeight, 1 };

		vkCmdCopyBufferToImage(
			commandBuffer,
			inBuffer,
			inImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region);

		EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);
	}

	bool GenerateMipmaps(
			const VkPhysicalDevice& inPhysicalDevice,
			const VkDevice& inDevice,
			const VkQueue& inQueue,
			const VkCommandPool& inCommandPool,
			const VkImage& inImage,
			const VkFormat& inFormat,
			std::int32_t inTexWidth,
			std::int32_t inTexHeight,
			std::uint32_t inMipLevles) {
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(inPhysicalDevice, inFormat, &formatProperties);

		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			ReturnFalse(L"Texture image format does not support linear bliting");
		}

		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = inImage;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		std::int32_t mipWidth = inTexWidth;
		std::int32_t mipHeight = inTexHeight;

		for (std::uint32_t i = 1; i < inMipLevles; ++i) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit = {};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth >> 1 : 1, mipHeight > 1 ? mipHeight >> 1 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(
				commandBuffer,
				inImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				inImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth = mipWidth >> 1;
			if (mipHeight > 1) mipHeight = mipHeight >> 1;;
		}

		barrier.subresourceRange.baseMipLevel = inMipLevles - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);

		return true;
	}

	bool CreateImage(
		const VkPhysicalDevice& inPhysicalDevice,
			const VkDevice& inDevice,
			std::uint32_t inWidth,
			std::uint32_t inHeight,
			std::uint32_t inMipLevels,
			const VkSampleCountFlagBits& inNumSamples,
			const VkFormat& inFormat,
			const VkImageTiling& inTiling,
			const VkImageUsageFlags& inUsage,
			const VkMemoryPropertyFlags& inProperties,
			VkImage& outImage,
			VkDeviceMemory& outImageMemory) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<std::uint32_t>(inWidth);
		imageInfo.extent.height = static_cast<std::uint32_t>(inHeight);
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = inMipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = inFormat;
		imageInfo.tiling = inTiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = inUsage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = inNumSamples;
		imageInfo.flags = 0;

		if (vkCreateImage(inDevice, &imageInfo, nullptr, &outImage) != VK_SUCCESS) {
			ReturnFalse(L"Failed to create image");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(inDevice, outImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(inPhysicalDevice, memRequirements.memoryTypeBits, inProperties);

		if (vkAllocateMemory(inDevice, &allocInfo, nullptr, &outImageMemory) != VK_SUCCESS) {
			ReturnFalse(L"Failed to allocate texture image memory");
		}

		vkBindImageMemory(inDevice, outImage, outImageMemory, 0);

		return true;
	}

	bool CreateImageView(
			const VkDevice& inDevice,
			const VkImage& inImage,
			const VkFormat& inFormat,
			std::uint32_t inMipLevles,
			const VkImageAspectFlags& inAspectFlags,
			VkImageView& outImageView) {
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = inImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = inFormat;
		viewInfo.subresourceRange.aspectMask = inAspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = inMipLevles;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(inDevice, &viewInfo, nullptr, &outImageView) != VK_SUCCESS) {
			ReturnFalse(L"Failed to create texture image view");
		}

		return true;
	}

	VkFormat FindSupportedFormat(
			const VkPhysicalDevice& inPhysicalDevice,
			const std::vector<VkFormat>& inCandidates,
			const VkImageTiling& inTiling,
			const VkFormatFeatureFlags inFeatures) {
		for (VkFormat format : inCandidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(inPhysicalDevice, format, &props);

			if (inTiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & inFeatures) == inFeatures) {
				return format;
			}
			else if (inTiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & inFeatures) == inFeatures) {
				return format;
			}
		}

		return VK_FORMAT_UNDEFINED;
	}

	VkFormat FindDepthFormat(const VkPhysicalDevice& inPhysicalDevice) {
		return FindSupportedFormat(
			inPhysicalDevice,
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	bool HasStencilComponent(const VkFormat& inFormat) {
		return inFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || inFormat == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	bool TransitionImageLayout(
			const VkDevice& inDevice,
			const VkQueue& inQueue,
			const VkCommandPool& inCommandPool,
			const VkImage& inImage,
			const VkFormat& inFormat,
			const VkImageLayout& inOldLayout,
			const VkImageLayout& inNewLayout,
			std::uint32_t inMipLevels) {
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = inOldLayout;
		barrier.newLayout = inNewLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = inImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = inMipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (inNewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (HasStencilComponent(inFormat))
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		else {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		if (inOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && inNewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (inOldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && inNewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (inOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && inNewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else {
			ReturnFalse(L"Unsupported layout transition");
		}

		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);

		return true;
	}

	bool ReadFile(const std::string& inFilePath, std::vector<char>& outData) {
		std::ifstream file(inFilePath, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			std::wstringstream wsstream;
			wsstream << L"Failed to load file: " << inFilePath.c_str();
			ReturnFalse(wsstream.str().c_str());
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		Logln(inFilePath.c_str(), " loaded (bytes: ", std::to_string(fileSize), ")");

		outData.resize(fileSize);

		file.seekg(0);
		file.read(outData.data(), fileSize);
		file.close();

		return true;
	}

	bool CreateShaderModule(const VkDevice& inDevice, const std::vector<char>& inCode, VkShaderModule& outModule) {
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = inCode.size();
		createInfo.pCode = reinterpret_cast<const std::uint32_t*>(inCode.data());

		if (vkCreateShaderModule(inDevice, &createInfo, nullptr, &outModule) != VK_SUCCESS) {
			ReturnFalse(L"Failed to create shader module");
		}

		return true;
	}
}

VkVertexInputBindingDescription Vertex::GetBindingDescription() {
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::GetAttributeDescriptions() {
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, mPos);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, mColor);

	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, mTexCoord);
	return attributeDescriptions;
}

Renderer::~Renderer() {
	if (!bIsCleanedUp) {
		CleanUp();
	}
}

bool Renderer::Initialize(int inClientWidth, int inClientHeight, GLFWwindow* pWnd) {
	CheckReturn(LowRenderer::Initialize(inClientWidth, inClientHeight, pWnd));

	CheckReturn(CreateImageViews());
	CheckReturn(CreateRenderPass());
	CheckReturn(CreateCommandPool());
	CheckReturn(CreateColorResources());
	CheckReturn(CreateDepthResources());
	CheckReturn(CreateFramebuffers());
	CheckReturn(CreateDescriptorSetLayout());
	CheckReturn(CreateGraphicsPipeline());
	CheckReturn(CreateDescriptorPool());
	CheckReturn(CreateCommandBuffers());
	CheckReturn(CreateSyncObjects());

	return true;
}

void Renderer::CleanUp() {
	vkDeviceWaitIdle(mDevice);
	
	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
		vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
	}

	for (const auto& matPair : mMaterials) {
		const auto& mat = matPair.second;
		vkDestroySampler(mDevice, mat->TextureSampler, nullptr);

		vkDestroyImageView(mDevice, mat->TextureImageView, nullptr);

		vkDestroyImage(mDevice, mat->TextureImage, nullptr);
		vkFreeMemory(mDevice, mat->TextureImageMemory, nullptr);
	}

	for (const auto& meshPair : mMeshes) {
		const auto& mesh = meshPair.second;
		vkDestroyBuffer(mDevice, mesh->IndexBuffer, nullptr);
		vkFreeMemory(mDevice, mesh->IndexBufferMemory, nullptr);

		vkDestroyBuffer(mDevice, mesh->VertexBuffer, nullptr);
		vkFreeMemory(mDevice, mesh->VertexBufferMemory, nullptr);
	}

	vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
	
	CleanUpSwapChain();
	
	vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

	LowRenderer::CleanUp();

	bIsCleanedUp = true;
}

void Renderer::OnResize(int inClientWidth, int inClientHeight) {
	LowRenderer::OnResize(inClientWidth, inClientHeight);

	bFramebufferResized = true;
}

bool Renderer::AddModel(
		const std::string& inFilePath, 
		const std::string& inTexFilePath,
		const std::string& inName, 
		RenderTypes inType,
		bool bFlipped,
		glm::vec3 inScale, 
		glm::fquat inQuat,
		glm::vec3 inPos) {
	if (mMeshes.count(inFilePath) == 0) {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, inFilePath.c_str())) {
			std::wstringstream wsstream;
			wsstream << warn.c_str() << err.c_str();
			ReturnFalse(wsstream.str());
		}

		auto mesh = std::make_unique<Mesh>();

		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex = {};

				vertex.mPos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				float texY = attrib.texcoords[2 * index.texcoord_index + 1];
				vertex.mTexCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					bFlipped ? 1.0f - texY : texY,
				};

				vertex.mColor = { 1.0f, 1.0f, 1.0f };

				if (mesh->UniqueVertices.count(vertex) == 0) {
					mesh->UniqueVertices[vertex] = static_cast<std::uint32_t>(mesh->Vertices.size());
					mesh->Vertices.push_back(vertex);
				}

				mesh->Indices.push_back(static_cast<std::uint32_t>(mesh->UniqueVertices[vertex]));
			}
		}

		CheckReturn(CreateVertexBuffer(mesh.get()));
		CheckReturn(CreateIndexBuffer(mesh.get()));

		mMeshes[inFilePath] = std::move(mesh);
	}

	if (mMaterials.count(inTexFilePath) == 0) {
		CheckReturn(AddTexture(inTexFilePath));
	}

	auto ritem = std::make_unique<RenderItem>();
	ritem->Scale = inScale;
	ritem->Quat = inQuat;
	ritem->Pos = inPos;
	ritem->MeshName = inFilePath;
	ritem->MatName = inTexFilePath;

	CheckReturn(CreateUniformBuffers(ritem.get()));
	CheckReturn(CreateDescriptorSets(ritem.get()));

	mRItemRefs[inType][inName] = ritem.get();
	mRItems.push_back(std::move(ritem));

	return true;
}

bool Renderer::UpdateCamera(const glm::vec3& inPos, const glm::vec3& inTarget) {
	mCameraPos = inPos;
	mCameraTarget = inTarget;

	return true;
}

bool Renderer::UpdateModel(
		const std::string& inName, 
		RenderTypes inType, 
		glm::vec3 inScale, 
		glm::fquat inQuat, 
		glm::vec3 inPos) {
	mRItemRefs[inType][inName]->Scale = inScale;
	mRItemRefs[inType][inName]->Quat = inQuat;
	mRItemRefs[inType][inName]->Pos = inPos;

	return true;
}

bool Renderer::Update(const GameTimer& gt) {
	vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);

	VkResult result = vkAcquireNextImageKHR(
		mDevice,
		mSwapChain,
		UINT64_MAX,
		mImageAvailableSemaphores[mCurrentFrame],
		VK_NULL_HANDLE,
		&mCurentImageIndex
	);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		CheckReturn(RecreateSwapChain());
		return true;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		ReturnFalse(L"Failed to acquire swap chain image");
	}

	if (mImagesInFlight[mCurentImageIndex] != VK_NULL_HANDLE)
		vkWaitForFences(mDevice, 1, &mImagesInFlight[mCurentImageIndex], VK_TRUE, UINT64_MAX);
	
	mImagesInFlight[mCurentImageIndex] = mInFlightFences[mCurrentFrame];
	
	CheckReturn(UpdateUniformBuffer(gt));
	
	if (bNeedToUpdateDescriptorSet) {
		CheckReturn(UpdateDescriptorSet());
		bNeedToUpdateDescriptorSet = false;
	}
	
	mOrderedRItemRefs.clear();
	const auto& blendRItemRefs = mRItemRefs[RenderTypes::EBlend];
	for (const auto& blendRItemRefPair : blendRItemRefs) {
		const auto& blendRItemRef = blendRItemRefPair.second;

		float dist = glm::distance(mCameraPos, blendRItemRef->Pos);
		mOrderedRItemRefs.insert(std::make_pair(dist, blendRItemRef));
	}
	
	return true;
}

bool Renderer::Draw() {
	auto& commandBuffer = mCommandBuffers[mCurrentFrame];
	if (vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) != VK_SUCCESS) {
		ReturnFalse(L"Failed to reset command buffer");
	}

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		ReturnFalse(L"Failed to begin recording command buffer");
	}

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = mRenderPass;
	renderPassInfo.framebuffer = mSwapChainFramebuffers[mCurentImageIndex];
	renderPassInfo.renderArea.offset = { 0,0 };
	renderPassInfo.renderArea.extent = mSwapChainExtent;

	// Note: that the order of clearValues should be identical to the order of attachments.
	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = { 0.87f, 0.87f, 0.87f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);
	
	for (const auto& ritemRefPair : mRItemRefs[RenderTypes::EOpaque]) {
		const auto& ritemRef = ritemRefPair.second;
		const auto& mesh = mMeshes[ritemRef->MeshName];

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &ritemRef->DescriptorSets[mCurrentFrame], 0, nullptr);
	
		VkBuffer vertexBuffers[] = { mesh->VertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	
		vkCmdBindIndexBuffer(commandBuffer, mesh->IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
	
		vkCmdDrawIndexed(commandBuffer, static_cast<std::uint32_t>(mesh->Indices.size()), 1, 0, 0, 0);
	}

	for (auto begin = mOrderedRItemRefs.rbegin(), end = mOrderedRItemRefs.rend(); begin != end; ++begin) {
		const auto& ritemRef = begin->second;
		const auto& mesh = mMeshes[ritemRef->MeshName];

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &ritemRef->DescriptorSets[mCurrentFrame], 0, nullptr);
	
		VkBuffer vertexBuffers[] = { mesh->VertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	
		vkCmdBindIndexBuffer(commandBuffer, mesh->IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
	
		vkCmdDrawIndexed(commandBuffer, static_cast<std::uint32_t>(mesh->Indices.size()), 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		ReturnFalse(L"Failed to record command buffer");
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {
		mImageAvailableSemaphores[mCurrentFrame]
	};
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VkSemaphore signalSemaphores[] = {
		mRenderFinishedSemaphores[mCurrentFrame]
	};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);

	if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[mCurrentFrame]) != VK_SUCCESS) {
		ReturnFalse(L"Failed to submit draw command buffer");
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { mSwapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &mCurentImageIndex;
	presentInfo.pResults = nullptr;

	VkResult result = vkQueuePresentKHR(mPresentQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || bFramebufferResized) {
		bFramebufferResized = false;
		CheckReturn(RecreateSwapChain());
	}
	else if (result != VK_SUCCESS) {
		ReturnFalse(L"Failed to present swap chain image");
	}

	mCurrentFrame = (mCurrentFrame + 1) % SwapChainImageCount;

	return true;
}

bool Renderer::RecreateSwapChain() {
	vkDeviceWaitIdle(mDevice);

	CleanUpSwapChain();

	LowRenderer::RecreateSwapChain();

	CheckReturn(CreateImageViews());
	CheckReturn(CreateRenderPass());
	CheckReturn(CreateGraphicsPipeline());
	CheckReturn(CreateColorResources());
	CheckReturn(CreateDepthResources());
	CheckReturn(CreateFramebuffers());
	CheckReturn(RecreateUniformBuffers());
	CheckReturn(CreateDescriptorPool());
	CheckReturn(RecreateDescriptorSets());
	CheckReturn(CreateCommandBuffers());

	return true;
}

void Renderer::CleanUpSwapChain() {
	vkDestroyImageView(mDevice, mColorImageView, nullptr);
	vkDestroyImage(mDevice, mColorImage, nullptr);
	vkFreeMemory(mDevice, mColorImageMemory, nullptr);
	
	vkDestroyImageView(mDevice, mDepthImageView, nullptr);
	vkDestroyImage(mDevice, mDepthImage, nullptr);
	vkFreeMemory(mDevice, mDepthImageMemory, nullptr);
	
	for (auto& ritem : mRItems) {
		for (size_t i = 0; i < SwapChainImageCount; ++i) {
			vkDestroyBuffer(mDevice, ritem->UniformBuffers[i], nullptr);
			vkFreeMemory(mDevice, ritem->UniformBufferMemories[i], nullptr);
		}
	}
	
	vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
	
	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyFramebuffer(mDevice, mSwapChainFramebuffers[i], nullptr);
	}
	
	vkFreeCommandBuffers(mDevice, mCommandPool, static_cast<std::uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());
	
	vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
	vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
	
	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyImageView(mDevice, mSwapChainImageViews[i], nullptr);
	}

	LowRenderer::CleanUpSwapChain();
}

bool Renderer::AddTexture(const std::string& inFilePath) {
	int texWidth = 0;
	int texHeight = 0;
	int texChannels = 0;

	stbi_uc* pixels = stbi_load(inFilePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) ReturnFalse(L"Failed to load texture image");

	auto material = std::make_unique<Material>();
	auto pMat = material.get();
	CheckReturn(CreateTextureImage(texWidth, texHeight, pixels, pMat));
	CheckReturn(CreateTextureImageView(pMat));
	CheckReturn(CreateTextureSampler(pMat));
	mMaterials[inFilePath] = std::move(material);

	stbi_image_free(pixels);

	bNeedToUpdateDescriptorSet = true;

	return true;
}

bool Renderer::UpdateUniformBuffer(const GameTimer& gt) {
	for (auto& ritem : mRItems) {
		UniformBufferObject ubo = {};		

		ubo.mModel = glm::translate(glm::mat4(1.0f), ritem->Pos) *
			glm::mat4_cast(ritem->Quat) *
			glm::scale(glm::mat4(1.0f), ritem->Scale);		
		
		ubo.mView = glm::lookAt(
			mCameraPos,
			mCameraTarget,
			UpVector);

		ubo.mProj = glm::perspective(glm::radians(90.0f), static_cast<float>(mSwapChainExtent.width) / static_cast<float>(mSwapChainExtent.height), 0.1f, 1000.0f);
		ubo.mProj[1][1] *= -1.0f;

		void* data;
		const auto& bufferMemory = ritem->UniformBufferMemories[mCurentImageIndex];		
		vkMapMemory(mDevice, bufferMemory, 0, sizeof(ubo), 0, &data);
		std::memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(mDevice, bufferMemory);
	}

	return true;
}

bool Renderer::UpdateDescriptorSet() {
	for (auto& ritem : mRItems) {
		for (size_t i = 0; i < SwapChainImageCount; ++i) {
			VkDescriptorBufferInfo bufferInfo = {};
			bufferInfo.buffer = ritem->UniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			const auto& mat = mMaterials[ritem->MatName];
			VkDescriptorImageInfo imageInfo = {};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = mat->TextureImageView;
			imageInfo.sampler = mat->TextureSampler;

			std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = ritem->DescriptorSets[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &bufferInfo;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = ritem->DescriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(mDevice, static_cast<std::uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}

	return true;
}

bool Renderer::CreateVertexBuffer(Mesh* pMesh) {
	auto& vertices = pMesh->Vertices;
	auto& vertexBuffer = pMesh->VertexBuffer;
	auto& vertexBufferMemory = pMesh->VertexBufferMemory;

	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CheckReturn(CreateBuffer(
		mPhysicalDevice,
		mDevice,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory));

	void* data;
	vkMapMemory(mDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(mDevice, stagingBufferMemory);

	CheckReturn(CreateBuffer(
		mPhysicalDevice,
		mDevice,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer,
		vertexBufferMemory));

	CopyBuffer(mDevice, mGraphicsQueue, mCommandPool, stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);

	return true;
}

bool Renderer::CreateIndexBuffer(Mesh* pMesh) {
	auto& indices = pMesh->Indices;
	auto& indexBuffer = pMesh->IndexBuffer;
	auto& indexBufferMemory = pMesh->IndexBufferMemory;

	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CheckReturn(CreateBuffer(
		mPhysicalDevice,
		mDevice,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory));

	void* data;
	vkMapMemory(mDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	std::memcpy(data, indices.data(), bufferSize);
	vkUnmapMemory(mDevice, stagingBufferMemory);

	CheckReturn(CreateBuffer(
		mPhysicalDevice,
		mDevice,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBuffer,
		indexBufferMemory));

	CopyBuffer(mDevice, mGraphicsQueue, mCommandPool, stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);

	return true;
}

bool Renderer::CreateUniformBuffers(RenderItem* inRItem) {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	inRItem->UniformBuffers.resize(SwapChainImageCount);
	inRItem->UniformBufferMemories.resize(SwapChainImageCount);

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		CreateBuffer(
			mPhysicalDevice,
			mDevice,
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			inRItem->UniformBuffers[i],
			inRItem->UniformBufferMemories[i]);
	}

	return true;
}

bool Renderer::RecreateUniformBuffers() {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	for (auto& ritem : mRItems) {
		ritem ->UniformBuffers.resize(SwapChainImageCount);
		ritem ->UniformBufferMemories.resize(SwapChainImageCount);

		for (size_t i = 0; i < SwapChainImageCount; ++i) {
			CreateBuffer(
				mPhysicalDevice,
				mDevice,
				bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				ritem->UniformBuffers[i],
				ritem->UniformBufferMemories[i]);
		}
	}

	return true;
}

bool Renderer::CreateDescriptorSets(RenderItem* inRItem) {
	std::vector<VkDescriptorSetLayout> layouts(SwapChainImageCount, mDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = mDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<std::uint32_t>(SwapChainImageCount);
	allocInfo.pSetLayouts = layouts.data();

	inRItem->DescriptorSets.resize(SwapChainImageCount);
	if (vkAllocateDescriptorSets(mDevice, &allocInfo, inRItem->DescriptorSets.data()) != VK_SUCCESS) {
		ReturnFalse(L"Failed to allocate descriptor sets");
	}

	return true;
}

bool Renderer::RecreateDescriptorSets() {
	std::vector<VkDescriptorSetLayout> layouts(SwapChainImageCount, mDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = mDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<std::uint32_t>(SwapChainImageCount);
	allocInfo.pSetLayouts = layouts.data();

	for (auto& ritem : mRItems) {
		ritem->DescriptorSets.resize(SwapChainImageCount);
		if (vkAllocateDescriptorSets(mDevice, &allocInfo, ritem->DescriptorSets.data()) != VK_SUCCESS) {
			ReturnFalse(L"Failed to allocate descriptor sets");
		}
	}

	return true;
}

bool Renderer::CreateTextureImage(int inTexWidth, int inTexHeight, void* pData, Material* ioMaterial) {
	VkDeviceSize imageSize = inTexWidth * inTexHeight * 4;

	ioMaterial->MipLevels = static_cast<std::uint32_t>(std::floor(std::log2(std::max(inTexWidth, inTexHeight)))) + 1;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	CreateBuffer(
		mPhysicalDevice,
		mDevice,
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory);

	void* data;
	vkMapMemory(mDevice, stagingBufferMemory, 0, imageSize, 0, &data);
	std::memcpy(data, pData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mDevice, stagingBufferMemory);

	CheckReturn(CreateImage(
		mPhysicalDevice,
		mDevice,
		inTexWidth,
		inTexHeight,
		ioMaterial->MipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		ImageFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		ioMaterial->TextureImage,
		ioMaterial->TextureImageMemory));

	CheckReturn(TransitionImageLayout(
		mDevice,
		mGraphicsQueue,
		mCommandPool,
		ioMaterial->TextureImage,
		ImageFormat,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		ioMaterial->MipLevels));

	CopyBufferToImage(
		mDevice,
		mGraphicsQueue,
		mCommandPool,
		stagingBuffer,
		ioMaterial->TextureImage,
		static_cast<std::uint32_t>(inTexWidth),
		static_cast<std::uint32_t>(inTexHeight));

	GenerateMipmaps(
		mPhysicalDevice,
		mDevice,
		mGraphicsQueue,
		mCommandPool,
		ioMaterial->TextureImage,
		ImageFormat,
		inTexWidth,
		inTexHeight,
		ioMaterial->MipLevels);

	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);

	return true;
}

bool Renderer::CreateTextureImageView(Material* ioMaterial) {
	CheckReturn(CreateImageView(mDevice, ioMaterial->TextureImage, ImageFormat, ioMaterial->MipLevels, VK_IMAGE_ASPECT_COLOR_BIT, ioMaterial->TextureImageView));

	return true;
}

bool Renderer::CreateTextureSampler(Material* ioMaterial) {
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = static_cast<float>(ioMaterial->MipLevels);

	if (vkCreateSampler(mDevice, &samplerInfo, nullptr, &ioMaterial->TextureSampler) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create texture sampler");
	}

	return true;
}

bool Renderer::CreateImageViews() {
	mSwapChainImageViews.resize(mSwapChainImages.size());

	for (size_t i = 0, end = mSwapChainImages.size(); i < end; ++i) {
		CheckReturn(CreateImageView(mDevice, mSwapChainImages[i], mSwapChainImageFormat, 1, VK_IMAGE_ASPECT_COLOR_BIT, mSwapChainImageViews[i]));
	}

	return true;
}

bool Renderer::CreateRenderPass() {
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = mSwapChainImageFormat;
	colorAttachment.samples = mMSAASamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = FindDepthFormat(mPhysicalDevice);
	depthAttachment.samples = mMSAASamples;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription colorAttachmentResolve = {};
	colorAttachmentResolve.format = mSwapChainImageFormat;
	colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentResolveRef = {};
	colorAttachmentResolveRef.attachment = 2;
	colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
	subpass.pResolveAttachments = &colorAttachmentResolveRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create render pass");
	}

	return true;
}

bool Renderer::CreateCommandPool() {
	QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice, mSurface);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = indices.GetGraphicsFamilyIndex();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create command pool");
	}

	return true;
}

bool Renderer::CreateColorResources() {
	VkFormat colorFormat = mSwapChainImageFormat;

	CheckReturn(CreateImage(
		mPhysicalDevice,
		mDevice,
		mSwapChainExtent.width,
		mSwapChainExtent.height,
		1,
		mMSAASamples,
		colorFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mColorImage,
		mColorImageMemory));

	CheckReturn(CreateImageView(
		mDevice,
		mColorImage,
		colorFormat,
		1,
		VK_IMAGE_ASPECT_COLOR_BIT,
		mColorImageView));

	return true;
}

bool Renderer::CreateDepthResources() {
	VkFormat depthFormat = FindDepthFormat(mPhysicalDevice);

	CheckReturn(CreateImage(
		mPhysicalDevice,
		mDevice,
		mSwapChainExtent.width,
		mSwapChainExtent.height,
		1,
		mMSAASamples,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mDepthImage,
		mDepthImageMemory));

	CheckReturn(CreateImageView(
		mDevice, 
		mDepthImage, 
		depthFormat, 
		1, 
		VK_IMAGE_ASPECT_DEPTH_BIT, 
		mDepthImageView));

	CheckReturn(TransitionImageLayout(
		mDevice,
		mGraphicsQueue,
		mCommandPool,
		mDepthImage,
		depthFormat,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		1
	));

	return true;
}

bool Renderer::CreateFramebuffers() {
	mSwapChainFramebuffers.resize(mSwapChainImageViews.size());

	for (size_t i = 0, end = mSwapChainImageViews.size(); i < end; ++i) {
		std::array<VkImageView, 3> attachments = {
			mColorImageView,
			mDepthImageView,
			mSwapChainImageViews[i],
		};
	
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = mRenderPass;
		framebufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = mSwapChainExtent.width;
		framebufferInfo.height = mSwapChainExtent.height;
		framebufferInfo.layers = 1;
	
		if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mSwapChainFramebuffers[i]) != VK_SUCCESS) {
			ReturnFalse(L"Failed to create framebuffer");
		}
	}

	return true;
}

bool Renderer::CreateDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create descriptor set layout");
	}

	return true;
}

bool Renderer::CreateGraphicsPipeline() {
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	{
		std::vector<char> vertShaderCode;
		CheckReturn(ReadFile("./../../../../Assets/Shaders/vert.spv", vertShaderCode));
		std::vector<char> fragShaderCode;
		CheckReturn(ReadFile("./../../../../Assets/Shaders/frag.spv", fragShaderCode));

		CheckReturn(CreateShaderModule(mDevice, vertShaderCode, vertShaderModule));
		CheckReturn(CreateShaderModule(mDevice, fragShaderCode, fragShaderModule));
	}

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {
		vertShaderStageInfo, fragShaderStageInfo
	};

	auto bindingDescription = Vertex::GetBindingDescription();
	auto attributeDescriptioins = Vertex::GetAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptioins.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptioins.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(mSwapChainExtent.width);
	viewport.height = static_cast<float>(mSwapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = mSwapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_TRUE;
	multisampling.rasterizationSamples = mMSAASamples;
	multisampling.minSampleShading = 0.2f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_SUBTRACT;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create pipeline layout");
	}

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = mPipelineLayout;
	pipelineInfo.renderPass = mRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create graphics pipeline");
	}

	vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
	vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);

	return true;
}

bool Renderer::CreateDescriptorPool() {
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<std::uint32_t>(SwapChainImageCount);

	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<std::uint32_t>(SwapChainImageCount);

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<std::uint32_t>(SwapChainImageCount * 32);
	poolInfo.flags = 0;

	if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create descriptor pool");
	}

	return true;
}

bool Renderer::CreateCommandBuffers() {
	mCommandBuffers.resize(mSwapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = mCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<std::uint32_t>(mCommandBuffers.size());

	if (vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
		ReturnFalse(L"Failed to create command buffers");
	}

	return true;
}

bool Renderer::CreateSyncObjects() {
	mImageAvailableSemaphores.resize(SwapChainImageCount);
	mRenderFinishedSemaphores.resize(SwapChainImageCount);
	mInFlightFences.resize(SwapChainImageCount);
	mImagesInFlight.resize(SwapChainImageCount, VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
			ReturnFalse(L"Failed to create synchronization object(s) for a frame");
	}

	return true;
}
