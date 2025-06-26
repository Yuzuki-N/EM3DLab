//
// Created by yuri on 15/6/2025.
//
module;
#include <fstream>
#include <iostream>
#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define VK_CHECK(x)                                                 \
do                                                              \
{                                                               \
VkResult err = x;                                           \
if (err)                                                    \
{                                                           \
std::cout <<"Detected Vulkan error: " << err << std::endl; \
abort();                                                \
}                                                           \
} while (0)

export module VkEngine;
import window;
import vk_initializers;

export class PipelineBuilder {
public:

    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkViewport _viewport;
    VkRect2D _scissor;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;

    VkPipeline build_pipeline(VkDevice device, VkRenderPass pass) {
        //make viewport state from our stored viewport and scissor.
        //at the moment we won't support multiple viewports or scissors
        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.pNext = nullptr;

        viewportState.viewportCount = 1;
        viewportState.pViewports = &_viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &_scissor;

        //setup dummy color blending. We aren't using transparent objects yet
        //the blending is just "no blend", but we do write to the color attachment
        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.pNext = nullptr;

        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &_colorBlendAttachment;

        //build the actual pipeline
        //we now use all of the info structs we have been writing into into this one to create the pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = nullptr;

        pipelineInfo.stageCount = _shaderStages.size();
        pipelineInfo.pStages = _shaderStages.data();
        pipelineInfo.pVertexInputState = &_vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &_inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &_rasterizer;
        pipelineInfo.pMultisampleState = &_multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = _pipelineLayout;
        pipelineInfo.renderPass = pass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        //it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
        VkPipeline newPipeline;
        if (vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
            std::cout << "failed to create pipeline\n";
            return VK_NULL_HANDLE; // failed to create graphics pipeline
            }
        return newPipeline;
    }
};


export class VkEngine {
public:

    Window mWindow;
    bool bIsEngineInit = false;

    VkInstance mInstance; // Vulkan library handle
    VkDebugUtilsMessengerEXT mDebugMessenger; // Vulkan debug output handle
    VkPhysicalDevice mChosenGPU; // GPU chosen as the default device
    VkDevice mDevice; // Vulkan device for commands
    VkSurfaceKHR mSurface; // Vulkan window surface

    VkSwapchainKHR mSwapchain;
    VkFormat mSwapchainImageFormat;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;

    VkQueue mGraphicsQueue; //queue we will submit to
    uint32_t mGraphicsQueueFamily; //family of that queue

    VkCommandPool mCommandPool; //the command pool for our commands
    VkCommandBuffer mMainCommandBuffer; //the buffer we will record into
    VkRenderPass mRenderPass;
    std::vector<VkFramebuffer> mFramebuffers;

    VkSemaphore mPresentSemaphore, mRenderSemaphore;
    VkFence mRenderFence;
    VkExtent2D mWindowExtent;
    uint32_t mFrameNumber;

    VkPipelineLayout _trianglePipelineLayout;
    VkPipeline _trianglePipeline;


    void Init() {
        if (bIsEngineInit)
            return;

        mWindow.Init();
        InitVulkan();
        InitSwapchain();
        InitCommands();
        InitDefaultRenderpass();
        InitFramebuffers();
        InitSyncStructures();
        InitPipeline();

        bIsEngineInit = true;
    }
    void Run() {
        Draw();
    }
    void Cleanup() {
        if (bIsEngineInit) {
            vkDeviceWaitIdle(mDevice);
            vkDestroyFence(mDevice, mRenderFence, nullptr);
            vkDestroySemaphore(mDevice, mPresentSemaphore, nullptr);
            vkDestroySemaphore(mDevice, mRenderSemaphore, nullptr);
            vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
            vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
            vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

            //destroy swapchain resources
            for (int i = 0; i < mSwapchainImageViews.size(); i++) {
                vkDestroyFramebuffer(mDevice, mFramebuffers[i], nullptr);
                vkDestroyImageView(mDevice, mSwapchainImageViews[i], nullptr);
            }

            vkDestroyDevice(mDevice, nullptr);
            vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
            vkb::destroy_debug_utils_messenger(mInstance, mDebugMessenger);
            vkDestroyInstance(mInstance, nullptr);
        }
        mWindow.Cleanup();
    }
    ~VkEngine() {
        Cleanup();
    }

