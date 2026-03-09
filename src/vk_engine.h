// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vulkan/vulkan.h>
#include <vk_descriptors.h>

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct FrameData {
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	VkSemaphore _swapchainSemaphore;
	VkFence _renderFence;

	DeletionQueue _deletionQueue;
};

struct RenderData {
	VkSemaphore _renderSemaphore;
};

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	bool _isFirstIteration{ true };
	VkExtent2D _windowExtent{ 1700 , 900 };

	VkInstance _instance; // Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug outputhandle
	VkPhysicalDevice _chosenGPU; // GPU Chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface; // Vulkan window surface
	VkExtent2D _swapchainExtent;

	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	DescriptorAllocator globalDescriptorAllocator;
	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	std::vector<VkImage> _swapchainImages;

	std::vector<VkImageView> _swapchainImageViews;
	
	std::vector<VkSemaphore> _renderSemaphores;

	FrameData _frames[FRAME_OVERLAP];
	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP ]; }

	DeletionQueue _mainDeletionQueue;

	VmaAllocator _allocator;

	AllocatedImage _drawImage;
	VkExtent2D _drawExtent;

	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };

	VkPipelineLayout _trianglePipelineLayout;
	VkPipeline _trianglePipeline;

	//initializes everything in the engine
	void init();
	
	//shuts down the engine
	void cleanup();

	void draw_background(VkCommandBuffer cmd);

	//draw loop
	void draw();
	
	void draw_imgui(VkCommandBuffer cmd,
		VkImageView targetImageView);

	void draw_geometry(VkCommandBuffer cmd);

	//run main loop
	void run();

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

private:
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_pipelines();
	void init_background_pipelines();
	void init_imgui();
	void init_triangle_pipeline();
	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();

};
