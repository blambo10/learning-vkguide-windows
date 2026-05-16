// Continue here https://vkguide.dev/docs/new_chapter_4/textures/
// continue to debug why the checkerboard isnt rendering to monkey head from 

//trying to fix error where vkdescriptorsetlayout isnt being destroyed, likely one that was created in this section 

//Note: to modify the monkey head transparency, update the vec4 opactiry field in the fragment shader at coloured_triangle.frag, then rerun the shader compile..bat

//Note: Modify the _windowExtent to be manual width/height if you want to test the resizing functionality, otherwise it will be set to the maximum supported by the monitor
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>
#include <vulkan/vulkan.h>

#include "VkBootstrap.h"

#include <chrono>
#include <thread>
#include <vk_images.h>
#include <vk_globals.h>
#include <vk_logging.h>
#include <vk_pipelines.h>
#include <glm/gtx/transform.hpp>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#ifdef NDEBUG
constexpr bool bUseValidationLayers = false; l - ; p;
#else
constexpr bool bUseValidationLayers = true;
#endif

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

// Initialization Methods
void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    LOG_INFO("Initializing SDL2 window");

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    LOG_INFO("Creating SDL2 window");

	// get the display mode so we can set the window size to the maximum supported by the monitor
    if (SDL_GetCurrentDisplayMode(0, &DM) != 0) {
        LOG_ERROR("Could not get display mode for video display: {}", SDL_GetError());
    }

	// set the window extent to the display mode size
    _windowExtent = { (uint32_t)DM.w, (uint32_t)DM.h };

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    init_vulkan();

    init_swapchain();

    init_commands();

    init_sync_structures();

	init_descriptors();

    init_default_data();

    init_pipelines();

    init_imgui();

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::init_vulkan() {
	LOG_INFO("Initializing Vulkan");
	vkb::InstanceBuilder builder;

    auto inst_ret = builder.set_app_name("Example Vulkan Application")
        .request_validation_layers(bUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    _instance = vkb_inst.instance;
	_debug_messenger = vkb_inst.debug_messenger;

	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    // Vulkan 1.3 features
	VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features.dynamicRendering = true;
	features.synchronization2 = true;

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.descriptorIndexing = true;
    features12.bufferDeviceAddress = true;

    // use vkbootstrap to select a gpu

    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector
        .add_required_extension(VK_EXT_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_EXTENSION_NAME)
        .set_minimum_version(1, 3)
        .set_required_features_13(features)
        .set_required_features_12(features12)
        .set_surface(_surface)
        .select()
        .value();

    // create final device
    vkb::DeviceBuilder deviceBuilder{ physicalDevice };

    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT unusedAttachments{};
    unusedAttachments.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
    unusedAttachments.dynamicRenderingUnusedAttachments = VK_TRUE;

	vkb::Device vkb_device = deviceBuilder
        .add_pNext(&unusedAttachments)
        .build()
        .value();

    // get the vkdevice handle
	_device = vkb_device.device;
	_chosenGPU = physicalDevice.physical_device;
    
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, _surface, &_capabilities);

    //_windowExtent.height = DM.h;
    //_windowExtent.width = DM.w;


    _graphicsQueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    _mainDeletionQueue.push_function([&]() {
        vmaDestroyAllocator(_allocator);
        });

	LOG_INFO("Vulkan Initialized with device: {}", physicalDevice.properties.deviceName);

}