    void InitVulkan() {
        mWindowExtent = VkExtent2D(mWindow.width, mWindow.height);
        vkb::InstanceBuilder builder;

        //make the Vulkan instance, with basic debug features
        auto inst_ret = builder.set_app_name("EM3DLab")
            .request_validation_layers(true)
            .require_api_version(1, 1, 0)
            .use_default_debug_messenger()
            .build();

        vkb::Instance vkb_inst = inst_ret.value();

        //store the instance
        mInstance = vkb_inst.instance;
        //store the debug messenger
        mDebugMessenger = vkb_inst.debug_messenger;

        // get the surface of the window we opened with SDL
        SDL_Vulkan_CreateSurface(mWindow.mWindow, mInstance, nullptr, &mSurface);

        //use vkbootstrap to select a GPU.
        //We want a GPU that can write to the SDL surface and supports Vulkan 1.1
        vkb::PhysicalDeviceSelector selector{ vkb_inst };
        vkb::PhysicalDevice physicalDevice = selector
            .set_minimum_version(1, 1)
            .set_surface(mSurface)
            .select()
            .value();

        //create the final Vulkan device
        vkb::DeviceBuilder deviceBuilder{ physicalDevice };

        vkb::Device vkbDevice = deviceBuilder.build().value();

        // Get the VkDevice handle used in the rest of a Vulkan application
        mDevice = vkbDevice.device;
        mChosenGPU = physicalDevice.physical_device;

        // use vkbootstrap to get a Graphics queue
        mGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    }

    void InitSwapchain() {
        vkb::SwapchainBuilder swapchainBuilder{mChosenGPU,mDevice,mSurface };

        vkb::Swapchain vkbSwapchain = swapchainBuilder
            .use_default_format_selection()
            //use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(mWindow.width, mWindow.height)
            .build()
            .value();

        //store swapchain and its related images
        mSwapchain = vkbSwapchain.swapchain;
        mSwapchainImages = vkbSwapchain.get_images().value();
        mSwapchainImageViews = vkbSwapchain.get_image_views().value();

        mSwapchainImageFormat = vkbSwapchain.image_format;
    }

    void InitCommands() {
        //create a command pool for commands submitted to the graphics queue.
        //we also want the pool to allow for resetting of individual command buffers
        VkCommandPoolCreateInfo commandPoolInfo = vkinit::CommandPoolCreateInfo(mGraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        (vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &mCommandPool));

        //allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::CommandBufferAllocateInfo(mCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        (vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &mMainCommandBuffer));
    }

