#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED
#include "glfw/glfw3.h"
#include "daeLoader/daeLoader.h"
#include "vkEngine/imgAndBuf.h"

#define CHK_VK(function)                                                \
do {                                                                    \
    uint32_t errorcode=function;                                        \
    if(errorcode){                                                      \
        printf("Error code %x occured in line %d\n",errorcode,__LINE__);\
        exit(1);                                                        \
    }                                                                   \
} while (0)

#define CHK_VK_ACK(function,action)                                     \
do {                                                                    \
    uint32_t errorcode=function;                                        \
    if(errorcode){                                                      \
        printf("Error code %x occured in line %d\n",errorcode,__LINE__);\
        action;                                                         \
    }                                                                   \
} while (0)


struct eng3dObject{
    struct DataFromDae       daeData;
    VkBuffer                 vtxBufferHandle;
    VkDeviceSize             PosNormUvInBufOffset;
    VkDeviceSize             IdxInBufOffset;
    uint32_t                 vertexCount;
};
DlTypedef_plain(eng3dObj,struct eng3dObject);

struct engExtensionsAndLayers{
    uint32_t    InstExtensionCount;
    char**      InstExtensionNamesPP;
    uint32_t    InstLayerCount;
    char**      InstLayerNamesPP;

    uint32_t    DevExtensionCount;
    char**      DevExtensionNamesPP;
    uint32_t    DevLayerCount;
    char**      DevLayerNamesPP;
};


struct VulkanRuntimeInfo{
    GLFWwindow* mainWindowP;
    VkDevice device;
    VkPhysicalDevice* physAvailDevicesP;
    uint32_t physDeviceCount;
    VkPhysicalDevice physSelectedDevice;
    VkPhysicalDeviceProperties* PhysDevPropP;

    VkInstance instance;
    struct engExtensionsAndLayers _engExtAndLayers;

    uint32_t graphics_queue_family_idx;
    VkQueue  graphics_queue;

    VkSurfaceKHR        surface;

    VkSwapchainKHR      swapChain;
    uint32_t            imagesInFlightCount;
    VkImage*            swapChainImagesP;
    VkExtent2D          swapChainImageExtent;
    VkSurfaceFormatKHR  swapChainFormat;
    VkImageView*        swapChainImageViewsP;

    VkRenderPass renderPass;

    VkShaderModule VertexShaderModule;
    VkShaderModule FragmentShaderModule;

    VkPipeline       graphicsPipeline;
    VkPipelineLayout graphicsPipelineLayout;

    VkFramebuffer* FramebufferP;

    VkCommandBuffer* CommandbufferP;

    VkCommandPool commandPool;
    VkCommandBuffer primaryCommandBuffer;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet* descriptorSetsP;
    VkDescriptorSetLayout* descriptorSetLayoutP;

    VkSemaphore* imageAvailableSemaphoreP;
    VkSemaphore* renderFinishedSemaphoreP;
    VkFence*     ImageAlreadyProcessingFenceP;

    struct eng_AllocBlock* FastUpdateUniformAllocP;
};

#endif // CORE_H_INCLUDED
