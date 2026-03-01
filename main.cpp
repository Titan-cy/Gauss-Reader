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

VkCommandPool commandPool;  // Stores the commands we send to the GPU
VkBuffer vertexBuffer;      // The GPU container
VkDeviceMemory vertexBufferMemory;  // Actual Physical VRAM


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

// Find the right type of VRAM on the GPU

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties){
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties (physicalDevice, &memProperties);

    for (uint32_t i=0; i < memProperties.memoryTypeCount; i++){
        if((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties){
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable GPY memory type!");
}

// Create a Buffer and Allocate VRAM

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory){
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS){
        throw std::runtime_error("Failed to create Vulkan buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate GPU VRAM!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);

}

// Copy data from CPU Staging Buffer to GPU VRAM

void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    VkCommandBufferBeginInfo beginInfo{};

    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue); // Wait for the copy to finish

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// Main transfer Function
void createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(globalSplats[0]) * globalSplats.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    
    // 1. Create Staging Buffer
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    // 2. Map Memory and Copy the C++ Vector into it
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, globalSplats.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // 3. Create the Actual VRAM Buffer (GPU Only - Super Fast)
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    // 4. Command the GPU to copy from Staging -> VRAM
    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    // 5. Cleanup the Staging Buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    
    std::cout << "Successfully transferred " << globalSplats.size() << " splats to RTX 2050 VRAM!" << std::endl;
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

    for (const auto& dev : devices) {
        if (isDeviceSuitable(dev)) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(dev, &deviceProperties);
            
            // fallback GPU
            physicalDevice = dev;
            
            // If it is a Discrete GPU, lock it
            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                break; 
            }
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        std::cerr << "Failed to find a suitable GPU!" << std::endl;
        return -1;
    }

    // Announce the winner
    DeviceInfo(physicalDevice);

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

    // Setup Command Pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = indices.graphisFamily.value();
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool!" << std::endl;
        return -1;
    }

     // load the gauss file
    loadGaussFile("test_image.gauss");

    // Transfer the Data
    createVertexBuffer();
   
    //cleanup
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
    
}