    void InitDefaultRenderpass() {
        // the renderpass will use this color attachment.
        VkAttachmentDescription colorAttachment = {};
        //the attachment will have the format needed by the swapchain
        colorAttachment.format = mSwapchainImageFormat;
        //1 sample, we won't be doing MSAA
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // we Clear when this attachment is loaded
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // we keep the attachment stored when the renderpass ends
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        //we don't care about stencil
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        //we don't know or care about the starting layout of the attachment
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        //after the renderpass ends, the image has to be on a layout ready for display
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef = {};
        //attachment number will index into the pAttachments array in the parent renderpass itself
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        //we are going to create 1 subpass, which is the minimum you can do
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        //connect the color attachment to the info
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        //connect the subpass to the info
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;


        (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass));
    }

    void InitFramebuffers() {
        //create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.pNext = nullptr;

        fbInfo.renderPass = mRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.width = mWindow.width;
        fbInfo.height = mWindow.height;
        fbInfo.layers = 1;

        //grab how many images we have in the swapchain
        const uint32_t swapchainImagecount = mSwapchainImages.size();
        mFramebuffers = std::vector<VkFramebuffer>(swapchainImagecount);

        //create framebuffers for each of the swapchain image views
        for (int i = 0; i < swapchainImagecount; i++) {

            fbInfo.pAttachments = &mSwapchainImageViews[i];
            (vkCreateFramebuffer(mDevice, &fbInfo, nullptr, &mFramebuffers[i]));
        }
    }
    void InitSyncStructures() {
        //create synchronization structures
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = nullptr;

        //we want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        (vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &mRenderFence));

        //for the semaphores we don't need any flags
        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.pNext = nullptr;
        semaphoreCreateInfo.flags = 0;

        (vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mPresentSemaphore));
        (vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mRenderSemaphore));
    }

    bool LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule) {
        //open the file. With cursor at the end
        // ate means "at the end"?
        std::ifstream file(filePath, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            return false;
        }
        //find what the size of the file is by looking up the location of the cursor
        //because the cursor is at the end, it gives the size directly in bytes
        size_t fileSize = file.tellg();
        //spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
        //put file cursor at beginning
        file.seekg(0);
        //load the entire file into the buffer
        file.read((char*)buffer.data(), fileSize);
        //now that the file is loaded into the buffer, we can close it
        file.close();

        //create a new shader module, using the buffer we loaded
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pNext = nullptr;
        //codeSize has to be in bytes, so multiply the ints in the buffer by size of int to know the real size of the buffer
        createInfo.codeSize = buffer.size() * sizeof(uint32_t);
        createInfo.pCode = buffer.data();
        //check that the creation goes well.
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            return false;
        }
        *outShaderModule = shaderModule;
        return true;

    }

    void InitPipeline() {
        VkShaderModule triangleFragShader;
        if (!LoadShaderModule("./Shaders/triangle.frag.spv", &triangleFragShader))
        {
            std::cout << "Error when building the triangle fragment shader module" << std::endl;
        }
        else {
            std::cout << "Triangle fragment shader successfully loaded" << std::endl;
        }

        VkShaderModule triangleVertexShader;
        if (!LoadShaderModule("./Shaders/triangle.vert.spv", &triangleVertexShader))
        {
            std::cout << "Error when building the triangle vertex shader module" << std::endl;

        }
        else {
            std::cout << "Triangle vertex shader successfully loaded" << std::endl;
        }

        VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();

        VK_CHECK(vkCreatePipelineLayout(mDevice, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));


        PipelineBuilder pipelineBuilder;
        pipelineBuilder._shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader));
        pipelineBuilder._shaderStages.push_back(
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

        //vertex input controls how to read vertices from vertex buffers. We aren't using it yet
        pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

        //input assembly is the configuration for drawing triangle lists, strips, or individual points.
        //we are just going to draw triangle list
        pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        //build viewport and scissor from the swapchain extents
        pipelineBuilder._viewport.x = 0.0f;
        pipelineBuilder._viewport.y = 0.0f;
        pipelineBuilder._viewport.width = (float)mWindowExtent.width;
        pipelineBuilder._viewport.height = (float)mWindowExtent.height;
        pipelineBuilder._viewport.minDepth = 0.0f;
        pipelineBuilder._viewport.maxDepth = 1.0f;

        pipelineBuilder._scissor.offset = { 0, 0 };
        pipelineBuilder._scissor.extent = mWindowExtent;

        //configure the rasterizer to draw filled triangles
        pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

        //we don't use multisampling, so just run the default one
        pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

        //a single blend attachment with no blending and writing to RGBA
        pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

        //use the triangle layout we created
        pipelineBuilder._pipelineLayout = _trianglePipelineLayout;

        //finally build the pipeline
        _trianglePipeline = pipelineBuilder.build_pipeline(mDevice, mRenderPass);
    }

    void Draw() {
        SDL_Event e;
        bool bQuit = false;

        while (!bQuit)
        {
            while (SDL_PollEvent(&e) != 0)
            {
                //close the window when user alt-f4s or clicks the X button
                if (e.type == SDL_EVENT_QUIT) bQuit = true;
            }

            EngineDraw();
        }
    }

    void EngineDraw() {
        //wait until the GPU has finished rendering the last frame. Timeout of 1 second
        (vkWaitForFences(mDevice, 1, &mRenderFence, true, 1'000'000'000));
        (vkResetFences(mDevice, 1, &mRenderFence));

        //request image from the swapchain, one second timeout
        uint32_t swapchainImageIndex;
        // mPresentSemaphore to signal.
        (vkAcquireNextImageKHR(mDevice, mSwapchain, 1'000'000'000, mPresentSemaphore, nullptr, &swapchainImageIndex));

        //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
        vkResetCommandBuffer(mMainCommandBuffer, 0);

        //naming it cmd for shorter writing
        VkCommandBuffer cmd = mMainCommandBuffer;

        //begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
        VkCommandBufferBeginInfo cmdBeginInfo = {};
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.pNext = nullptr;

        cmdBeginInfo.pInheritanceInfo = nullptr;
        cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &cmdBeginInfo);

        //make a clear-color from frame number. This will flash with a 120*pi frame period.
        VkClearValue clearValue;
        float flash = abs(sin(mFrameNumber / 120.f));
        clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

        //start the main renderpass.
        //We will use the clear color from above, and the framebuffer of the index the swapchain gave us
        VkRenderPassBeginInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.pNext = nullptr;

        rpInfo.renderPass = mRenderPass;
        rpInfo.renderArea.offset.x = 0;
        rpInfo.renderArea.offset.y = 0;
        rpInfo.renderArea.extent = mWindowExtent;
        rpInfo.framebuffer = mFramebuffers[swapchainImageIndex];

        //connect clear values
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        //once we start adding rendering commands, they will go here

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        //finalize the render pass
        vkCmdEndRenderPass(cmd);

        //finalize the command buffer (we can no longer add commands, but it can now be executed)
        (vkEndCommandBuffer(cmd));



        //prepare the submission to the queue.
        //we want to wait on the mPresentSemaphore, as that semaphore is signaled when the swapchain is ready
        //we will signal the mRenderSemaphore, to signal that rendering has finished

        VkSubmitInfo submit = {};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.pNext = nullptr;

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        submit.pWaitDstStageMask = &waitStage;

        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &mPresentSemaphore;

        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &mRenderSemaphore;

        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        //submit command buffer to the queue and execute it.
        // mRenderFence will now block until the graphic commands finish execution
        (vkQueueSubmit(mGraphicsQueue, 1, &submit, mRenderFence));

        // this will put the image we just rendered into the visible window.
        // we want to wait on the mRenderSemaphore for that,
        // as it's necessary that drawing commands have finished before the image is displayed to the user
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;

        presentInfo.pSwapchains = &mSwapchain;
        presentInfo.swapchainCount = 1;

        presentInfo.pWaitSemaphores = &mRenderSemaphore;
        presentInfo.waitSemaphoreCount = 1;

        presentInfo.pImageIndices = &swapchainImageIndex;

        (vkQueuePresentKHR(mGraphicsQueue, &presentInfo));

        //increase the number of frames drawn
        mFrameNumber++;


    }
};

