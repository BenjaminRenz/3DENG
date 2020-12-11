#include <stdio.h>
#define GLFW_INCLUDE_VULKAN
#include "vulkan/vulkan.h"
#include "submodules/glfw/glfw3.h"

#include "submodules/xmlReader/debug.h"
#include "submodules/xmlReader/xmlReader.h"
#include "submodules/xmlReader/stringutils.h"

/*struct vulkanObj{

};*/

int loadDaeObject(char* filePath,char* meshName,struct vulkanObj* outputVulkanObjectP){
    dprintf(DBGT_INFO,"%d,%d,%d,%d",sizeof(uint32_t),sizeof(void*),sizeof(void**),sizeof(unsigned int));
    struct DynamicList* meshID=stringToUTF32Dynlist(meshName);

    FILE* cylinderDaeFileP=fopen(filePath,"rb");
    struct xmlTreeElement* xmlDaeRootP=0;
    readXML(cylinderDaeFileP,&xmlDaeRootP);
    fclose(cylinderDaeFileP);
    //printXMLsubelements(xmlDaeRootP);

    struct xmlTreeElement* xmlColladaElementP=getNthSubelement(xmlDaeRootP,0);
    struct xmlTreeElement* xmlLibGeoElementP=getFirstSubelementWith_freeArg2345(xmlColladaElementP,stringToUTF32Dynlist("library_geometries"),NULL,NULL,NULL,0);
    struct xmlTreeElement* xmlGeoElementP=getFirstSubelementWith_freeArg2345(xmlLibGeoElementP,stringToUTF32Dynlist("geometry"),stringToUTF32Dynlist("id"),DlDuplicate(sizeof(uint32_t),meshID),NULL,0); //does  not work?
    struct xmlTreeElement* xmlMeshElementP=getFirstSubelementWith_freeArg2345(xmlGeoElementP,stringToUTF32Dynlist("mesh"),NULL,NULL,NULL,0);

    //Get Triangles
    struct xmlTreeElement* xmlTrianglesP=getFirstSubelementWith_freeArg2345(xmlMeshElementP,stringToUTF32Dynlist("triangles"),NULL,NULL,NULL,0);
    struct DynamicList* TrianglesCountString=getValueFromKeyName_freeArg2(xmlTrianglesP->attributes,stringToUTF32Dynlist("count"));
    struct DynamicList* TrianglesCount=utf32dynlistToInts64_freeArg1(createCharMatchList(2,' ',' '),TrianglesCountString);
    dprintf(DBGT_INFO,"Model has %d triangles",((int64_t*)TrianglesCount->items)[0]);

    struct xmlTreeElement* xmlTrianglesOrderP=getFirstSubelementWith_freeArg2345(xmlTrianglesP,stringToUTF32Dynlist("p"),NULL,NULL,NULL,0);
    struct xmlTreeElement* xmlTrianglesOrderContentP=getNthSubelementOrMisc(xmlTrianglesOrderP,0);
    struct DynamicList* TrianglesOrder=utf32dynlistToInts64_freeArg1(createCharMatchList(2,' ',' '),xmlTrianglesOrderContentP->content);
    dprintf(DBGT_INFO,"Found Triangle order list with %d entries",TrianglesOrder->itemcnt);

    //Get Normals
    struct xmlTreeElement* xmlNormalsSourceP=getFirstSubelementWith_freeArg2345(xmlMeshElementP,stringToUTF32Dynlist("source"),
                                                                                                stringToUTF32Dynlist("id"),
                                                                                                DlCombine_freeArg3(sizeof(uint32_t),meshID,stringToUTF32Dynlist("-normals")),
                                                                                                NULL,0);
    struct xmlTreeElement* xmlNormalsFloatP=getFirstSubelementWith_freeArg2345(xmlNormalsSourceP,stringToUTF32Dynlist("float_array"),
                                                                                                stringToUTF32Dynlist("id"),
                                                                                                DlCombine_freeArg3(sizeof(uint32_t),meshID,stringToUTF32Dynlist("-normals-array")),
                                                                                                NULL,0);
    struct DynamicList* NormalsCountString=getValueFromKeyName_freeArg2(xmlNormalsFloatP->attributes,stringToUTF32Dynlist("count"));
    printUTF32Dynlist(NormalsCountString);
    struct DynamicList* NormalsCount=utf32dynlistToInts64_freeArg1(createCharMatchList(2,' ',' '),NormalsCountString);
    dprintf(DBGT_INFO,"Model has count %d normal coordinates",((int64_t*)NormalsCount->items)[0]);
    struct xmlTreeElement* xmlNormalsFloatContentP=getNthSubelementOrMisc(xmlNormalsFloatP,0);
    struct DynamicList* NormalsDlP=utf32dynlistToFloats_freeArg123(createCharMatchList(4,' ',' ','\t','\t'),createCharMatchList(4,'e','e','E','E'),createCharMatchList(2,'.','.'),xmlNormalsFloatContentP->content);
    dprintf(DBGT_INFO,"Model has %d normal coordinates",NormalsDlP->itemcnt);

    //Get Positions
    struct xmlTreeElement* xmlPositionsSourceP=getFirstSubelementWith_freeArg2345(xmlMeshElementP,stringToUTF32Dynlist("source"),
                                                                     stringToUTF32Dynlist("id"),DlCombine_freeArg3(sizeof(uint32_t),meshID,stringToUTF32Dynlist("-positions")),NULL,0);

    struct xmlTreeElement* xmlPositionsFloatP=getFirstSubelementWith_freeArg2345(xmlPositionsSourceP,stringToUTF32Dynlist("float_array"),
                                                                    stringToUTF32Dynlist("id"),DlCombine_freeArg3(sizeof(uint32_t),meshID,stringToUTF32Dynlist("-positions-array")),NULL,0);

    struct DynamicList* PositionsCountString=getValueFromKeyName_freeArg2(xmlPositionsFloatP->attributes,stringToUTF32Dynlist("count"));
    struct DynamicList* PositionsCount=utf32dynlistToInts64_freeArg1(createCharMatchList(2,' ',' '),PositionsCountString);
    //if(xmlPositionsFloatP->content->type!=dynlisttype_utf32chars){dprintf(DBGT_ERROR,"Invalid content type");return 0;}
    struct xmlTreeElement* xmlPositionsFloatContentP=getNthSubelementOrMisc(xmlPositionsFloatP,0);
    struct DynamicList* PositionsDlP=utf32dynlistToFloats_freeArg123(createCharMatchList(4,' ',' ','\t','\t'),createCharMatchList(4,'e','e','E','E'),createCharMatchList(2,'.','.'),xmlPositionsFloatContentP->content);

}

