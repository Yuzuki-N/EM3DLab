//
// Created by yuri on 15/6/2025.
//
module;
#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
export module VkEngine;
import window;
import vk_initializers;


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

    void Init() {
        if (bIsEngineInit)
            return;

        mWindow.Init();
        InitVulkan();
        InitSwapchain();
        InitCommands();
        InitDefaultRenderpass();
        InitFramebuffers();
        init_sync_structures();

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
    void init_sync_structures() {
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