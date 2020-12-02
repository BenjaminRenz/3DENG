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

int main(int argc, char** argv){
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* mainWindowP = glfwCreateWindow(1920, 1080, "3DENG", NULL, NULL);

    VkApplicationInfo AppInfo;
    AppInfo.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    AppInfo.pNext=NULL;
    AppInfo.apiVersion=VK_API_VERSION_1_1;
    AppInfo.pApplicationName="Test Application";
    AppInfo.applicationVersion=VK_MAKE_VERSION(1,0,0);
    AppInfo.pEngineName="3DENG";
    AppInfo.engineVersion=VK_MAKE_VERSION(1,0,0);


    VkInstanceCreateInfo CreateInfo;
    CreateInfo.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    CreateInfo.pApplicationInfo=&AppInfo;
    CreateInfo.pNext=NULL;
    uint32_t glfwExtensionsCount=0;
    const char** glfwExtensions=glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
    const char* validationLayers[1]={"VK_LAYER_KHRONOS_validation"};
    CreateInfo.enabledExtensionCount=glfwExtensionsCount;
    CreateInfo.ppEnabledExtensionNames=glfwExtensions;
    CreateInfo.enabledLayerCount=0;
    //TODO enable validation layers

    VkInstance instance;
    if(vkCreateInstance(&CreateInfo,NULL,&instance)!=VK_SUCCESS){
        dprintf(DBGT_ERROR,"Could not create vulkan instance");
        exit(1);
    }

    //Pick physical device
    uint32_t physicalDevicesCount=0;
    vkEnumeratePhysicalDevices(instance,&physicalDevicesCount,NULL);
    VkPhysicalDevice* physicalDevicesP=(VkPhysicalDevice*)malloc(physicalDevicesCount*sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(instance,&physicalDevicesCount,physicalDevicesP);
    uint32_t physicalDevicesIdx;
    for(physicalDevicesIdx=0;physicalDevicesIdx<physicalDevicesCount;physicalDevicesIdx++){
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevicesP[physicalDevicesIdx],&deviceProperties);
        if(deviceProperties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
            break;
        }
    }
    free(physicalDevicesP);
    //Print supported Queues
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevicesP[physicalDevicesIdx],&queueFamilyCount,NULL);
    dprintf(DBGT_INFO,"Found %d queueFamilys",queueFamilyCount);
    VkQueueFamilyProperties* queueFamiliesP=(VkQueueFamilyProperties*)malloc(queueFamilyCount*sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevicesP[physicalDevicesIdx],&queueFamilyCount,queueFamiliesP);
    for(uint32_t queueFamilyIdx=0;queueFamilyIdx<queueFamilyCount;queueFamilyIdx++){
        dprintf(DBGT_INFO,"Found Queue with Count %d\n Properties:\nGRAP\t COMP\t TRANS\t SPARSE\t PROT\n%d \t %d \t %d\t %d\t %d",
                queueFamiliesP[queueFamilyIdx].queueCount,
                (queueFamiliesP[queueFamilyIdx].queueFlags&VK_QUEUE_GRAPHICS_BIT         )/VK_QUEUE_GRAPHICS_BIT,
                (queueFamiliesP[queueFamilyIdx].queueFlags&VK_QUEUE_COMPUTE_BIT          )/VK_QUEUE_COMPUTE_BIT,
                (queueFamiliesP[queueFamilyIdx].queueFlags&VK_QUEUE_TRANSFER_BIT         )/VK_QUEUE_TRANSFER_BIT,
                (queueFamiliesP[queueFamilyIdx].queueFlags&VK_QUEUE_SPARSE_BINDING_BIT   )/VK_QUEUE_SPARSE_BINDING_BIT,
                (queueFamiliesP[queueFamilyIdx].queueFlags&VK_QUEUE_PROTECTED_BIT        )/VK_QUEUE_PROTECTED_BIT
        );
    }
    free(queueFamiliesP);



    //Add Logical Device
    VkDevice device;
    VkDeviceQueueCreateInfo QueueCreateInfo;
    QueueCreateInfo.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    QueueCreateInfo.queueCount=1;
    exit(0);/*
    QueueCreateInfo.queueFamilyIndex=
    float queuePriority=1.0f;
    QueueCreateInfo.pQueuePriorities=&queuePriority;

    VkDeviceCreateInfo DevCreateInfo;
    DevCreateInfo.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DevCreateInfo.que
    vkCreateDevice(physicalDevicesIdx,,NULL,&device);
    */

    while (!glfwWindowShouldClose(mainWindowP)) {
        glfwPollEvents();
    }

}