struct VulkanRuntimeInfo{
    GLFWwindow* mainWindowP;
    VkInstance instance;
    VkDevice device;
    uint32_t graphics_queue_family_idx;
    VkSwapchainKHR swapChain;
    VkCommandPool commandPool;
    VkCommandBuffer primaryCommandBuffer;
};

void eng_vulkan_create_instance(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    VkApplicationInfo AppInfo;
    AppInfo.sType=              VK_STRUCTURE_TYPE_APPLICATION_INFO;
    AppInfo.pNext=              NULL;
    AppInfo.apiVersion=         VK_API_VERSION_1_1;
    AppInfo.pApplicationName=   "Test Application";
    AppInfo.applicationVersion= VK_MAKE_VERSION(1,0,0);
    AppInfo.pEngineName=        "3DENG";
    AppInfo.engineVersion=      VK_MAKE_VERSION(1,0,0);


    uint32_t glfwExtensionsCount=0;
    const char** glfwExtensions=glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
    const char* validationLayers[1]={"VK_LAYER_KHRONOS_validation"};

    VkInstanceCreateInfo CreateInfo;
    CreateInfo.sType=                   VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    CreateInfo.pApplicationInfo=        &AppInfo;
    CreateInfo.pNext=                   NULL;
    CreateInfo.enabledExtensionCount=   glfwExtensionsCount;
    CreateInfo.ppEnabledExtensionNames= glfwExtensions;
    CreateInfo.enabledLayerCount=       0;
    //TODO enable validation layers

    VkInstance instance;
    if(vkCreateInstance(&CreateInfo,NULL,&(vkRuntimeInfoP->instance))!=VK_SUCCESS){
        dprintf(DBGT_ERROR,"Could not create vulkan instance");
        exit(1);
    }
}

