#include <stdio.h>
#include <stdlib.h>
#define GLFW_INCLUDE_VULKAN
#include "vulkan/vulkan.h"
#include "submodules/glfw/glfw3.h"

#include "submodules/xmlReader/debug.h"
#include "submodules/xmlReader/xmlReader.h"
#include "submodules/xmlReader/stringutils.h"

struct vulkanObj{

};

int loadDaeObject(char* filePath,char* meshName,struct vulkanObj* outputVulkanObjectP){
    dprintf(DBGT_INFO,"%d,%d,%d,%d",sizeof(uint32_t),sizeof(void*),sizeof(void**),sizeof(unsigned int));
    struct DynamicList* meshID=stringToUTF32Dynlist(meshName);

    FILE* cylinderDaeFileP=fopen(filePath,"rb");
    struct xmlTreeElement* xmlDaeRootP=0;
    readXML(cylinderDaeFileP,&xmlDaeRootP);
    fclose(cylinderDaeFileP);
    //printXMLsubelements(xmlDaeRootP);

    struct xmlTreeElement* xmlColladaElementP=getNthSubelement(xmlDaeRootP,0);
    struct xmlTreeElement* xmlLibGeoElementP=getFirstSubelementWith_freeArg234(xmlColladaElementP,stringToUTF32Dynlist("library_geometries"),NULL,NULL,NULL,0);
    struct xmlTreeElement* xmlGeoElementP=getFirstSubelementWith_freeArg234(xmlLibGeoElementP,stringToUTF32Dynlist("geometry"),stringToUTF32Dynlist("id"),DlDuplicate(sizeof(uint32_t),meshID),NULL,0); //does  not work?
    struct xmlTreeElement* xmlMeshElementP=getFirstSubelementWith_freeArg234(xmlGeoElementP,stringToUTF32Dynlist("mesh"),NULL,NULL,NULL,0);

    //Get Triangles
    struct xmlTreeElement* xmlTrianglesP=getFirstSubelementWith_freeArg234(xmlMeshElementP,stringToUTF32Dynlist("triangles"),NULL,NULL,NULL,0);
    struct DynamicList* TrianglesCountString=getValueFromKeyName_freeArg2(xmlTrianglesP->attributes,stringToUTF32Dynlist("count"));
    struct DynamicList* TrianglesCount=utf32dynlistToInts64_freeArg1(createCharMatchList(2,' ',' '),TrianglesCountString);
    dprintf(DBGT_INFO,"Model has %d triangles",((int64_t*)TrianglesCount->items)[0]);

    struct xmlTreeElement* xmlTrianglesOrderP=getFirstSubelementWith_freeArg234(xmlTrianglesP,stringToUTF32Dynlist("p"),NULL,NULL,NULL,0);
    struct xmlTreeElement* xmlTrianglesOrderContentP=getNthSubelementOrMisc(xmlTrianglesOrderP,0);
    struct DynamicList* TrianglesOrder=utf32dynlistToInts64_freeArg1(createCharMatchList(2,' ',' '),xmlTrianglesOrderContentP->content);
    dprintf(DBGT_INFO,"Found Triangle order list with %d entries",TrianglesOrder->itemcnt);

    //Get Normals
    struct xmlTreeElement* xmlNormalsSourceP=getFirstSubelementWith_freeArg234(xmlMeshElementP,stringToUTF32Dynlist("source"),
                                                                                                stringToUTF32Dynlist("id"),
                                                                                                DlCombine_freeArg3(sizeof(uint32_t),meshID,stringToUTF32Dynlist("-normals")),
                                                                                                NULL,0);
    struct xmlTreeElement* xmlNormalsFloatP=getFirstSubelementWith_freeArg234(xmlNormalsSourceP,stringToUTF32Dynlist("float_array"),
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
    struct xmlTreeElement* xmlPositionsSourceP=getFirstSubelementWith_freeArg234(xmlMeshElementP,stringToUTF32Dynlist("source"),
                                                                     stringToUTF32Dynlist("id"),DlCombine_freeArg3(sizeof(uint32_t),meshID,stringToUTF32Dynlist("-positions")),NULL,0);

    struct xmlTreeElement* xmlPositionsFloatP=getFirstSubelementWith_freeArg234(xmlPositionsSourceP,stringToUTF32Dynlist("float_array"),
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
    VkPhysicalDevice* available_phys_devices;
    VkDevice device;
    uint32_t graphics_queue_family_idx;
    VkSwapchainKHR swapChain;
    VkCommandPool commandPool;
    VkCommandBuffer primaryCommandBuffer;
};



void eng_vulkan_create_instance(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct xmlTreeElement* eng_setupxmlP){
    //get required information from xml object in memory
    //engine and app name
    struct xmlTreeElement* engNameXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,stringToUTF32Dynlist("EngineName"),NULL,NULL,0,0);
    struct xmlTreeElement* engNameContentXmlElmntP=getFirstSubelementWith(engNameXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0);
    char* engNameCharP=utf32dynlist_to_string(utf32dynlistStripSpaceChars(engNameContentXmlElmntP->content));

    struct xmlTreeElement* appNameXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,stringToUTF32Dynlist("EngineName"),NULL,NULL,0,0);
    struct xmlTreeElement* appNameContentXmlElmntP=getFirstSubelementWith(appNameXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0);
    char* appNameCharP=utf32dynlist_to_string(utf32dynlistStripSpaceChars(appNameContentXmlElmntP->content));

    //engine and app version
    struct xmlTreeElement* engVersionXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,stringToUTF32Dynlist("EngineVersion"),NULL,NULL,0,0);
    struct xmlTreeElement* engVersionContentXmlElmntP=getFirstSubelementWith(engVersionXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0);
    uint32_t engVersion=eng_get_version_number_from_UTF32DynlistP(utf32dynlistStripSpaceChars(engVersionContentXmlElmntP->content));

    struct xmlTreeElement* appVersionXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,stringToUTF32Dynlist("EngineVersion"),NULL,NULL,0,0);
    struct xmlTreeElement* appVersionContentXmlElmntP=getFirstSubelementWith(appVersionXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0);
    uint32_t appVersion=eng_get_version_number_from_UTF32DynlistP(utf32dynlistStripSpaceChars(appVersionContentXmlElmntP->content));

    //Create Application Info structure
    VkApplicationInfo AppInfo;
    AppInfo.sType=              VK_STRUCTURE_TYPE_APPLICATION_INFO;
    AppInfo.pNext=              NULL;
    AppInfo.apiVersion=         VK_API_VERSION_1_1;
    AppInfo.pApplicationName=   appNameCharP;
    AppInfo.applicationVersion= appVersion;
    AppInfo.pEngineName=        engNameCharP;
    AppInfo.engineVersion=      engVersion;

    //retrieve required layers and extensions for interface
    struct xmlTreeElement* reqIfaceLayerXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,stringToUTF32Dynlist("RequiredInterfaceLayers"),NULL,NULL,0,0);
    struct DynamicList* reqIfaceLayerDynlistP=getAllSubelementsWith_freeArg234(reqIfaceLayerXmlElmntP,stringToUTF32Dynlist("Layer"),NULL,NULL,0,0);
    struct xmlTreeElement* reqIfaceExtensionXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,stringToUTF32Dynlist("RequiredInterfaceExtensions"),NULL,NULL,0,0);
    struct DynamicList* reqIfaceExtensionDynlistP=getAllSubelementsWith_freeArg234(reqIfaceExtensionXmlElmntP,stringToUTF32Dynlist("Extension"),NULL,NULL,0,0);


    //Check layer support
    uint32_t layerCount=0;
    vkEnumerateInstanceLayerProperties(layerCount,NULL);
    VkLayerProperties* LayerProptertiesP=(VkLayerProperties*)malloc(layerCount*sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(layerCount,LayerProptertiesP);
    for(unsigned int required_layer_idx=0;required_layer_idx<reqIfaceLayerDynlistP->itemcnt;required_layer_idx++){
        unsigned int available_layer_idx;
        struct xmlTreeElement* currentLayerXmlElmntP=((struct xmlTreeElement**)(reqIfaceLayerDynlistP->items))[required_layer_idx];
        char* reqLayerNameCharP=utf32dynlist_to_string(getValueFromKeyName_freeArg2(currentLayerXmlElmntP->attributes,stringToUTF32Dynlist("name")));
        uint32_t minVersion=eng_get_version_number_from_xmlemnt(currentLayerXmlElmntP);
        for(available_layer_idx=0;available_layer_idx<layerCount;available_layer_idx++){
            if(!strcmp(LayerProptertiesP[available_layer_idx].layerName,reqLayerNameCharP)){
                uint32_t availableVersion=LayerProptertiesP[available_layer_idx].implementationVersion;
                if(availableVersion>=minVersion){
                    break;//requested layer was found with a version that is supported
                }else{
                    dprintf(DBGT_INFO,"Layer %s was found but version %d while %d required!\n",LayerProptertiesP[available_layer_idx].layerName,availableVersion,minVersion);
                }
            }
        }
        free(reqLayerNameCharP);
        if(available_layer_idx==layerCount){ //layer was not found
            free(LayerProptertiesP);
            dprintf(DBGT_ERROR,"Vulkan instance does not support required layer");
            exit(1);
        }
    }
    free(LayerProptertiesP);

    //Check extension support
    uint32_t extensionCount=0;
    vkEnumerateInstanceExtensionProperties(NULL,&extensionCount,NULL);
    VkExtensionProperties* ExtensionProptertiesP=(VkExtensionProperties*)malloc(extensionCount*sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL,&extensionCount,ExtensionProptertiesP);
    for(unsigned int required_extension_idx=0;required_extension_idx<reqIfaceExtensionDynlistP->itemcnt;required_extension_idx++){
        unsigned int available_extension_idx;
        struct xmlTreeElement* currentExtensionXmlElmntP=((struct xmlTreeElement**)(reqIfaceExtensionDynlistP->items))[required_extension_idx];
        char* reqExtensionNameCharP=utf32dynlist_to_string(getValueFromKeyName_freeArg2(currentExtensionXmlElmntP->attributes,stringToUTF32Dynlist("name")));
        uint32_t minVersion=eng_get_version_number_from_xmlemnt(currentExtensionXmlElmntP);
        for(available_extension_idx=0;available_extension_idx<extensionCount;available_extension_idx++){
            if(!strcmp(ExtensionProptertiesP[available_extension_idx].extensionName,reqExtensionNameCharP)){
                uint32_t availableVersion=ExtensionProptertiesP[available_extension_idx].specVersion;
                if(availableVersion>=minVersion){
                    break;//requested extension was found with a version that is supported
                }else{
                    dprintf(DBGT_INFO,"Extension %s was found but version %d while %d required!\n",ExtensionProptertiesP[available_extension_idx].extensionName,availableVersion,minVersion);
                }
            }
        }
        free(reqExtensionNameCharP);
        if(available_extension_idx==extensionCount){ //extension was not found
            free(ExtensionProptertiesP);
            dprintf(DBGT_ERROR,"Vulkan instance does not support required extension");
            exit(1);
        }
    }
    free(ExtensionProptertiesP);

    //TODO check glfwExtensions in xml file
    uint32_t glfwExtensionsCount=0;
    const char** glfwExtensions=glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
    VkInstanceCreateInfo CreateInfo;
    CreateInfo.sType=                   VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    CreateInfo.pApplicationInfo=        &AppInfo;
    CreateInfo.pNext=                   NULL;
    CreateInfo.enabledExtensionCount=   glfwExtensionsCount;
    CreateInfo.ppEnabledExtensionNames= glfwExtensions;
    CreateInfo.enabledLayerCount=       sizeof(required_instance_layers_names)/sizeof(required_instance_layers_names[0]);
    CreateInfo.ppEnabledLayerNames=     required_instance_layers_names;
    //TODO enable validation layers

    VkInstance instance;
    if(vkCreateInstance(&CreateInfo,NULL,&(vkRuntimeInfoP->instance))!=VK_SUCCESS){
        dprintf(DBGT_ERROR,"Could not create vulkan instance");
        exit(1);
    }
}