void VulkanEngine::init_swapchain() {
    create_swapchain(_windowExtent.width, _windowExtent.height);

    VkExtent3D drawImageExtent = {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

	LOG_INFO("device capabilities: max image extent: {}x{}", DM.w, DM.h);
   // VkExtent3D drawImageExtent = {
   //     _capabilities.currentExtent.width,
   //     _capabilities.currentExtent.height,
		 //1
   // };

    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    //_drawImage.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    _drawImage.imageExtent = drawImageExtent;
    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.imageExtent = drawImageExtent;


    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(
        _drawImage.imageFormat,
        drawImageUsages,
        drawImageExtent
    );

    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageCreateInfo dimg_info = vkinit::image_create_info(
        _depthImage.imageFormat,
        depthImageUsages,
        drawImageExtent
    );

    vmaCreateImage(_allocator,
        &rimg_info,
        &rimg_allocinfo,
        &_drawImage.image,
        &_drawImage.allocation,
        nullptr);

    vmaCreateImage(_allocator,
        &dimg_info,
        &rimg_allocinfo,
        &_depthImage.image,
        &_depthImage.allocation,
        nullptr
    );

    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(
        _drawImage.imageFormat,
        _drawImage.image,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(
        _depthImage.imageFormat,
        _depthImage.image,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );

    VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

    VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));

    _mainDeletionQueue.push_function([=]() {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

        vkDestroyImageView(_device, _depthImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
    });
}

void VulkanEngine::init_commands() {
 
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
    };

    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

    _mainDeletionQueue.push_function([=]() {
        vkDestroyCommandPool(_device, _immCommandPool, nullptr);
        });
}

void VulkanEngine::init_sync_structures() {
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++){
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
    };

    // Per swapchain image semaphore instead of by frame (vulkan spec)
    for (int i = 0; i < _swapchainImages.size(); i++) {
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphores[i]));
    }

    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
    _mainDeletionQueue.push_function([=]() {vkDestroyFence(_device, _immFence, nullptr); });
}

void VulkanEngine::init_descriptors() {
    LOG_DEBUG("init descriptors");

    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    globalDescriptorAllocator.init_pool(_device, 10, sizes);

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    };

    LOG_DEBUG("descriptor pools allocated");

    _drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

    VkDescriptorImageInfo imgInfo = {};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = _drawImage.imageView;

    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;

    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = _drawImageDescriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;

    LOG_DEBUG("descriptor structs created");

    vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        _gpuSceneDataDescriptorLayout = builder.build(_device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    };

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        _singleImageDescriptorLayout = builder.build(
            _device,
            VK_SHADER_STAGE_FRAGMENT_BIT
        );
    };

    LOG_DEBUG("updated descriptor sets");

    LOG_INFO("frame descriptor allocators init");

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3  },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3  },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        _frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
        _frames[i]._frameDescriptors.init(_device, 1000, frame_sizes);

        _mainDeletionQueue.push_function([&, i]() {
            _frames[i]._frameDescriptors.destroy_pools(_device);
        });
    }

    LOG_INFO("finished allocation of frame descriptor allocators init");


    LOG_INFO("finished init_descriptors");


    _mainDeletionQueue.push_function([&]() {
        globalDescriptorAllocator.destroy_pool(_device);

        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout, nullptr);
        });

};

void VulkanEngine::init_pipelines() {
    init_background_pipelines();

    init_mesh_pipeline();
}

void VulkanEngine::init_background_pipelines() {
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device,
                                    &computeLayout,
                                    nullptr,
                                    &_gradientPipelineLayout));

    VkShaderModule gradientShader;
    if (!vkutil::load_shader_module("../../shaders/gradient_color.comp.spv", _device, &gradientShader)) {
        LOG_ERROR("unable to build the gradient shader");
    }

    VkShaderModule skyShader;
    if (!vkutil::load_shader_module("../../shaders/sky.comp.spv", _device, &skyShader)) {
        LOG_ERROR("unable to build the sky shader");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = gradientShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    ComputeEffect gradient;
    gradient.layout = _gradientPipelineLayout;
    gradient.name = "gradient";
    gradient.data = {};

    //default colors
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    VK_CHECK(vkCreateComputePipelines(_device,
                                      VK_NULL_HANDLE,
                                      1,
                                      &computePipelineCreateInfo,
                                      nullptr,
                                      &gradient.pipeline));
    
    computePipelineCreateInfo.stage.module = skyShader;

    ComputeEffect sky;
    sky.layout = _gradientPipelineLayout;
    sky.name = "sky";
    sky.data = {};
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);
    
    VK_CHECK(vkCreateComputePipelines(_device,
        VK_NULL_HANDLE,
        1,
        &computePipelineCreateInfo,
        nullptr,
        &sky.pipeline));


    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(sky);

    vkDestroyShaderModule(_device,
                          gradientShader,
                          nullptr);

    vkDestroyShaderModule(_device,
                        skyShader,
                        nullptr);

    _mainDeletionQueue.push_function([=]() {
        vkDestroyPipelineLayout(_device,
            _gradientPipelineLayout,
            nullptr);
        vkDestroyPipeline(_device,
            sky.pipeline,
            nullptr);
        vkDestroyPipeline(_device,
            gradient.pipeline,
            nullptr);

    });
}

void VulkanEngine::init_imgui() {

    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(_window);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _chosenGPU;
    init_info.Device = _device;
    init_info.Queue = _graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();

    _mainDeletionQueue.push_function([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_device, imguiPool, nullptr);
        });
}

void VulkanEngine::init_mesh_pipeline() {
    VkShaderModule triangleFragShader;
    if (!vkutil::load_shader_module("../../shaders/tex_image.frag.spv", _device, &triangleFragShader)) {
        LOG_ERROR("Error when building the triangle fragment shader module");
    }
    else {
        LOG_INFO("Triangle fragment shader succesfully loaded");
    }

    VkShaderModule triangleVertexShader;
    if (!vkutil::load_shader_module("../../shaders/colored_triangle_mesh.vert.spv", _device, &triangleVertexShader)) {
        LOG_ERROR("Error when building the triangle vertex shader module");
    }
    else {
        LOG_INFO("Mesh vertex shader succesfully loaded");
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &bufferRange;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pSetLayouts = &_singleImageDescriptorLayout;
    pipeline_layout_info.setLayoutCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));

    
    PipelineBuilder pipelineBuilder;

    pipelineBuilder._pipelineLayout = _meshPipelineLayout;
    pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.enable_blending_alphablend();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(_depthImage.imageFormat);

    LOG_INFO("draw image format: {}", std::to_string(_drawImage.imageFormat));
    LOG_INFO("depth image format: {}", std::to_string(_depthImage.imageFormat));

    _meshPipeline = pipelineBuilder.build_pipline(_device);

    vkDestroyShaderModule(_device, triangleFragShader, nullptr);
    vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
        vkDestroyPipeline(_device, _meshPipeline, nullptr);
    });
}

void VulkanEngine::init_default_data() {
    //std::array<Vertex, 4> rect_vertices;

    //rect_vertices[0].position = { 0.5,-0.5, 0 };
    //rect_vertices[1].position = { 0.5,0.5, 0 };
    //rect_vertices[2].position = { -0.5,-0.5, 0 };
    //rect_vertices[3].position = { -0.5,0.5, 0 };

    //rect_vertices[0].color = { 0,0,0,1 };
    //rect_vertices[1].color = { 0.5,0.5,0.5 ,1 };
    //rect_vertices[2].color = { 1,0, 0,1 };
    //rect_vertices[3].color = { 0,1, 0,1 };

    //std::array<uint32_t, 6> rect_indices;

    //rect_indices[0] = 0;
    //rect_indices[1] = 1;
    //rect_indices[2] = 2;

    //rect_indices[3] = 2;
    //rect_indices[4] = 1;
    //rect_indices[5] = 3;

    //rectangle = uploadMesh(rect_indices, rect_vertices);

        //testMeshes = loadGltfMeshes(this, "C:\\Users\\blamb\\repos\\vulkan - guide\\assets\\basicmesh.glb").value();
//C:\Users\blamb\repos\vulkan - guide\assets
    //delete the rectangle data on engine shutdown
    //_mainDeletionQueue.push_function([&]() {
    //    destroy_buffer(rectangle.indexBuffer);
    //    destroy_buffer(rectangle.vertexBuffer);
    //    });

    testMeshes = loadGltfMeshes(this, "..\\..\\assets\\basicmesh.glb").value();

    LOG_INFO("creating white image");
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage = create_image((void*)&white,
        VkExtent3D{ 1,1,1 },
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT
    );

    LOG_INFO("white image created, now creating grey image");

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    _greyImage = create_image((void*)&grey,
        VkExtent3D{ 1,1,1 },
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT
    );

    LOG_INFO("grey image created, now creating black image");

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    _blackImage = create_image(
        (void *)&black,
        VkExtent3D{ 1,1,1 },
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT
    );

    LOG_INFO("black image created, now creating checkerboard image");

    //uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    //std::array<uint32_t, 16 * 16> pixels;
    //for (int x = 0; x < 16; x++) {
    //    for (int y = 0; y < 16; y++) {
    //        pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
    //    }
    //}

    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

	LOG_INFO("finished creating checkerboard image, now creating image resource");
    
	//todo: fix the size issue here (causing the error when copying from buffer to image, trying to copy more than the buffer contains)
    _errorCheckerboardImage = create_image(
        pixels.data(),
        VkExtent3D{ 16,16,1 },
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT
    );

    LOG_INFO("Finished copying checkerboard error image from buffer to image");

    VkSamplerCreateInfo sampl = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO
    };

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;
    
    vkCreateSampler(
        _device,
        &sampl,
        nullptr,
        &_defaultSamplerNearest
    );

    sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;

    vkCreateSampler(
        _device,
        &sampl,
        nullptr,
        &_defaultSamplerLinear
    );

    _mainDeletionQueue.push_function([&]() {
        vkDestroySampler(
            _device,
            _defaultSamplerNearest,
            nullptr
        );
        vkDestroySampler(
            _device,
            _defaultSamplerLinear,
            nullptr
        );

        destroy_image(_whiteImage);
        destroy_image(_greyImage);
        destroy_image(_blackImage);
        destroy_image(_errorCheckerboardImage);

    });
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;

    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    VK_CHECK(vmaCreateBuffer(_allocator,
        &bufferInfo,
        &vmaallocInfo,
        &newBuffer.buffer,
        &newBuffer.allocation,
        &newBuffer.info));

    return newBuffer;
}