void eng_vulkan_pick_device(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    uint32_t physicalDevicesCount=0;
    vkEnumeratePhysicalDevices(vkRuntimeInfoP->instance,&physicalDevicesCount,NULL);
    VkPhysicalDevice* physicalDevicesP=(VkPhysicalDevice*)malloc(physicalDevicesCount*sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(vkRuntimeInfoP->instance,&physicalDevicesCount,physicalDevicesP);
    uint32_t physicalDevicesIdx;
    for(physicalDevicesIdx=0;physicalDevicesIdx<physicalDevicesCount;physicalDevicesIdx++){
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevicesP[physicalDevicesIdx],&deviceProperties);
        if(deviceProperties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
            break;
        }
    }
    if(physicalDevicesCount==0||physicalDevicesIdx==physicalDevicesCount){
        dprintf(DBGT_ERROR,"No supported GPU found in your System");
        exit(1);
    }

    //Print supported Queues
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevicesP[physicalDevicesIdx],&queueFamilyCount,NULL);
    dprintf(DBGT_INFO,"Found %d queueFamilys",queueFamilyCount);
    VkQueueFamilyProperties* queueFamiliyPropP=(VkQueueFamilyProperties*)malloc(queueFamilyCount*sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevicesP[physicalDevicesIdx],&queueFamilyCount,queueFamiliyPropP);
    uint32_t queueFamilyIdx;
    for(queueFamilyIdx=0;queueFamilyIdx<queueFamilyCount;queueFamilyIdx++){
        dprintf(DBGT_INFO,"Found Queue with Count %d\n Properties:\nGRAP\t COMP\t TRANS\t SPARSE\t PROT\n%d \t %d \t %d\t %d\t %d",
                queueFamiliyPropP[queueFamilyIdx].queueCount,
                (queueFamiliyPropP[queueFamilyIdx].queueFlags&VK_QUEUE_GRAPHICS_BIT         )/VK_QUEUE_GRAPHICS_BIT,
                (queueFamiliyPropP[queueFamilyIdx].queueFlags&VK_QUEUE_COMPUTE_BIT          )/VK_QUEUE_COMPUTE_BIT,
                (queueFamiliyPropP[queueFamilyIdx].queueFlags&VK_QUEUE_TRANSFER_BIT         )/VK_QUEUE_TRANSFER_BIT,
                (queueFamiliyPropP[queueFamilyIdx].queueFlags&VK_QUEUE_SPARSE_BINDING_BIT   )/VK_QUEUE_SPARSE_BINDING_BIT,
                (queueFamiliyPropP[queueFamilyIdx].queueFlags&VK_QUEUE_PROTECTED_BIT        )/VK_QUEUE_PROTECTED_BIT
        );
        if((queueFamiliyPropP[queueFamilyIdx].queueFlags&(VK_QUEUE_GRAPHICS_BIT))&&(queueFamiliyPropP[queueFamilyIdx].queueFlags&VK_QUEUE_COMPUTE_BIT)){

            break;
        }
    }
    vkRuntimeInfoP->graphics_queue_family_idx=queueFamilyIdx;
    for(uint32_t RemainingQueueFamilyIdx=queueFamilyIdx+1;RemainingQueueFamilyIdx<queueFamilyCount;RemainingQueueFamilyIdx++){
        dprintf(DBGT_INFO,"Found Queue with Count %d\n Properties:\nGRAP\t COMP\t TRANS\t SPARSE\t PROT\n%d \t %d \t %d\t %d\t %d",
         queueFamiliyPropP[RemainingQueueFamilyIdx].queueCount,
        (queueFamiliyPropP[RemainingQueueFamilyIdx].queueFlags&VK_QUEUE_GRAPHICS_BIT         )/VK_QUEUE_GRAPHICS_BIT,
        (queueFamiliyPropP[RemainingQueueFamilyIdx].queueFlags&VK_QUEUE_COMPUTE_BIT          )/VK_QUEUE_COMPUTE_BIT,
        (queueFamiliyPropP[RemainingQueueFamilyIdx].queueFlags&VK_QUEUE_TRANSFER_BIT         )/VK_QUEUE_TRANSFER_BIT,
        (queueFamiliyPropP[RemainingQueueFamilyIdx].queueFlags&VK_QUEUE_SPARSE_BINDING_BIT   )/VK_QUEUE_SPARSE_BINDING_BIT,
        (queueFamiliyPropP[RemainingQueueFamilyIdx].queueFlags&VK_QUEUE_PROTECTED_BIT        )/VK_QUEUE_PROTECTED_BIT
        );
    }
    if(queueFamilyIdx>=queueFamilyCount){
        dprintf(DBGT_ERROR,"Your GPU does not support a combined Graphics and Compute Queue. Hint: This can be used to justify buying a new GPU...");
        exit(1);
    }


    //Create logical device
    float queuePriority=1.0f;

    VkDeviceQueueCreateInfo QueueCreateInfo;
    QueueCreateInfo.sType=              VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    QueueCreateInfo.queueCount=         1;
    QueueCreateInfo.queueFamilyIndex=   queueFamilyIdx;
    QueueCreateInfo.pQueuePriorities=   &queuePriority;
    QueueCreateInfo.pNext           =   NULL;

    VkDeviceCreateInfo DevCreateInfo;
    DevCreateInfo.sType=                VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DevCreateInfo.pQueueCreateInfos=    &QueueCreateInfo;
    DevCreateInfo.queueCreateInfoCount= 1;
    DevCreateInfo.pNext=                NULL;
    DevCreateInfo.enabledExtensionCount=0;
    DevCreateInfo.ppEnabledExtensionNames=NULL;
    DevCreateInfo.pEnabledFeatures=     NULL;

    if(vkCreateDevice(physicalDevicesP[physicalDevicesIdx],&DevCreateInfo,NULL,&(vkRuntimeInfoP->device))){
        dprintf(DBGT_ERROR,"Could not create Vulkan logical device");
        exit(1);
    }
    free(queueFamiliyPropP);
    free(physicalDevicesP);

}

