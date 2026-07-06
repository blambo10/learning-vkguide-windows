#pragma once
#include <vk_types.h>
#include <unordered_map>
#include <filesystem>

struct GLTFMaterial {
	MaterialInstance data;
};

struct GeoSurface {
	uint32_t startIndex;
	uint32_t count;
	std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
	std::string name;
	std::vector<GeoSurface> surfaces;
	GPUMeshBuffers meshBuffers;
};

struct LoadedGLTF : public IRenderable {
	//Storage for all data on a given GLTF file
	std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
	std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
	std::unordered_map<std::string, AllocatedImage> images;
	std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

	//Nodes that dont have parent, for iterating through the file in tree order
	std::vector<std::shared_ptr<Node>> topNodes;

	std::vector<VkSampler> samplers;
	
	DescriptorAllocatorGrowable descriptorPool;

	AllocatedBuffer materialDataBuffer;

	VulkanEngine* creator;

	~LoadedGLTF() { clearAll(); };

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx);

private:
	void clearAll();
};

class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath);