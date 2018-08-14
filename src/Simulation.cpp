#include "Simulation.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <stdexcept>
#include <iostream>
#include <fstream>

#include "Extensions.h"
#include "Layers.h"
#include "Version.h"


Simulation::Simulation() {
 
}
 
Simulation::~Simulation() {
    cleanup_swapchain();
    vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
    vkFreeMemory(m_device, m_ubo_mem, nullptr);
    vkDestroyBuffer(m_device, m_ubo, nullptr);
    vkFreeMemory(m_device, m_ibo_mem, nullptr);
    vkDestroyBuffer(m_device, m_ibo, nullptr);
    vkFreeMemory(m_device, m_vbo_mem, nullptr);
    vkDestroyBuffer(m_device, m_vbo, nullptr);
    vkDestroySemaphore(m_device, m_image_available_semaphore, nullptr);
    vkDestroySemaphore(m_device, m_render_finished_semaphore, nullptr);

    vkDestroyDescriptorSetLayout(m_device, m_desc_set_layout, nullptr);
    vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    auto vkDestroyDebugReportCallbackEXT = 
        (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT");
    if(vkDestroyDebugReportCallbackEXT) {
        vkDestroyDebugReportCallbackEXT(m_instance, m_debug_callback, nullptr);
    }
    vkDestroyDevice(m_device, nullptr);

    vkDestroyInstance(m_instance, nullptr);
    glfwDestroyWindow(m_window);
    glfwTerminate();
}
 
void Simulation::run() {
    create_window();
    setup_device();

    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        draw_frame();
        vkQueueWaitIdle(m_present_queue);
    }

    vkDeviceWaitIdle(m_device);
}
 
void Simulation::create_window() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(1920, 1080, "Vulkan Test", nullptr, nullptr);
    if(!m_window) {
        throw std::runtime_error("Failed to create SFML window!");
    }
    glfwSetWindowUserPointer(m_window, this);
    glfwSetWindowSizeCallback(m_window, glfw_resize_callback);
}
 
void Simulation::setup_device() {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Triangle Demo";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto layers = get_extension_layers();
    auto extensions = get_instance_extensions();

    createInfo.enabledExtensionCount = extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = layers.size();
    createInfo.ppEnabledLayerNames = layers.data();

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if(result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vulkan instance.");
    }
    setup_debug_callback();
    make_logical_device(); 
    setup_surface();
    setup_framebuffer();
    setup_render_pass();
    create_pipeline();
    create_framebuffer();
    create_command_pool();
    create_vbo();
    create_ibo();
    create_semaphores();
    create_ubo();
    create_descriptor_pool();
    create_descriptor_set();
    create_command_buffers();
}
 
std::vector<const char*> Simulation::get_extension_layers() {
    uint32_t layers_count;
    vkEnumerateInstanceLayerProperties(&layers_count, nullptr);
    std::vector<VkLayerProperties> all_layers(layers_count);
    vkEnumerateInstanceLayerProperties(&layers_count, all_layers.data());
    LayerSet layers(std::move(all_layers));

    std::vector<const char*> debug_layers = {
        "VK_LAYER_LUNARG_standard_validation"
    };

    if(!layers.contains_all(debug_layers)) {
        std::cout << "Not all debug layers exist!\n";
        return {};
    }

    std::cout << "Available Layers:\n";
    for(auto& layer: layers) {
        std::cout << "\t" << layer.layerName << ": " << layer.description << " v" << layer.implementationVersion 
            << ". Spec v" << VkVersion(layer.specVersion) << "\n";
    }

    bool found = false;

    return debug_layers;
}

std::vector<const char*> Simulation::get_instance_extensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    auto extensions = ExtensionSet::get_instance_extensions();

    std::cout << "Available Instance Extensions:\n";
    for(const auto& ex : extensions) {
        std::cout << "\t" << ex << "\n";
    }

    std::vector<const char*> requested_extension;
    for(uint32_t i = 0; i < glfwExtensionCount; ++i) {
        requested_extension.push_back(glfwExtensions[i]);
    }
    requested_extension.push_back("VK_EXT_debug_report");

    std::cout << "Required Extensions:\n";
    for(const auto& ex : requested_extension) {
        std::cout << "\t" << ex << "\n";
    }

    if(!extensions.contains_all(requested_extension)) {
        throw std::runtime_error("Not all required extensions are supported!");
    }


    return requested_extension;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* layerPrefix,
    const char* msg,
    void* userData) {

    std::cerr << "Debug layer: " << msg << std::endl;

    return VK_FALSE;
}