AllocatedImage VulkanEngine::create_image(
    VkExtent3D size,
    VkFormat format,
    VkImageUsageFlags usage,
    bool mipmapped) {

    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped) {
		img_info.mipLevels = static_cast<uint32_t>(
            std::floor(
                std::log2(
                    std::max(size.width, size.height)
                )
            )
        ) + 1;
    }

    VmaAllocationCreateInfo allocinfo = {};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(
            _allocator,
            &img_info,
            &allocinfo,
            &newImage.image,
            &newImage.allocation,
            nullptr
        )
    );

    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(
        format,
        newImage.image,
        aspectFlag
    );
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    
    VK_CHECK(
        vkCreateImageView(
            _device, 
            &view_info, 
            nullptr, 
            &newImage.imageView
        )
    );

    return newImage;
}

AllocatedImage VulkanEngine::create_image(
    void* data,
    VkExtent3D size,
    VkFormat format,
    VkImageUsageFlags usage,
    bool mipmapped) {

    size_t data_size = size.width * size.height * 4;
    AllocatedBuffer uploadbuffer = create_buffer(
        data_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    memcpy(
        uploadbuffer.info.pMappedData, 
        data, 
        data_size
    );

    AllocatedImage new_image = create_image(
        size, 
        format, 
        usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        mipmapped
    );

    immediate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(
            cmd,
            new_image.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        LOG_INFO("copying upload buffer to image");

        vkCmdCopyBufferToImage(
            cmd,
            uploadbuffer.buffer,
            new_image.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
			&copyRegion
        );

        LOG_INFO("finished copying upload buffer to image");

        vkutil::transition_image(
            cmd,
            new_image.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

    });

    destroy_buffer(uploadbuffer);

    return new_image;
}

//Cleanup Methods
void VulkanEngine::cleanup()
{
    LOG_INFO("Cleaning up");
    if (_isInitialized) {

        vkDeviceWaitIdle(_device);

		for (int i = 0; i < _swapchainImages.size(); i++)        {
            vkDestroySemaphore(_device, _renderSemaphores[i], nullptr);
        }

		for (int i = 0; i < FRAME_OVERLAP; i++) {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
            _frames[i]._deletionQueue.flush();
            _frames[i]._frameDescriptors.destroy_pools(_device);
        }

        for (auto& mesh : testMeshes) {
            destroy_buffer(mesh->meshBuffers.indexBuffer);
            destroy_buffer(mesh->meshBuffers.vertexBuffer);
        }

		_mainDeletionQueue.flush();

        LOG_INFO("Destroying swap chain");
        destroy_swapchain();

        LOG_INFO("Destroying surface");
        //LOG_INFO("deleting device before deleting pipelines here");
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);

        LOG_INFO("Destroying debug messenger and instance + window");
        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);
        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw_background(VkCommandBuffer cmd) {
    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

    // bind the gradient drawing compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptorset containing the draw image for the compute pipeline 
    vkCmdBindDescriptorSets(cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        _gradientPipelineLayout,
        0,
        1,
        &_drawImageDescriptors,
        0,
        nullptr);

    vkCmdPushConstants(cmd,
        _gradientPipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(ComputePushConstants),
        &effect.data);
    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd,
    VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView,
        nullptr,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
};

void VulkanEngine::draw()
{
    _drawExtent.height = std::min(
        _swapchainExtent.height,
        _drawImage.imageExtent.height) * renderScale;
    _drawExtent.width = std::min(
        _swapchainExtent.width,
        _drawImage.imageExtent.width
	) * renderScale;

    VK_CHECK(
        vkWaitForFences(_device, 
        1, 
        &get_current_frame()._renderFence, 
        true, 1000000000)
    );
    
    get_current_frame()._deletionQueue.flush();
    get_current_frame()._frameDescriptors.clear_pools(_device);

    VK_CHECK(
        vkResetFences(_device, 
        1, 
        &get_current_frame()._renderFence)
    );

    
    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(_device,
        _swapchain, 
        1000000000,
        get_current_frame()._swapchainSemaphore,
        nullptr,
        &swapchainImageIndex
    );
    if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        resize_requested = true;
        return;
    };

    
    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;
	VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    _drawExtent.width = _drawImage.imageExtent.width;
    _drawExtent.height = _drawImage.imageExtent.height;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(cmd,
        _drawImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL);
    vkutil::transition_image(cmd,
        _depthImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
    );


	draw_background(cmd);
 
    vkutil::transition_image(cmd, 
        _drawImage.image, 
        VK_IMAGE_LAYOUT_GENERAL, 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );
    //vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    draw_geometry(cmd);

    //LOG_INFO("BREAK");

    //transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(cmd,
        _drawImage.image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd,
        _swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(
        cmd,
        _drawImage.image,
        _swapchainImages[swapchainImageIndex],
        _drawExtent,
        _swapchainExtent
    );

    // set swapchain image layout to Attachment Optimal so we can draw it
    vkutil::transition_image(cmd, 
                            _swapchainImages[swapchainImageIndex], 
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

    // set swapchain image layout to Present so we can show it on the screen
    vkutil::transition_image(
        cmd,
        _swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        get_current_frame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        _renderSemaphores[swapchainImageIndex]);

    VkSubmitInfo2 submit = vkinit::submit_info(
        &cmdinfo,
        &signalInfo,
        &waitInfo);

    VK_CHECK(vkQueueSubmit2(
        _graphicsQueue,
        1,
        &submit,
        get_current_frame()._renderFence));

    VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &_renderSemaphores[swapchainImageIndex];
	presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;
    
    VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        resize_requested = true;
    }

    //get_current_frame()._deletionQueue.flush();

    _frameNumber++;
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd) {

	LOG_INFO("allocating buffer and image descriptor sets in drawing geometry");

    AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VMA_MEMORY_USAGE_CPU_TO_GPU);
    LOG_INFO("buffer created for gpu scene");

    get_current_frame()._deletionQueue.push_function([=, this]() {
        destroy_buffer(gpuSceneDataBuffer);
    });

    LOG_INFO("obtained frame for gpu scene");

    GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
    *sceneUniformData = sceneData;

    LOG_INFO("obtained allocation for sceneuniformdata");

	LOG_INFO("frame number: {}", _frameNumber);
    // todo: fix this 
    //VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);

    //LOG_INFO("writing descriptor set");
    //DescriptorWriter writer;
    //
    //writer.write_buffer(
    //    0, 
    //    gpuSceneDataBuffer.buffer, 
    //    sizeof(GPUSceneData), 
    //    0, 
    //    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    //);

    //writer.update_set(
    //    _device, 
    //    globalDescriptor
    //);

    LOG_INFO("finished allocating buffer and image descriptor sets in drawing geometry");

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);

    //todo: look at this function as there are duplicates and unecesarry code
    vkCmdBindPipeline(cmd, 
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        _meshPipeline);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = _drawExtent.width;
    viewport.height = _drawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = _drawExtent.width;
    scissor.extent.height = _drawExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);


    VkDescriptorSet imageSet = get_current_frame()._frameDescriptors.allocate(
        _device, 
        _singleImageDescriptorLayout
    );

    {
        DescriptorWriter writer;

        writer.write_image(0,
            _errorCheckerboardImage.imageView,
            _defaultSamplerNearest,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        );

        writer.update_set(_device, imageSet);
    }

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        _meshPipelineLayout,
        0,
        1,
        &imageSet,
        0,
        nullptr
    );

    glm::mat4 view = glm::translate(glm::vec3{ 0,0,-5 });

    // Camera Projection
    glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)_drawExtent.width / (float)_drawExtent.height, 10000.f, 0.1f);

    projection[1][1] *= -1;

    GPUDrawPushConstants push_constants;
    push_constants.worldMatrix = projection * view;
    push_constants.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAddress;

    vkCmdPushConstants(cmd,
        _meshPipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(GPUDrawPushConstants),
        &push_constants
    );

    vkCmdBindIndexBuffer(cmd,
        testMeshes[2]->meshBuffers.indexBuffer.buffer,
        0,
        VK_INDEX_TYPE_UINT32
    );

    LOG_INFO("drawing mesh");
    vkCmdDrawIndexed(cmd,
        testMeshes[2]->surfaces[0].count,
        1,
        testMeshes[2]->surfaces[0].startIndex, 
        0, 
        0
    );
    LOG_INFO("finished drawing mesh");

    vkCmdEndRendering(cmd);
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        if (_frameNumber > 0) {
            first_frame = false;
        };

        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }

            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    bQuit = true;
                }

                LOG_INFO("Key {} was pressed", e.key.keysym.sym);
            }

            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        if (resize_requested) {
            resize_swapchain();
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        //ImGui::ShowDemoWindow();

        if (ImGui::Begin("background")) {
            ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.f);

            ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

            ImGui::Text("Selected effect: ", selected.name);
            ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);
            
            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);
        }
        ImGui::End();

        ImGui::Render();

        draw();
    }
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) {
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU, _device, _surface };

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();
    
    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();

    _renderSemaphores.resize(_swapchainImages.size());
}

void VulkanEngine::resize_swapchain() {
    vkDeviceWaitIdle(_device);

    destroy_swapchain();

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;

    create_swapchain(_windowExtent.width, _windowExtent.height);

    resize_requested = false;
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    newSurface.vertexBuffer = create_buffer(vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    VkBufferDeviceAddressInfo deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO ,
        .buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

    newSurface.indexBuffer = create_buffer(indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = create_buffer(
        vertexBufferSize + indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );

    void* data = staging.allocation->GetMappedData();

    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd,
            staging.buffer,
            newSurface.vertexBuffer.buffer,
            1,
            &vertexCopy);

        VkBufferCopy indexCopy{ 0 };
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd,
            staging.buffer,
            newSurface.indexBuffer.buffer,
            1,
            &indexCopy);

        });

    destroy_buffer(staging);

    return newSurface;
}

void VulkanEngine::destroy_swapchain() {
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    for (int i = 0; i < _swapchainImageViews.size(); i++) {
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer) {
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

void VulkanEngine::destroy_image(const AllocatedImage& img) {
    vkDestroyImageView(_device, img.imageView, nullptr);
    vmaDestroyImage(_allocator, img.image, img.allocation);
}