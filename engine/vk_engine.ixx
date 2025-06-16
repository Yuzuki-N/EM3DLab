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


export class VkEngine {
public:
    void Init() {
        if (bIsEngineInit)
            return;

        mWindow.Init();
        InitVulkan();
        InitSwapchain();


        bIsEngineInit = true;
    }
    void Run() {
        mWindow.Run();
    }
    void Cleanup() {

        if (bIsEngineInit) {
            vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

            //destroy swapchain resources
            for (int i = 0; i < mSwapchainImageViews.size(); i++) {

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
};