void Simulation::setup_debug_callback() {
    VkDebugReportCallbackCreateInfoEXT debug_info = {};
    debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_info.pNext = nullptr;
    debug_info.pUserData = nullptr;
    debug_info.pfnCallback = &debug_callback;
    debug_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT
        | VK_DEBUG_REPORT_WARNING_BIT_EXT
        | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;

    auto vkCreateDebugReportCallbackEXT = 
        (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT");
    if(!vkCreateDebugReportCallbackEXT) {
        throw std::runtime_error("Can't load vkCreateDebugReportCallbackEXT function pointer!");
    }

    vkCreateDebugReportCallbackEXT(m_instance, &debug_info, nullptr, &m_debug_callback);
    
}
 
VkPhysicalDevice Simulation::select_physical_device() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

    std::cout << "Available Devices: " << std::endl;
    for(const auto& dev : devices) {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(dev, &memory_properties);
        vkGetPhysicalDeviceProperties(dev, &properties);
        std::cout << "\t" << properties.deviceID << ": " << properties.deviceName << "\n";
        std::cout << "\t|> " << "API Version: " << VkVersion(properties.apiVersion) << "\n";
        std::cout << "\t|> " << "Memory: " << "\n";
        for(int i = 0; i < memory_properties.memoryHeapCount; ++i) {
            std::cout << "\t\t|> Heap " << i << ": " << memory_properties.memoryHeaps[i].size << "\n";
        }
    }

    VkPhysicalDevice device = devices[0];

    return device;
}
 
void Simulation::make_logical_device() {
    VkPhysicalDevice physical_device = select_physical_device();
    
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

    std::cout << "Available Queue Families:\n";
    auto id = 0;
    auto i = 0;
    for(const auto& family: queue_families) {
        std::cout << "\t#" << i << " Flags: " << family.queueFlags << " Max Count: " << family.queueCount << std::endl;
        if((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0) {
            id = i;
        }
        i += 1;
    }
    float queue_priorities = 1.0;

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = id;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queue_priorities;

    VkPhysicalDeviceFeatures device_features = {};
    VkDeviceCreateInfo device_info = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pEnabledFeatures = &device_features;
    device_info.pQueueCreateInfos = &queueCreateInfo;
    device_info.queueCreateInfoCount = 1;

    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, extensions.data());

    std::cout << "Available Device Extensions:\n";
    for(std::size_t i = 0; i < extensions.size(); ++i) {
        const auto& ex = extensions[i];
        std::cout << "\t" << ex.extensionName << ". Spec version " << ex.specVersion << "\n";
    }

    std::vector<const char*> device_extensions = {
        "VK_KHR_swapchain",
    };

    device_info.enabledExtensionCount = device_extensions.size();
    device_info.ppEnabledExtensionNames = device_extensions.data();

    auto result = vkCreateDevice(physical_device, &device_info, nullptr, &m_device);
    if(result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create device!");
    }

    vkGetDeviceQueue(m_device, id, 0, &m_queue);
    m_physical_device = physical_device;
    m_draw_queue_idx = id; 
    m_present_queue_idx = id; 
    m_present_queue = m_queue;
}
 
void Simulation::setup_surface() {
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, 0, m_surface, &present_support);
}
 
