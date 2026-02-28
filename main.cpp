#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <optional>
#include <fstream>
#include "SplatFormat.h"

// Global Variables

VkInstance instance;
VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
VkDevice device;            // Logical Device
VkQueue graphicsQueue;      // Command Queue

// Global variables to hold our loaded .gauss data

GaussHeader globalHeader;
std::vector<GaussianSplat> globalSplats;

// .gauss CPU paraser

bool loadGaussFile(const std::string& filename){
    std::ifstream file(filename, std::ios::binary);
    if(!file.is_open()){
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return false;
    }
    
    file.read(reinterpret_cast<char*>(&globalHeader), sizeof(GaussHeader));

    if(globalHeader.ID != ID){
        std::cerr << "Error: Not a valid .gauss file ID mismatch" << std::endl;
        return false;
    }

    std::cout << "--- File loaded succesfully ---" << std::endl;
    std::cout << "Resolution: " << globalHeader.width << "x" << globalHeader.height << std::endl;
    std::cout << "Splat Count" << globalHeader.splatCount << std::endl;
    
    // Allocate CPU RAM and read the splats
    globalSplats.resize(globalHeader.splatCount);

    // Read all splats in 1 chunck
    file.read(reinterpret_cast<char*>(globalSplats.data()), globalHeader.splatCount * sizeof(GaussianSplat));

    file.close();
    return true;

}


bool DeviceInfo(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    std::cout << "Found GPU: " << deviceProperties.deviceName;
    
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        std::cout << " (Discrete - PREFERRED)" << std::endl;
        return true; 
    }
    
    std::cout << " (Integrated)" << std::endl;
    return true;
}

struct QueueFamilyIndices{
    std::optional<uint32_t> graphisFamily;
    bool isComplete(){
        return graphisFamily.has_value();
    }

};

QueueFamilyIndices findQueueFamilies (VkPhysicalDevice device){
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for(const auto& queueFamily : queueFamilies){
        if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT){
            indices.graphisFamily = i;
        }

        if(indices.isComplete()){
            break;
        }

        i++;
    }

    return indices;
}

bool isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    return indices.isComplete();
}


// Main Engine loop

int main(){

    // Initialize Window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "SplatXEngine", nullptr, nullptr);

    // Create Instance (fixed)
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName =  "SplatX";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    if(vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS){
        std::cerr << "Failed to create Instance!" << std::endl;
        return -1;
    }

    // Pick Physical Device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& dev : devices){
        if (isDeviceSuitable(dev)){
            physicalDevice = dev;
            DeviceInfo(physicalDevice);
            break;
        }
    }

    // Create Logical Device
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphisFamily.value();
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo logicCreateInfo{};
    logicCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logicCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    logicCreateInfo.queueCreateInfoCount = 1;
    logicCreateInfo.pEnabledFeatures = &deviceFeatures;

    if (vkCreateDevice(physicalDevice, &logicCreateInfo, nullptr, &device) != VK_SUCCESS){
        std::cerr << "Failed to create logical device!" << std::endl;
        return -1;
    }
    vkGetDeviceQueue(device, indices.graphisFamily.value(), 0, &graphicsQueue);
    std::cout << "Logical Device & Queue Created" << std::endl;

    // Test the reader
    loadGaussFile("test_image.gauss");

    //cleanup
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
    
}