uint8_t* eng_vulkan_generate_device_ranking(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct xmlTreeElement* eng_setupxmlP){

    struct xmlTreeElement* reqDevLayerXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,stringToUTF32Dynlist("RequiredDeviceLayers"),NULL,NULL,0,1);
    struct DynamicList* reqDevLayerDynlistP=getAllSubelementsWith_freeArg234(reqDevLayerXmlElmntP,stringToUTF32Dynlist("Layer"),NULL,NULL,0,1);
    struct xmlTreeElement* reqDevExtensionXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,stringToUTF32Dynlist("RequiredDeviceExtensions"),NULL,NULL,0,1);
    struct DynamicList* reqDevExtensionDynlistP=getAllSubelementsWith_freeArg234(reqDevExtensionXmlElmntP,stringToUTF32Dynlist("Extension"),NULL,NULL,0,1);

    //get all vulkan devices
    uint32_t physicalDevicesCount=0;
    vkEnumeratePhysicalDevices(vkRuntimeInfoP->instance,&physicalDevicesCount,NULL);
    vkRuntimeInfoP->available_phys_devices=(VkPhysicalDevice*)malloc(physicalDevicesCount*sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(vkRuntimeInfoP->instance,&physicalDevicesCount,vkRuntimeInfoP->available_phys_devices);
    //check if our device supports the required layers,extensions and queues
    uint8_t* deviceRankingP=malloc(physicalDevicesCount*sizeof(uint8_t));
    memset(deviceRankingP,1,physicalDevicesCount);
    uint32_t physicalDevicesIdx;
    for(physicalDevicesIdx=0;physicalDevicesIdx<physicalDevicesCount;physicalDevicesIdx++){
        //Check device properties
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(vkRuntimeInfoP->available_phys_devices[physicalDevicesIdx],&deviceProperties);
        if(deviceProperties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
            deviceRankingP[physicalDevicesCount]=2;   //discrete GPU's are prefered
            continue;
        }

        //Check layer support
        uint32_t layerCount=0;
        vkEnumerateDeviceLayerProperties(vkRuntimeInfoP->available_phys_devices[physicalDevicesIdx],layerCount,NULL);
        VkLayerProperties* LayerProptertiesP=(VkLayerProperties*)malloc(layerCount*sizeof(VkLayerProperties));
        vkEnumerateDeviceLayerProperties(vkRuntimeInfoP->available_phys_devices[physicalDevicesIdx],layerCount,LayerProptertiesP);
        for(unsigned int required_layer_idx=0;required_layer_idx<reqDevLayerDynlistP->itemcnt;required_layer_idx++){
            unsigned int available_layer_idx;
            struct xmlTreeElement* currentLayerXmlElmntP=((struct xmlTreeElement**)(reqDevLayerDynlistP->items))[required_layer_idx];
            char* reqLayerNameCharP=utf32dynlist_to_string(getValueFromKeyName_freeArg2(currentLayerXmlElmntP->attributes,stringToUTF32Dynlist("name")));
            uint32_t minVersion=eng_get_version_number_from_xmlemnt(currentLayerXmlElmntP);
            for(available_layer_idx=0;available_layer_idx<layerCount;available_layer_idx++){
                if(!strcmp(LayerProptertiesP[available_layer_idx].layerName,reqLayerNameCharP)){
                    uint32_t availableVersion=LayerProptertiesP[available_layer_idx].implementationVersion;
                    if(availableVersion>=minVersion){
                        break;//requested layer was found with a version that is supported
                    }else{
                        dprintf(DBGT_INFO,"Layer %s was found but version %d while %d required!\n",LayerProptertiesP[available_layer_idx].layerName,availableVersion,minVersion);
                    }
                }
            }
            free(reqLayerNameCharP);
            if(available_layer_idx==layerCount){ //layer was not found
                free(LayerProptertiesP);
                deviceRankingP[physicalDevicesCount]=0;
                break;
            }
        }
        free(LayerProptertiesP);
        if(!deviceRankingP[physicalDevicesCount]){
            continue;
        }

        //Check extension support
        uint32_t extensionCount=0;
        vkEnumerateDeviceExtensionProperties(vkRuntimeInfoP->available_phys_devices[physicalDevicesIdx],NULL,&extensionCount,NULL);
        VkExtensionProperties* ExtensionProptertiesP=(VkExtensionProperties*)malloc(extensionCount*sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(vkRuntimeInfoP->available_phys_devices[physicalDevicesIdx],NULL,&extensionCount,ExtensionProptertiesP);
        for(unsigned int required_extension_idx=0;required_extension_idx<reqDevExtensionDynlistP->itemcnt;required_extension_idx++){
            unsigned int available_extension_idx;
            struct xmlTreeElement* currentExtensionXmlElmntP=((struct xmlTreeElement**)(reqDevExtensionDynlistP->items))[required_extension_idx];
            char* reqExtensionNameCharP=utf32dynlist_to_string(getValueFromKeyName_freeArg2(currentExtensionXmlElmntP->attributes,stringToUTF32Dynlist("name")));
            uint32_t minVersion=eng_get_version_number_from_xmlemnt(currentExtensionXmlElmntP);
            for(available_extension_idx=0;available_extension_idx<extensionCount;available_extension_idx++){
                if(!strcmp(ExtensionProptertiesP[available_extension_idx].extensionName,reqExtensionNameCharP)){
                    uint32_t availableVersion=ExtensionProptertiesP[available_extension_idx].implementationVersion;
                    if(availableVersion>=minVersion){
                        break;//requested extension was found with a version that is supported
                    }else{
                        dprintf(DBGT_INFO,"Extension %s was found but version %d while %d required!\n",ExtensionProptertiesP[available_extension_idx].extensionName,availableVersion,minVersion);
                    }
                }
            }
            free(reqExtensionNameCharP);
            if(available_extension_idx==extensionCount){ //extension was not found
                free(ExtensionProptertiesP);
                deviceRankingP[physicalDevicesCount]=0;
                break;
            }
        }
        free(ExtensionProptertiesP);
        if(!deviceRankingP[physicalDevicesCount]){
            continue;
        }


        //Check supported Queues
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(vkRuntimeInfoP->available_phys_devices[physicalDevicesIdx],&queueFamilyCount,NULL);
        dprintf(DBGT_INFO,"Found %d queueFamilys",queueFamilyCount);
        VkQueueFamilyProperties* queueFamiliyPropP=(VkQueueFamilyProperties*)malloc(queueFamilyCount*sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(vkRuntimeInfoP->available_phys_devices[physicalDevicesIdx],&queueFamilyCount,queueFamiliyPropP);
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
            if(queueFamiliyPropP[queueFamilyIdx].queueFlags&(VK_QUEUE_GRAPHICS_BIT)){
                free(queueFamiliyPropP);
                break;
            }
        }
        free(queueFamiliyPropP);
        if(queueFamilyIdx==queueFamilyCount){
            deviceRankingP[physicalDevicesCount]=0;
            dprintf(DBGT_ERROR,"This GPU does not support a Graphics Queue.");
            break;
        }
    }
    return deviceRankingP;
}

void eng_vulkan_create_device(struct VulkanRuntimeInfo* vkRuntimeInfoP){

    //Create logical device
    float queuePriority=1.0f;

    VkDeviceQueueCreateInfo QueueCreateInfo;
    QueueCreateInfo.sType=              VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    QueueCreateInfo.queueCount=         1;
    QueueCreateInfo.queueFamilyIndex=   queueFamilyIdx;
    QueueCreateInfo.pQueuePriorities=   &queuePriority;
    QueueCreateInfo.pNext=              NULL;

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

struct xmlTreeElement* eng_get_eng_setupxml(char* FilePath,int debug_enabled){
    FILE* engSetupFileP=fopen(FilePath,"rb");
    struct xmlTreeElement* engSetupRootP=0;
    readXML(engSetupFileP,&engSetupRootP);
    fclose(engSetupFileP);
    //select release or debug
    struct xmlTreeElement* engSetupDebOrRelP;
    struct DynamicList* tempXmlDynlistP;
    if(debug_enabled){
        tempXmlDynlistP=getSubelementsWith_freeArg2345(engSetupRootP,stringToUTF32Dynlist("Debug"),NULL,NULL,NULL,1);
    }else{
        tempXmlDynlistP=getSubelementsWith_freeArg2345(engSetupRootP,stringToUTF32Dynlist("Release"),NULL,NULL,NULL,1);
    }
    if(tempXmlDynlistP->itemcnt!=1){
        dprintf(DBGT_ERROR,"Invalid EngSetupFile format");
        exit(1);
    }
    engSetupDebOrRelP=((struct xmlTreeElement**)tempXmlDynlistP->items)[0];
    return engSetupDebOrRelP;
};

uint32_t eng_get_version_number_from_UTF32DynlistP(struct DynamicList* inputStringP){
    struct DynamicList* versionNumP=utf32dynlistToInts64_freeArg1(createCharMatchList(2,'.','.'),inputStringP);
    if(versionNumP->itemcnt!=3){
        dprintf(DBGT_ERROR,"Invalid Version format");
        exit(1);
    }
    uint64_t* versionIntsP=(uint64_t*)(versionNumP->items);
    uint32_t version=VK_MAKE_VERSION(versionIntsP[0],versionIntsP[1],versionIntsP[2]);
    free(versionIntsP);
    return version;
}

uint32_t eng_get_version_number_from_xmlemnt(struct xmlTreeElement* currentReqXmlP){
    struct DynamicList* currentReqLayerAttribP=(currentReqXmlP->attributes);
    struct DynamicList* minversionUTF32DynlistP=getValueFromKeyName_freeArg2(currentReqLayerAttribP,stringToUTF32Dynlist("minversion"));
    if(!minversionUTF32DynlistP){
        return 0;
    }
    return eng_get_version_number_from_UTF32DynlistP(minversionUTF32DynlistP);
}

void cleanup(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    //vkDestroySwapchainKHR(vkRuntimeInfoP->device, vkRuntimeInfoP->swapChain, NULL);
    /*vkDestroyDevice();
    vkDestroySurfaceKHR();
    vkDestroyInstance();*/
    glfwDestroyWindow(vkRuntimeInfoP->mainWindowP);
    glfwTerminate();
}

#define DEBUG

int main(int argc, char** argv){
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    #ifdef DEBUG
        struct xmlTreeElement* eng_setupxmlP=eng_get_eng_setupxml("./res/vk_setup.xml",1);
    #else
        struct xmlTreeElement* eng_setupxmlP=eng_get_eng_setupxml("./res/vk_setup.xml",0);
    #endif
    struct xmlTreeElement* applicationNameXmlElmntP=getFirstSubelementWith_freeArg2345(eng_setupxmlP,stringToUTF32Dynlist("eng_setupxml"),NULL,NULL,NULL,1);
    char* applicationNameCharP=utf32dynlist_to_string(getFirstSubelementWith(applicationNameXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0)->content);
    GLFWwindow* mainWindowP = glfwCreateWindow(1920, 1080, applicationNameCharP, NULL, NULL);
    free(applicationNameCharP);

    struct VulkanRuntimeInfo engVkRuntimeInfo;
    eng_vulkan_create_instance(&engVkRuntimeInfo,eng_setupxmlP);
    eng_vulkan_generate_device_ranking(&engVkRuntimeInfo,eng_setupxmlP);
    //eng_vulkan_add_device(&engVkRuntimeInfo);
    dprintf(DBGT_INFO,"Got inside main loop");
    while (!glfwWindowShouldClose(mainWindowP)) {
        glfwPollEvents();
    }
}
