#ifndef SIMULATION_H_
#define SIMULATION_H_

#include <array>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec4 color;

    static VkVertexInputBindingDescription binding_desc();
    static std::array<VkVertexInputAttributeDescription, 2> attrib_desc();
};

struct Uniforms {
    glm::mat4 perspective;
    glm::mat4 view;
    glm::mat4 model;

    static VkDescriptorSetLayoutBinding binding_desc();
    static VkDescriptorSetLayoutCreateInfo layout_info();
};

class Simulation {
public:
    Simulation();
    ~Simulation();

    Simulation(const Simulation& other) = delete;
    Simulation(Simulation&& other) noexcept = delete;
    Simulation& operator =(const Simulation& other) = delete;
    Simulation& operator =(Simulation&& other) noexcept = delete;

    void run();

private:
    void cleanup_swapchain();

    void initialize();
    void create_window();
    void setup_device();
    void setup_debug_callback();
    void setup_surface();
    void setup_framebuffer();
    void setup_render_pass();
    void create_pipeline();
    void create_framebuffer();
    void create_command_pool();
    void create_command_buffers();
    void create_semaphores();
    void create_descriptor_pool();
    uint32_t select_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void create_vbo();
    void create_ibo();
    void create_ubo();
    void create_descriptor_set();

    void update_ubo();

    void rebuild_swapchain();
    VkExtent2D choose_swapchain_extent();
    static void glfw_resize_callback(GLFWwindow* window, int width, int height);

    void draw_frame();

    VkPhysicalDevice select_physical_device();
    void make_logical_device();
    std::tuple<VkBuffer, VkDeviceMemory> make_buffer(VkDeviceSize size, VkBufferUsageFlags usage, 
        VkMemoryPropertyFlags properties);
    void buffer_copy(VkBuffer source, VkBuffer dest, VkDeviceSize size);

    static std::vector<char> load_shader_file(const std::string& filename);
    VkShaderModule make_shader_module(const std::vector<char>& data);

    std::vector<const char*> get_extension_layers();
    std::vector<const char*> get_instance_extensions();

    GLFWwindow* m_window;

    uint32_t m_draw_queue_idx;
    uint32_t m_present_queue_idx;
    VkFormat m_swapchain_format;
    VkExtent2D m_swapchain_size;
    bool m_was_resized = false;

    VkInstance m_instance;
    VkDevice m_device;
    VkPhysicalDevice m_physical_device;
    VkDebugReportCallbackEXT m_debug_callback;
    VkSurfaceKHR m_surface;
    VkSwapchainKHR m_swapchain;
    VkQueue m_queue;
    VkQueue m_present_queue;
    VkRenderPass m_render_pass;
    VkDescriptorSetLayout m_desc_set_layout;
    VkPipelineLayout m_pipeline_layout;
    VkPipeline m_pipeline;
    VkCommandPool m_command_pool;
    VkDescriptorPool m_descriptor_pool;

    VkBuffer m_vbo;
    VkDeviceMemory m_vbo_mem;
    VkBuffer m_ibo;
    VkDeviceMemory m_ibo_mem;
    VkBuffer m_ubo;
    VkDeviceMemory m_ubo_mem;

    VkSemaphore m_image_available_semaphore;
    VkSemaphore m_render_finished_semaphore;

    std::vector<VkImageView> m_swap_chain_views;
    std::vector<VkImage> m_swap_chain_images;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_command_buffers;
    std::vector<VkDescriptorSet> m_descriptor_sets;
};

#endif