void Simulation::setup_framebuffer() {
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &surface_capabilities); 

    std::cout << "Surface Capabilities:\n";
    std::cout << "\tMin Extent: " << surface_capabilities.minImageExtent.width 
        << "x" << surface_capabilities.minImageExtent.height << "\n";
    std::cout << "\tCurrent Extent: " << surface_capabilities.currentExtent.width 
        << "x" << surface_capabilities.currentExtent.height << "\n";
    std::cout << "\tMax Extent: " << surface_capabilities.maxImageExtent.width << "x"
        << surface_capabilities.maxImageExtent.height << "\n";
    std::cout << "\tMax Images: " << surface_capabilities.maxImageCount << "\n";
    std::cout << "\tMin Images: " << surface_capabilities.minImageCount << "\n";

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &format_count, surface_formats.data());

    std::cout << "Surface Formats: " << std::endl;
    for(auto& format : surface_formats) {
        std::cout << "\tColor Space: " << format.colorSpace << ". Format: ";
        std::cout << format.format << "\n";
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_count, present_modes.data());

    std::cout << "Present Modes: " << std::endl;
    for(auto& mode : present_modes) {
        std::cout << "\t" << mode << "\n";
    }

    m_swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
    m_swapchain_size = choose_swapchain_extent();

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.surface = m_surface;
    createInfo.imageExtent = m_swapchain_size;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.imageArrayLayers = 1;
    createInfo.minImageCount = std::max(surface_capabilities.minImageCount, uint32_t{3});
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.queueFamilyIndexCount = 1;
    createInfo.pQueueFamilyIndices = &m_draw_queue_idx;
    createInfo.imageFormat = m_swapchain_format;
    createInfo.clipped = VK_TRUE;

    VkResult result = vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain);
    if(result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain!");
    }

    uint32_t swapchain_size;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchain_size, nullptr);
    m_swap_chain_images.resize(swapchain_size);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchain_size, m_swap_chain_images.data());

    m_swap_chain_views.resize(createInfo.minImageCount);
    for(uint32_t i = 0; i < createInfo.minImageCount; ++i) {
        VkImageViewCreateInfo view_create = {};
        view_create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create.pNext = nullptr;
        view_create.image = m_swap_chain_images[i];
        view_create.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_create.format = m_swapchain_format;
        view_create.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_create.subresourceRange.baseMipLevel = 0;
        view_create.subresourceRange.levelCount = 1;
        view_create.subresourceRange.baseArrayLayer = 0;
        view_create.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(m_device, &view_create, nullptr, &m_swap_chain_views[i]);
        if(result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view!");
        }
    }
}
 
void Simulation::create_pipeline() {
    auto vert_file = load_shader_file("../src/glsl/vert.spv");
    auto frag_file = load_shader_file("../src/glsl/frag.spv");
    std::cout << vert_file.size() << std::endl;
    std::cout << frag_file.size() << std::endl;

    auto vert_module = make_shader_module(vert_file);
    auto frag_module = make_shader_module(frag_file);

    std::array<VkPipelineShaderStageCreateInfo, 2> stages;

    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].pNext = nullptr;
    stages[0].pSpecializationInfo = nullptr;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_module;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = nullptr;
    stages[1].pSpecializationInfo = nullptr;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_module;
    stages[1].pName = "main";

    auto binding_desc = Vertex::binding_desc();
    auto attrib_desc = Vertex::attrib_desc();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &binding_desc;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attrib_desc.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) m_swapchain_size.width;
    viewport.height = (float) m_swapchain_size.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchain_size;

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
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

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

    auto ubo_binding = Uniforms::binding_desc();
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &ubo_binding;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_desc_set_layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    } 

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_desc_set_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pTessellationState = nullptr;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = m_pipeline_layout;
    pipelineInfo.renderPass = m_render_pass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline!");
    }

    vkDestroyShaderModule(m_device, vert_module, nullptr);
    vkDestroyShaderModule(m_device, frag_module, nullptr);
}
 
std::vector<char> Simulation::load_shader_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::in);

    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> data(size);
    file.read(data.data(), data.size());

    return data;
}
 
VkShaderModule Simulation::make_shader_module(const std::vector<char>& data) {
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = data.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(data.data());
    create_info.flags = 0;

    VkShaderModule module;
    VkResult result = ::vkCreateShaderModule(m_device, &create_info, nullptr, &module);
    if(result != VK_SUCCESS) {
        throw std::runtime_error("Unable to create shader module!");
    }
    return module;
}
 
void Simulation::setup_render_pass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = m_swapchain_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}
 
void Simulation::create_framebuffer() {
    m_framebuffers.resize(m_swap_chain_views.size());

    for(std::size_t i = 0; i < m_framebuffers.size(); ++i) {
        VkImageView attachments[] = {
            m_swap_chain_views[i]
        };

        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = m_render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = m_swapchain_size.width;
        framebuffer_info.height = m_swapchain_size.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create a framebuffer!");
        }
    }
}
 
void Simulation::create_command_pool() {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = m_draw_queue_idx;
    pool_info.flags = 0;

    if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }
}
 