void eng_vulkan_create_command_buffers(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    //Create Pool
    VkCommandPoolCreateInfo CommandPoolCreateInfo;
    CommandPoolCreateInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolCreateInfo.pNext=NULL;
    CommandPoolCreateInfo.flags=0;
    CommandPoolCreateInfo.queueFamilyIndex=vkRuntimeInfoP->graphics_queue_family_idx;
    if(VK_SUCCESS!=vkCreateCommandPool(vkRuntimeInfoP->device,&CommandPoolCreateInfo,NULL,&(vkRuntimeInfoP->commandPool))){
        dprintf(DBGT_ERROR,"Could not create Command Pool");
    }

    //Create Buffer
    VkCommandBufferAllocateInfo CommandBufferCreateInfo;
    CommandBufferCreateInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferCreateInfo.pNext=NULL;
    CommandBufferCreateInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferCreateInfo.commandPool=vkRuntimeInfoP->commandPool;
    CommandBufferCreateInfo.commandBufferCount=1;
    if(VK_SUCCESS!=vkAllocateCommandBuffers(vkRuntimeInfoP->device,&CommandBufferCreateInfo,&(vkRuntimeInfoP->primaryCommandBuffer)));
}

void cleanup(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    //vkDestroySwapchainKHR(vkRuntimeInfoP->device, vkRuntimeInfoP->swapChain, NULL);
    /*vkDestroyDevice();
    vkDestroySurfaceKHR();
    vkDestroyInstance();*/
    glfwDestroyWindow(vkRuntimeInfoP->mainWindowP);
    glfwTerminate();
}

int main(int argc, char** argv){
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* mainWindowP = glfwCreateWindow(1920, 1080, "3DENG", NULL, NULL);

    struct VulkanRuntimeInfo engVkRuntimeInfo;
    eng_vulkan_create_instance(&engVkRuntimeInfo);
    eng_vulkan_pick_device(&engVkRuntimeInfo);
    //eng_vulkan_add_device(&engVkRuntimeInfo);
    while (!glfwWindowShouldClose(mainWindowP)) {
        glfwPollEvents();
    }


}