void Simulation::create_command_buffers() {
    m_command_buffers.resize(m_framebuffers.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_command_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) m_command_buffers.size();

    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_command_buffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }

    for(std::size_t i = 0; i < m_command_buffers.size(); ++i) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(m_command_buffers[i], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_render_pass;
        renderPassInfo.framebuffer = m_framebuffers[i];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_swapchain_size;

        VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(m_command_buffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(m_command_buffers[i], 0, 1, &m_vbo, &offset);
        vkCmdBindIndexBuffer(m_command_buffers[i], m_ibo, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(m_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1, 
            &m_descriptor_sets[i], 0, nullptr);
        vkCmdBindPipeline(m_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdDrawIndexed(m_command_buffers[i], 6, 1, 0, 0, 0);

        vkCmdEndRenderPass(m_command_buffers[i]);

        if (vkEndCommandBuffer(m_command_buffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }
}
 
void Simulation::draw_frame() {
    update_ubo();

    uint32_t image_idx;
    auto result = vkAcquireNextImageKHR(m_device, m_swapchain, std::numeric_limits<uint64_t>::max(), 
        m_image_available_semaphore, VK_NULL_HANDLE, &image_idx);

    if(result == VK_ERROR_OUT_OF_DATE_KHR || m_was_resized) {
        rebuild_swapchain();
        m_was_resized = false;
        draw_frame();
        return;
    } else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap image!");
    }

    std::cout << "Acquired swapchain image #" << image_idx << "\n";

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_image_available_semaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = waitSemaphores;
    submit_info.pWaitDstStageMask = waitStages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffers[image_idx];
    VkSemaphore signalSemaphores[] = {m_render_finished_semaphore};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {m_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &image_idx;
    presentInfo.pResults = nullptr; // Optional

    vkQueuePresentKHR(m_present_queue, &presentInfo);
}
 
void Simulation::create_semaphores() {
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_image_available_semaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create semaphore!");
    }
    if(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_render_finished_semaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create semaphore!");
    }
}
 
void Simulation::cleanup_swapchain() {
    for(auto& framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    vkDestroyRenderPass(m_device, m_render_pass, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    for(auto& view : m_swap_chain_views) {
        vkDestroyImageView(m_device, view, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}
 
void Simulation::rebuild_swapchain() {
    vkDeviceWaitIdle(m_device);

    cleanup_swapchain();
    setup_framebuffer();
    setup_render_pass();
    create_pipeline();
    create_framebuffer();
    create_command_buffers();
}
 
VkExtent2D Simulation::choose_swapchain_extent() {
    std::cout << "Rebuild swapchain\n";
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &surface_capabilities); 

    if(surface_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return surface_capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);

        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }
}
 
void Simulation::glfw_resize_callback(GLFWwindow* window, int width, int height) {
    Simulation* sim = reinterpret_cast<Simulation*>(glfwGetWindowUserPointer(window));
    sim->m_was_resized = true;
}
 
void Simulation::create_vbo() {
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 1.0}, {1.0f, 0.0f, 0.0f, 1.0}},
        {{0.5f, -0.5f, 1.0}, {0.0f, 1.0f, 0.0f, 1.0}},
        {{0.5f, 0.5f, 1.0}, {0.0f, 0.0f, 1.0f, 1.0}},
        {{-0.5f, 0.5f, 1.0}, {1.0f, 1.0f, 1.0f, 1.0}},
    };

    VkDeviceSize buffer_size = vertices.size() * sizeof(Vertex);

    auto [buffer, memory] = make_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(m_device, memory, 0, buffer_size, 0, &data);
    std::memcpy(data, vertices.data(), vertices.size()*sizeof(Vertex));
    vkUnmapMemory(m_device, memory);

    auto [device_buffer, device_memory] = make_buffer(buffer_size, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    buffer_copy(buffer, device_buffer, buffer_size);

    vkDestroyBuffer(m_device, buffer, nullptr);
    vkFreeMemory(m_device, memory, nullptr);

    m_vbo = device_buffer;
    m_vbo_mem = device_memory;
}
 
VkVertexInputBindingDescription Vertex::binding_desc() {
    VkVertexInputBindingDescription desc = {};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return desc;
}

std::array<VkVertexInputAttributeDescription, 2> Vertex::attrib_desc() {
    std::array<VkVertexInputAttributeDescription, 2> attrib_desc;

    attrib_desc[0].binding = 0;
    attrib_desc[0].location = 0;
    attrib_desc[0].offset = offsetof(Vertex, pos);
    attrib_desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrib_desc[1].binding = 0;
    attrib_desc[1].location = 1;
    attrib_desc[1].offset = offsetof(Vertex, color);
    attrib_desc[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;

    return attrib_desc;
}

uint32_t Simulation::select_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            std::cout << "Choosing memory " << i << ".\n";
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}
 
std::tuple<VkBuffer, VkDeviceMemory> Simulation::make_buffer(VkDeviceSize size, VkBufferUsageFlags usage, 
    VkMemoryPropertyFlags properties) 
{
    VkBuffer buffer;
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.flags = 0;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.pQueueFamilyIndices = &m_draw_queue_idx;
    bufferInfo.queueFamilyIndexCount = 1;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vertex buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memProperties);

    auto mem_type = select_memory_type(memRequirements.memoryTypeBits, properties);

    std::cout << memRequirements.memoryTypeBits << "\n";
    for(uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        std::cout << "\t" <<memProperties.memoryTypes[i].heapIndex << ":" << memProperties.memoryTypes[i].propertyFlags << std::endl;
    }
    VkDeviceMemory memory;
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = mem_type;

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }
    vkBindBufferMemory(m_device, buffer, memory, 0);

    return {buffer, memory};
}

void Simulation::buffer_copy(VkBuffer source, VkBuffer dest, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_command_pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, source, dest, 1, &copyRegion);
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);

    vkFreeCommandBuffers(m_device, m_command_pool, 1, &commandBuffer);
}
 
void Simulation::create_ibo() {
    std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0
    };

    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    auto [staging_buffer, staging_mem] = make_buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(m_device, staging_mem, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t) bufferSize);
    vkUnmapMemory(m_device, staging_mem);

    auto [dev_buffer, dev_buffer_mem] = make_buffer(bufferSize, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    buffer_copy(staging_buffer, dev_buffer, bufferSize);

    vkDestroyBuffer(m_device, staging_buffer, nullptr);
    vkFreeMemory(m_device, staging_mem, nullptr);

    m_ibo = dev_buffer;
    m_ibo_mem = dev_buffer_mem;
}
 
VkDescriptorSetLayoutBinding Uniforms::binding_desc() {
    VkDescriptorSetLayoutBinding uboLayoutBinding = {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    return uboLayoutBinding;
}
 
VkDescriptorSetLayoutCreateInfo Uniforms::layout_info() {
    VkDescriptorSetLayoutBinding binding = binding_desc();
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    return layoutInfo;
}
 
void Simulation::create_ubo() {
    VkDeviceSize bufferSize = sizeof(Uniforms);

    auto [uniform_buffer, uniform_buffer_mem] = make_buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    m_ubo = uniform_buffer;
    m_ubo_mem = uniform_buffer_mem;
}
 
void Simulation::update_ubo() {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    Uniforms u;
    u.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));;
    u.view = glm::lookAt(glm::vec3(2.0, 2.0, 2.0), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
    u.perspective = glm::perspective(45.0f, static_cast<float>(m_swapchain_size.width) / m_swapchain_size.height, 0.1f, 10.0f);

    void* data;
    vkMapMemory(m_device, m_ubo_mem, 0, sizeof(u), 0, &data);
    std::memcpy(data, &u, sizeof(u));
    vkUnmapMemory(m_device, m_ubo_mem);
}
 
void Simulation::create_descriptor_pool() {
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(m_swap_chain_images.size());

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(m_swap_chain_images.size());;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptor_pool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}
 
void Simulation::create_descriptor_set() {
    std::vector<VkDescriptorSetLayout> layouts(m_swap_chain_images.size(), m_desc_set_layout);
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptor_pool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(m_swap_chain_images.size());
    allocInfo.pSetLayouts = layouts.data();

    m_descriptor_sets.resize(m_swap_chain_images.size());
    if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptor_sets[0]) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < m_swap_chain_images.size(); i++) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = m_ubo;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniforms);

        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptor_sets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr;
        descriptorWrite.pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
    }
}
 
