#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define GLFW_INCLUDE_VULKAN
#include "vulkan/vulkan.h"
#include "shaderc/shaderc.h"

#include "submodules/glfw/glfw3.h"

#include "submodules/xmlReader/debug.h"
#include "submodules/xmlReader/xmlReader.h"
#include "submodules/xmlReader/stringutils.h"

uint32_t eng_get_version_number_from_xmlemnt(struct xmlTreeElement* currentReqXmlP);
uint32_t eng_get_version_number_from_UTF32DynlistP(struct DynamicList* inputStringP);
struct xmlTreeElement* eng_get_eng_setupxml(char* FilePath,int debug_enabled);
struct vulkanObj{

};

struct VulkanRuntimeInfo{
    GLFWwindow* mainWindowP;

    VkInstance instance;
    uint32_t InstExtensionCount;
    char** InstExtensionNamesPP;
    uint32_t InstLayerCount;
    char** InstLayerNamesPP;

    VkPhysicalDevice* physAvailDevicesP;
    uint32_t physDeviceCount;
    VkPhysicalDevice physSelectedDevice;

    VkDevice device;
    uint32_t DevExtensionCount;
    char** DevExtensionNamesPP;
    uint32_t DevLayerCount;
    char** DevLayerNamesPP;

    uint32_t graphics_queue_family_idx;
    VkQueue  graphics_queue;

    VkSurfaceKHR surface;
    VkSwapchainKHR swapChain;
    uint32_t swapChainImageCount;
    VkImage* swapChainImagesP;
    VkExtent2D swapChainImageExtent;
    VkSurfaceFormatKHR swapChainFormat;
    VkImageView* swapChainImageViewsP;

    VkRenderPass renderPass;

    VkShaderModule VertexShaderModule;
    VkShaderModule FragmentShaderModule;

    VkPipeline graphicsPipeline;

    uint32_t FramebufferCount;
    VkFramebuffer* FramebufferP;

    uint32_t CommandbufferCount;   //TODO merge with FramebufferCount and swapChainImageCount
    VkCommandBuffer* CommandbufferP;

    VkCommandPool commandPool;
    VkCommandBuffer primaryCommandBuffer;

    VkSemaphore* imageAvailableSemaphoreP;
    VkSemaphore* renderFinishedSemaphoreP;
    VkFence*     ImageAlreadyProcessingFenceP;
};



uint32_t max_uint32_t(uint32_t a, uint32_t b){
    if(a>b){
        return a;
    }else{
        return b;
    }
}

uint32_t min_uint32_t(uint32_t a, uint32_t b){
    if(a<b){
        return a;
    }else{
        return b;
    }
}

void eng_createShaderModule(struct VulkanRuntimeInfo* vkRuntimeInfoP,char* ShaderFileLocation){
    FILE* ShaderXmlFileP=fopen(ShaderFileLocation,"rb");
    if(ShaderXmlFileP==NULL){
        dprintf(DBGT_ERROR,"Could not open file %s for compilation",ShaderFileLocation);
        exit(1);
    }
    struct xmlTreeElement* xmlRootElementP;
    readXML(ShaderXmlFileP,&xmlRootElementP);

    struct xmlTreeElement* VertexShaderXmlElmntP=getFirstSubelementWith(xmlRootElementP,Dl_utf32_fromString("vertex"),NULL,NULL,0,1);
    struct xmlTreeElement* VertexShaderContentXmlElmntP=getFirstSubelementWith(VertexShaderXmlElmntP,NULL,NULL,NULL,xmltype_chardata,1);
    char* VertexShaderAsciiSourceP=(char*)malloc((VertexShaderContentXmlElmntP->content->itemcnt+1)*sizeof(char));
    uint32_t VertexSourceLength=utf32CutASCII(VertexShaderContentXmlElmntP->content->items,VertexShaderContentXmlElmntP->content->itemcnt,VertexShaderAsciiSourceP);


    struct xmlTreeElement* FragmentShaderXmlElmntP=getFirstSubelementWith(xmlRootElementP,Dl_utf32_fromString("fragment"),NULL,NULL,0,1);
    struct xmlTreeElement* FragmentShaderContentXmlElmntP=getFirstSubelementWith(FragmentShaderXmlElmntP,NULL,NULL,NULL,xmltype_chardata,1);
    char* FragmentShaderAsciiSourceP=(char*)malloc((FragmentShaderContentXmlElmntP->content->itemcnt+1)*sizeof(char));
    uint32_t FragmentSourceLength=utf32CutASCII(FragmentShaderContentXmlElmntP->content->items,FragmentShaderContentXmlElmntP->content->itemcnt,FragmentShaderAsciiSourceP);

    shaderc_compiler_t shaderCompilerObj=shaderc_compiler_initialize();
    shaderc_compilation_result_t compResultVertex=shaderc_compile_into_spv(shaderCompilerObj,VertexShaderAsciiSourceP,VertexSourceLength,shaderc_glsl_vertex_shader,ShaderFileLocation,"main",NULL);
    shaderc_compilation_result_t compResultFragment=shaderc_compile_into_spv(shaderCompilerObj,FragmentShaderAsciiSourceP,FragmentSourceLength,shaderc_glsl_fragment_shader,ShaderFileLocation,"main",NULL);
    free(VertexShaderAsciiSourceP);
    free(FragmentShaderAsciiSourceP);
    //Print result for vertex
    if(shaderc_result_get_compilation_status(compResultVertex)){
        dprintf(DBGT_ERROR,"Vertex shader compilation failed");
        if(shaderc_result_get_num_errors(compResultVertex)){
            dprintf(DBGT_ERROR,"Error was:\n%s",shaderc_result_get_error_message(compResultVertex));
            exit(1);
        }
    }
    dprintf(DBGT_INFO,"While compiling vertex_%s there were %lld warnings.",ShaderFileLocation,shaderc_result_get_num_warnings(compResultVertex));
    //Print result for fragment
    if(shaderc_result_get_compilation_status(compResultFragment)){
        dprintf(DBGT_ERROR,"Fragment shader compilation failed");
        if(shaderc_result_get_num_errors(compResultFragment)){
            dprintf(DBGT_ERROR,"Error was:\n%s",shaderc_result_get_error_message(compResultFragment));
            exit(1);
        }
    }
    dprintf(DBGT_INFO,"While compiling fragment_%s there were %lld warnings.",ShaderFileLocation,shaderc_result_get_num_warnings(compResultFragment));

    shaderc_result_get_bytes(compResultFragment);
    //cleanup

    shaderc_compiler_release(shaderCompilerObj);

    VkShaderModuleCreateInfo ShaderModuleCreateInfo;
    ShaderModuleCreateInfo.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.pNext=NULL;
    ShaderModuleCreateInfo.flags=0;
    ShaderModuleCreateInfo.pCode=(uint32_t*)shaderc_result_get_bytes(compResultVertex);
    ShaderModuleCreateInfo.codeSize=shaderc_result_get_length(compResultVertex);
    vkCreateShaderModule(vkRuntimeInfoP->device,&ShaderModuleCreateInfo,NULL,&(vkRuntimeInfoP->VertexShaderModule));
    //Recycle ShaderModuleCreateInfo

    ShaderModuleCreateInfo.pCode=(uint32_t*)shaderc_result_get_bytes(compResultFragment);
    ShaderModuleCreateInfo.codeSize=shaderc_result_get_length(compResultFragment);
    vkCreateShaderModule(vkRuntimeInfoP->device,&ShaderModuleCreateInfo,NULL,&(vkRuntimeInfoP->FragmentShaderModule));

    //shaderc_result_release(compResultVertex);
    //shaderc_result_release(compResultFragment);
}

uint32_t clamp_uint32_t(uint32_t lower_bound,uint32_t clampedValueInput,uint32_t upper_bound){
    return max_uint32_t(min_uint32_t(upper_bound,clampedValueInput),lower_bound);
}

void loadDaeObject(char* filePath,char* meshName,struct vulkanObj* outputVulkanObjectP){
    dprintf(DBGT_INFO,"%llu,%llu,%llu,%llu",sizeof(uint32_t),sizeof(void*),sizeof(void**),sizeof(unsigned int));
    struct DynamicList* meshID=Dl_utf32_fromString(meshName);

    FILE* cylinderDaeFileP=fopen(filePath,"rb");
    struct xmlTreeElement* xmlDaeRootP=0;
    readXML(cylinderDaeFileP,&xmlDaeRootP);
    fclose(cylinderDaeFileP);
    //printXMLsubelements(xmlDaeRootP);

    struct xmlTreeElement* xmlColladaElementP=getNthSubelement(xmlDaeRootP,0);
    struct xmlTreeElement* xmlLibGeoElementP=getFirstSubelementWith_freeArg234(xmlColladaElementP,Dl_utf32_fromString("library_geometries"),NULL,NULL,0,0);
    struct xmlTreeElement* xmlGeoElementP=getFirstSubelementWith_freeArg234(xmlLibGeoElementP,Dl_utf32_fromString("geometry"),Dl_utf32_fromString("id"),DlDuplicate(sizeof(uint32_t),meshID),0,0); //does  not work?
    struct xmlTreeElement* xmlMeshElementP=getFirstSubelementWith_freeArg234(xmlGeoElementP,Dl_utf32_fromString("mesh"),NULL,NULL,0,0);

    //Get Triangles
    struct xmlTreeElement* xmlTrianglesP=getFirstSubelementWith_freeArg234(xmlMeshElementP,Dl_utf32_fromString("triangles"),NULL,NULL,0,0);
    struct DynamicList* TrianglesCountString=getValueFromKeyName_freeArg2(xmlTrianglesP->attributes,Dl_utf32_fromString("count"));
    struct DynamicList* TrianglesCount=Dl_utf32_to_Dl_int64_freeArg1(Dl_CMatch_create(2,' ',' '),TrianglesCountString);
    dprintf(DBGT_INFO,"Model has %lld triangles",((int64_t*)TrianglesCount->items)[0]);

    struct xmlTreeElement* xmlTrianglesOrderP=getFirstSubelementWith_freeArg234(xmlTrianglesP,Dl_utf32_fromString("p"),NULL,NULL,0,0);
    struct xmlTreeElement* xmlTrianglesOrderContentP=getNthSubelement(xmlTrianglesOrderP,0);
    struct DynamicList* TrianglesOrder=Dl_utf32_to_Dl_int64_freeArg1(Dl_CMatch_create(2,' ',' '),xmlTrianglesOrderContentP->content);
    dprintf(DBGT_INFO,"Found Triangle order list with %d entries",TrianglesOrder->itemcnt);

    //Get Normals
    struct xmlTreeElement* xmlNormalsSourceP=getFirstSubelementWith_freeArg234(xmlMeshElementP,Dl_utf32_fromString("source"),
                                                                                                Dl_utf32_fromString("id"),
                                                                                                DlCombine_freeArg3(sizeof(uint32_t),meshID,Dl_utf32_fromString("-normals")),
                                                                                                0,0);
    struct xmlTreeElement* xmlNormalsFloatP=getFirstSubelementWith_freeArg234(xmlNormalsSourceP,Dl_utf32_fromString("float_array"),
                                                                                                Dl_utf32_fromString("id"),
                                                                                                DlCombine_freeArg3(sizeof(uint32_t),meshID,Dl_utf32_fromString("-normals-array")),
                                                                                                0,0);
    struct DynamicList* NormalsCountString=getValueFromKeyName_freeArg2(xmlNormalsFloatP->attributes,Dl_utf32_fromString("count"));
    Dl_utf32_print(NormalsCountString);
    struct DynamicList* NormalsCount=Dl_utf32_to_Dl_int64_freeArg1(Dl_CMatch_create(2,' ',' '),NormalsCountString);
    dprintf(DBGT_INFO,"Model has count %lld normal coordinates",((int64_t*)NormalsCount->items)[0]);
    struct xmlTreeElement* xmlNormalsFloatContentP=getNthSubelement(xmlNormalsFloatP,0);
    struct DynamicList* NormalsDlP=Dl_utf32_to_Dl_float_freeArg123(Dl_CMatch_create(4,' ',' ','\t','\t'),Dl_CMatch_create(4,'e','e','E','E'),Dl_CMatch_create(2,'.','.'),xmlNormalsFloatContentP->content);
    dprintf(DBGT_INFO,"Model has %d normal coordinates",NormalsDlP->itemcnt);

    //Get Positions
    struct xmlTreeElement* xmlPositionsSourceP=getFirstSubelementWith_freeArg234(xmlMeshElementP,Dl_utf32_fromString("source"),
                                                                     Dl_utf32_fromString("id"),DlCombine_freeArg3(sizeof(uint32_t),meshID,Dl_utf32_fromString("-positions")),0,0);

    struct xmlTreeElement* xmlPositionsFloatP=getFirstSubelementWith_freeArg234(xmlPositionsSourceP,Dl_utf32_fromString("float_array"),
                                                                    Dl_utf32_fromString("id"),DlCombine_freeArg3(sizeof(uint32_t),meshID,Dl_utf32_fromString("-positions-array")),0,0);

    struct DynamicList* PositionsCountString=getValueFromKeyName_freeArg2(xmlPositionsFloatP->attributes,Dl_utf32_fromString("count"));
    struct DynamicList* PositionsCount=Dl_utf32_to_Dl_int64_freeArg1(Dl_CMatch_create(2,' ',' '),PositionsCountString);
    //if(xmlPositionsFloatP->content->type!=dynlisttype_utf32chars){dprintf(DBGT_ERROR,"Invalid content type");return 0;}
    struct xmlTreeElement* xmlPositionsFloatContentP=getNthSubelement(xmlPositionsFloatP,0);
    struct DynamicList* PositionsDlP=Dl_utf32_to_Dl_float_freeArg123(Dl_CMatch_create(4,' ',' ','\t','\t'),Dl_CMatch_create(4,'e','e','E','E'),Dl_CMatch_create(2,'.','.'),xmlPositionsFloatContentP->content);
}

uint32_t findBestMemoryType(struct VulkanRuntimeInfo* vkRuntimeInfoP,uint32_t requiredBitfield,VkMemoryPropertyFlags properties){
    //Get information about all available memory types
    VkPhysicalDeviceMemoryProperties DeviceMemProperties;
    vkGetPhysicalDeviceMemoryProperties(vkRuntimeInfoP->device,&DeviceMemProperties);
    for(uint32_t MemoryTypeIdx=0;MemoryTypeIdx<DeviceMemProperties.memoryTypeCount;MemoryTypeIdx++){
        if(DeviceMemProperties.memoryTypes)
    }
}



void eng_createInstance(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct xmlTreeElement* eng_setupxmlP){
    //get required information from xml object in memory
    //engine and app name
    struct xmlTreeElement* engNameXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,Dl_utf32_fromString("EngineName"),NULL,NULL,0,0);
    struct xmlTreeElement* engNameContentXmlElmntP=getFirstSubelementWith(engNameXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0);
    engNameContentXmlElmntP->content=Dl_utf32_StripSpaces_freeArg1(engNameContentXmlElmntP->content);
    char* engNameCharP=Dl_utf32_toString(engNameContentXmlElmntP->content);

    struct xmlTreeElement* appNameXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,Dl_utf32_fromString("ApplicationName"),NULL,NULL,0,0);
    struct xmlTreeElement* appNameContentXmlElmntP=getFirstSubelementWith(appNameXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0);
    appNameContentXmlElmntP->content=Dl_utf32_StripSpaces_freeArg1(appNameContentXmlElmntP->content);
    char* appNameCharP=Dl_utf32_toString(appNameContentXmlElmntP->content);

    //engine and app version
    struct xmlTreeElement* engVersionXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,Dl_utf32_fromString("EngineVersion"),NULL,NULL,0,0);
    struct xmlTreeElement* engVersionContentXmlElmntP=getFirstSubelementWith(engVersionXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0);
    engVersionContentXmlElmntP->content=Dl_utf32_StripSpaces_freeArg1(engVersionContentXmlElmntP->content);
    uint32_t engVersion=eng_get_version_number_from_UTF32DynlistP(engVersionContentXmlElmntP->content);

    struct xmlTreeElement* appVersionXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,Dl_utf32_fromString("ApplicationVersion"),NULL,NULL,0,0);
    struct xmlTreeElement* appVersionContentXmlElmntP=getFirstSubelementWith(appVersionXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0);
    appVersionContentXmlElmntP->content=Dl_utf32_StripSpaces_freeArg1(appVersionContentXmlElmntP->content);
    uint32_t appVersion=eng_get_version_number_from_UTF32DynlistP(appVersionContentXmlElmntP->content);

    //Create Application Info structure
    VkApplicationInfo AppInfo;
    AppInfo.sType=              VK_STRUCTURE_TYPE_APPLICATION_INFO;
    AppInfo.pNext=              NULL;
    AppInfo.apiVersion=         VK_API_VERSION_1_1;
    AppInfo.pApplicationName=   appNameCharP;
    AppInfo.applicationVersion= appVersion;
    AppInfo.pEngineName=        engNameCharP;
    AppInfo.engineVersion=      engVersion;

    //retrieve required layers and extensions for instance
    struct xmlTreeElement* reqInstLayerXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,Dl_utf32_fromString("RequiredInstanceLayers"),NULL,NULL,0,0);
    struct DynamicList* reqInstLayerDynlistP=getAllSubelementsWith_freeArg234(reqInstLayerXmlElmntP,Dl_utf32_fromString("Layer"),NULL,NULL,0,0);
    struct xmlTreeElement* reqInstExtensionXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,Dl_utf32_fromString("RequiredInstanceExtensions"),NULL,NULL,0,0);
    struct DynamicList* reqInstExtensionDynlistP=getAllSubelementsWith_freeArg234(reqInstExtensionXmlElmntP,Dl_utf32_fromString("Extension"),NULL,NULL,0,0);

    //Check layer support
    uint32_t layerCount=0;
    vkEnumerateInstanceLayerProperties(&layerCount,NULL);
    VkLayerProperties* LayerProptertiesP=(VkLayerProperties*)malloc(layerCount*sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layerCount,LayerProptertiesP);


    for(unsigned int required_layer_idx=0;required_layer_idx<reqInstLayerDynlistP->itemcnt;required_layer_idx++){
        unsigned int available_layer_idx;
        struct xmlTreeElement* currentLayerXmlElmntP=((struct xmlTreeElement**)(reqInstLayerDynlistP->items))[required_layer_idx];
        struct DynamicList* reqLayerNameDynlistP=getValueFromKeyName_freeArg2(currentLayerXmlElmntP->attributes,Dl_utf32_fromString("name"));
        Dl_utf32_print(reqLayerNameDynlistP);
        char* reqLayerNameCharP=Dl_utf32_toString(reqLayerNameDynlistP);
        printf("%s",reqLayerNameCharP);
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
            dprintf(DBGT_ERROR,"Vulkan instance does not support required layer: %s",reqLayerNameCharP);
            exit(1);
        }
    }
    free(LayerProptertiesP);

    //Check extension support
    uint32_t extensionCount=0;
    vkEnumerateInstanceExtensionProperties(NULL,&extensionCount,NULL);
    VkExtensionProperties* ExtensionProptertiesP=(VkExtensionProperties*)malloc(extensionCount*sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL,&extensionCount,ExtensionProptertiesP);
    for(unsigned int required_extension_idx=0;required_extension_idx<reqInstExtensionDynlistP->itemcnt;required_extension_idx++){
        unsigned int available_extension_idx;
        struct xmlTreeElement* currentExtensionXmlElmntP=((struct xmlTreeElement**)(reqInstExtensionDynlistP->items))[required_extension_idx];
        char* reqExtensionNameCharP=Dl_utf32_toString(getValueFromKeyName_freeArg2(currentExtensionXmlElmntP->attributes,Dl_utf32_fromString("name")));
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

    //generate InstExtensionNames and Count
    vkRuntimeInfoP->InstExtensionCount=reqInstExtensionDynlistP->itemcnt;
    dprintf(DBGT_INFO,"Number of required instance extensions %d",vkRuntimeInfoP->InstExtensionCount);
    vkRuntimeInfoP->InstExtensionNamesPP=(char**)malloc(vkRuntimeInfoP->InstExtensionCount*sizeof(char*));
    for(uint32_t InstExtensionIdx=0;InstExtensionIdx<vkRuntimeInfoP->InstExtensionCount;InstExtensionIdx++){
        struct xmlTreeElement* currentExtensionXmlElmntP=((struct xmlTreeElement**)(reqInstExtensionDynlistP->items))[InstExtensionIdx];
        vkRuntimeInfoP->InstExtensionNamesPP[InstExtensionIdx]=Dl_utf32_toString(getValueFromKeyName_freeArg2(currentExtensionXmlElmntP->attributes,Dl_utf32_fromString("name")));
        dprintf(DBGT_INFO,"Requesting inst extension %s",vkRuntimeInfoP->InstExtensionNamesPP[InstExtensionIdx]);
    }

    //generate InstLayerNames and Count
    vkRuntimeInfoP->InstLayerCount=reqInstLayerDynlistP->itemcnt;
    vkRuntimeInfoP->InstLayerNamesPP=(char**)malloc(vkRuntimeInfoP->InstLayerCount*sizeof(char*));
    for(uint32_t InstLayerIdx=0;InstLayerIdx<vkRuntimeInfoP->InstLayerCount;InstLayerIdx++){
        struct xmlTreeElement* currentExtensionXmlElmntP=((struct xmlTreeElement**)(reqInstLayerDynlistP->items))[InstLayerIdx];
        vkRuntimeInfoP->InstLayerNamesPP[InstLayerIdx]=Dl_utf32_toString(getValueFromKeyName_freeArg2(currentExtensionXmlElmntP->attributes,Dl_utf32_fromString("name")));
        dprintf(DBGT_INFO,"Requesting inst layer %s",vkRuntimeInfoP->InstLayerNamesPP[InstLayerIdx]);
    }

    uint32_t count;
    const char** extensionsInstancePP=glfwGetRequiredInstanceExtensions(&count);

    //Create Vulkan instance
    VkInstanceCreateInfo CreateInfo;
    CreateInfo.sType=                   VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    CreateInfo.pApplicationInfo=        &AppInfo;
    CreateInfo.pNext=                   NULL;
    CreateInfo.flags=                   0;
    CreateInfo.enabledExtensionCount=   count;//vkRuntimeInfoP->InstExtensionCount;
    CreateInfo.ppEnabledExtensionNames= extensionsInstancePP;//vkRuntimeInfoP->InstExtensionNames;
    CreateInfo.enabledLayerCount=       vkRuntimeInfoP->InstLayerCount;
    CreateInfo.ppEnabledLayerNames=     vkRuntimeInfoP->InstLayerNamesPP;

    VkInstance instance;
    if(vkCreateInstance(&CreateInfo,NULL,&(vkRuntimeInfoP->instance))!=VK_SUCCESS){
        dprintf(DBGT_ERROR,"Could not create vulkan instance");
        exit(1);
    }
}

uint8_t* eng_vulkan_generate_device_ranking(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct xmlTreeElement* eng_setupxmlP){

    struct xmlTreeElement* reqDevLayerXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,Dl_utf32_fromString("RequiredDeviceLayers"),NULL,NULL,0,1);
    struct DynamicList* reqDevLayerDynlistP=getAllSubelementsWith_freeArg234(reqDevLayerXmlElmntP,Dl_utf32_fromString("Layer"),NULL,NULL,0,1);
    struct xmlTreeElement* reqDevExtensionXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,Dl_utf32_fromString("RequiredDeviceExtensions"),NULL,NULL,0,1);
    printXMLsubelements(reqDevExtensionXmlElmntP);
    struct DynamicList* reqDevExtensionDynlistP=getAllSubelementsWith_freeArg234(reqDevExtensionXmlElmntP,Dl_utf32_fromString("Extension"),NULL,NULL,0,1);

    //get all vulkan devices

    vkEnumeratePhysicalDevices(vkRuntimeInfoP->instance,&vkRuntimeInfoP->physDeviceCount,NULL);
    vkRuntimeInfoP->physAvailDevicesP=(VkPhysicalDevice*)malloc(vkRuntimeInfoP->physDeviceCount*sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(vkRuntimeInfoP->instance,&vkRuntimeInfoP->physDeviceCount,vkRuntimeInfoP->physAvailDevicesP);

    //check if our device supports the required layers,extensions and queues
    uint8_t* deviceRankingP=malloc(vkRuntimeInfoP->physDeviceCount*sizeof(uint8_t));
    memset(deviceRankingP,1,sizeof(uint8_t)*vkRuntimeInfoP->physDeviceCount);
    dprintf(DBGT_INFO,"Number of available devices %d",vkRuntimeInfoP->physDeviceCount);
    for(uint32_t physicalDevicesIdx=0;physicalDevicesIdx<vkRuntimeInfoP->physDeviceCount;physicalDevicesIdx++){
        //Check device properties
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],&deviceProperties);
        if(deviceProperties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
            deviceRankingP[physicalDevicesIdx]+=1;   //discrete GPU's are prefered
        }

        //Check layer support
        uint32_t layerCount=0;
        vkEnumerateDeviceLayerProperties(vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],&layerCount,NULL);
        VkLayerProperties* LayerProptertiesP=(VkLayerProperties*)malloc(layerCount*sizeof(VkLayerProperties));
        vkEnumerateDeviceLayerProperties(vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],&layerCount,LayerProptertiesP);

        for(unsigned int required_layer_idx=0;required_layer_idx<reqDevLayerDynlistP->itemcnt;required_layer_idx++){
            unsigned int available_layer_idx;
            struct xmlTreeElement* currentLayerXmlElmntP=((struct xmlTreeElement**)(reqDevLayerDynlistP->items))[required_layer_idx];
            char* reqLayerNameCharP=Dl_utf32_toString(getValueFromKeyName_freeArg2(currentLayerXmlElmntP->attributes,Dl_utf32_fromString("name")));
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
                deviceRankingP[physicalDevicesIdx]=0;
                break;
            }
        }
        free(LayerProptertiesP);
        if(!deviceRankingP[physicalDevicesIdx]){
            dprintf(DBGT_INFO,"Device missing support for at least one layer");
            continue;
        }

        //Check extension support
        uint32_t extensionCount=0;
        vkEnumerateDeviceExtensionProperties(vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],NULL,&extensionCount,NULL);
        VkExtensionProperties* ExtensionProptertiesP=(VkExtensionProperties*)malloc(extensionCount*sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],NULL,&extensionCount,ExtensionProptertiesP);

        for(unsigned int required_extension_idx=0;required_extension_idx<reqDevExtensionDynlistP->itemcnt;required_extension_idx++){
            unsigned int available_extension_idx;
            struct xmlTreeElement* currentExtensionXmlElmntP=((struct xmlTreeElement**)(reqDevExtensionDynlistP->items))[required_extension_idx];
            char* reqExtensionNameCharP=Dl_utf32_toString(getValueFromKeyName_freeArg2(currentExtensionXmlElmntP->attributes,Dl_utf32_fromString("name")));
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

            if(available_extension_idx==extensionCount){ //extension was not found
                deviceRankingP[physicalDevicesIdx]=0;
                dprintf(DBGT_INFO,"Device missing support for extension %s",reqExtensionNameCharP);
                free(reqExtensionNameCharP);
                break;
            }
            free(reqExtensionNameCharP);
        }
        free(ExtensionProptertiesP);
        if(!deviceRankingP[physicalDevicesIdx]){
            dprintf(DBGT_INFO,"Device missing support for at least one extension");
            continue;
        }

        //Check supported Queues
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],&queueFamilyCount,NULL);
        dprintf(DBGT_INFO,"Found %d queueFamilys",queueFamilyCount);
        VkQueueFamilyProperties* queueFamiliyPropP=(VkQueueFamilyProperties*)malloc(queueFamilyCount*sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],&queueFamilyCount,queueFamiliyPropP);
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
            if(queueFamiliyPropP[queueFamilyIdx].queueFlags&(VK_QUEUE_GRAPHICS_BIT)&&glfwGetPhysicalDevicePresentationSupport(vkRuntimeInfoP->instance,vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],queueFamilyIdx)){
                break;
            }
        }
        free(queueFamiliyPropP);
        if(queueFamilyIdx==queueFamilyCount){
            deviceRankingP[physicalDevicesIdx]=0;
            dprintf(DBGT_ERROR,"This GPU does not support a Graphics Queue or is missing presentation support.");
            break;
        }
    }

    //generate DevExtensionNames and Count
    vkRuntimeInfoP->DevExtensionCount=reqDevExtensionDynlistP->itemcnt;
    dprintf(DBGT_INFO,"count: %d",vkRuntimeInfoP->DevExtensionCount);
    vkRuntimeInfoP->DevExtensionNamesPP=(char**)malloc(vkRuntimeInfoP->DevExtensionCount*sizeof(char*));
    for(uint32_t DevExtensionIdx=0;DevExtensionIdx<vkRuntimeInfoP->DevExtensionCount;DevExtensionIdx++){
        struct xmlTreeElement* currentExtensionXmlElmntP=((struct xmlTreeElement**)(reqDevExtensionDynlistP->items))[DevExtensionIdx];
        printXMLsubelements(currentExtensionXmlElmntP);
        struct DynamicList* extensionNameTempP=getValueFromKeyName_freeArg2(currentExtensionXmlElmntP->attributes,Dl_utf32_fromString("name"));
        vkRuntimeInfoP->DevExtensionNamesPP[DevExtensionIdx]=Dl_utf32_toString(extensionNameTempP);
    }

    //generate DevLayerNames and Count
    vkRuntimeInfoP->DevLayerCount=reqDevLayerDynlistP->itemcnt;
    vkRuntimeInfoP->DevLayerNamesPP=(char**)malloc(vkRuntimeInfoP->DevLayerCount*sizeof(char*));
    for(uint32_t DevLayerIdx=0;DevLayerIdx<vkRuntimeInfoP->DevLayerCount;DevLayerIdx++){
        struct xmlTreeElement* currentExtensionXmlElmntP=((struct xmlTreeElement**)(reqDevLayerDynlistP->items))[DevLayerIdx];
        vkRuntimeInfoP->DevLayerNamesPP[DevLayerIdx]=Dl_utf32_toString(getValueFromKeyName_freeArg2(currentExtensionXmlElmntP->attributes,Dl_utf32_fromString("name")));
    }

    return deviceRankingP;
}

void eng_createDevice(struct VulkanRuntimeInfo* vkRuntimeInfoP,uint8_t* deviceRanking){
    //find highest ranking device in list
    uint8_t bestRank=0;
    for(uint32_t deviceNum=0;deviceNum<vkRuntimeInfoP->physDeviceCount;deviceNum++){
        if(deviceRanking[deviceNum]>bestRank){
            bestRank=deviceRanking[deviceNum];
        }
    }
    if(bestRank==0){
        dprintf(DBGT_ERROR,"None of your devices supports the required extensions, layers and features required for running this app");
        exit(1);
    }
    uint32_t deviceNum=0;
    for(;deviceNum<vkRuntimeInfoP->physDeviceCount;deviceNum++){
        if(deviceRanking[deviceNum]==bestRank){
            break;
        }
    }
    vkRuntimeInfoP->physSelectedDevice=vkRuntimeInfoP->physAvailDevicesP[deviceNum];

    //select first available graphics queue on device
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vkRuntimeInfoP->physSelectedDevice,&queueFamilyCount,NULL);
    VkQueueFamilyProperties* queueFamiliyPropP=(VkQueueFamilyProperties*)malloc(queueFamilyCount*sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(vkRuntimeInfoP->physSelectedDevice,&queueFamilyCount,queueFamiliyPropP);
    uint32_t queueFamilyIdx;

    for(queueFamilyIdx=0;queueFamilyIdx<queueFamilyCount;queueFamilyIdx++){
        if(queueFamiliyPropP[queueFamilyIdx].queueFlags&(VK_QUEUE_GRAPHICS_BIT)){
            break;
        }
        //vkGetPhysicalDeviceSurfaceSupportKHR()
    }
    if(queueFamilyIdx==queueFamilyCount){
        dprintf(DBGT_ERROR,"No graphics queue which support presentation was found");
        exit(1);
    }
    vkRuntimeInfoP->graphics_queue_family_idx=queueFamilyIdx;
    free(queueFamiliyPropP);


    //Create logical device
    float queuePriority=1.0f;

    VkDeviceQueueCreateInfo QueueCreateInfo;
    QueueCreateInfo.sType=              VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    QueueCreateInfo.queueCount=         1;
    QueueCreateInfo.queueFamilyIndex=   vkRuntimeInfoP->graphics_queue_family_idx;
    QueueCreateInfo.pQueuePriorities=   &queuePriority;
    QueueCreateInfo.pNext=              NULL;
    QueueCreateInfo.flags=              0;

    VkDeviceCreateInfo DevCreateInfo;
    DevCreateInfo.sType=                    VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DevCreateInfo.pQueueCreateInfos=        &QueueCreateInfo;
    DevCreateInfo.queueCreateInfoCount=     1;
    DevCreateInfo.pNext=                    NULL;
    DevCreateInfo.enabledExtensionCount=    vkRuntimeInfoP->DevExtensionCount;
    DevCreateInfo.ppEnabledExtensionNames=  vkRuntimeInfoP->DevExtensionNamesPP;
    DevCreateInfo.enabledLayerCount=        vkRuntimeInfoP->DevLayerCount;
    DevCreateInfo.ppEnabledLayerNames=      vkRuntimeInfoP->DevLayerNamesPP;
    DevCreateInfo.pEnabledFeatures=         NULL;
    DevCreateInfo.flags=                    0;
    if(vkCreateDevice(vkRuntimeInfoP->physSelectedDevice,&DevCreateInfo,NULL,&(vkRuntimeInfoP->device))){
        dprintf(DBGT_ERROR,"Could not create Vulkan logical device");
        exit(1);
    }



    //Get handle for graphics queue
    vkGetDeviceQueue(vkRuntimeInfoP->device,vkRuntimeInfoP->graphics_queue_family_idx,0,&vkRuntimeInfoP->graphics_queue);


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
    if(VK_SUCCESS!=vkAllocateCommandBuffers(vkRuntimeInfoP->device,&CommandBufferCreateInfo,&(vkRuntimeInfoP->primaryCommandBuffer))){
        dprintf(DBGT_ERROR,"Could no allocate command buffers");
        exit(1);
    };
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
        tempXmlDynlistP=getAllSubelementsWith_freeArg234(engSetupRootP,Dl_utf32_fromString("Debug"),NULL,NULL,0,1);
    }else{
        tempXmlDynlistP=getAllSubelementsWith_freeArg234(engSetupRootP,Dl_utf32_fromString("Release"),NULL,NULL,0,1);
    }
    if(tempXmlDynlistP->itemcnt!=1){
        dprintf(DBGT_ERROR,"Invalid EngSetupFile format");
        exit(1);
    }
    engSetupDebOrRelP=((struct xmlTreeElement**)(tempXmlDynlistP->items))[0];
    printXMLsubelements(engSetupDebOrRelP);
    DlDelete(tempXmlDynlistP);
    return engSetupDebOrRelP;
};

uint32_t eng_get_version_number_from_UTF32DynlistP(struct DynamicList* inputStringP){
    struct DynamicList* versionNumP=Dl_utf32_to_Dl_int64_freeArg1(Dl_CMatch_create(2,'.','.'),inputStringP);
    if(versionNumP->itemcnt!=3){
        dprintf(DBGT_ERROR,"Invalid Version format");
        exit(1);
    }
    uint64_t* versionIntsP=(uint64_t*)(versionNumP->items);
    uint32_t version=VK_MAKE_VERSION(versionIntsP[0],versionIntsP[1],versionIntsP[2]);
    free(versionNumP);
    return version;
}

uint32_t eng_get_version_number_from_xmlemnt(struct xmlTreeElement* currentReqXmlP){
    struct DynamicList* currentReqLayerAttribP=(currentReqXmlP->attributes);
    struct DynamicList* minversionUTF32DynlistP=getValueFromKeyName_freeArg2(currentReqLayerAttribP,Dl_utf32_fromString("minversion"));
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

void eng_createSurface(struct VulkanRuntimeInfo* vkRuntimeInfoP,GLFWwindow* glfwWindowP){
    if(glfwCreateWindowSurface(vkRuntimeInfoP->instance,glfwWindowP,NULL,&vkRuntimeInfoP->surface)){
        dprintf(DBGT_ERROR,"Could not create vulkan presentation surface");
        exit(1);
    }
}

void eng_createSwapChain(struct VulkanRuntimeInfo* vkRuntimeInfoP,GLFWwindow* glfwWindowP){
    VkBool32 surfaceSupport=VK_TRUE;
    if(vkGetPhysicalDeviceSurfaceSupportKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->graphics_queue_family_idx,vkRuntimeInfoP->surface,&surfaceSupport)){
        dprintf(DBGT_ERROR,"Could not querry device surface support");
        exit(1);
    }
    if(surfaceSupport!=VK_TRUE){
        dprintf(DBGT_ERROR,"device does not support surface");
        exit(1);
    }


    //get basic surface capabilitiers
    struct VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->surface,&surfaceCapabilities);
    //get supported formats
    uint32_t formatCount=0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->surface,&formatCount,NULL);
    VkSurfaceFormatKHR* SurfaceFormatsP=(VkSurfaceFormatKHR*)malloc(formatCount*sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->surface,&formatCount,SurfaceFormatsP);
    //get supported present modes
    uint32_t presentModeCount=0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->surface,&presentModeCount,NULL);
    VkPresentModeKHR* PresentModeP=(VkPresentModeKHR*)malloc(presentModeCount*sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->surface,&presentModeCount,PresentModeP);

    //choose the format/mode we want
    uint32_t availableFormatIdx;
    for(availableFormatIdx=0;availableFormatIdx<formatCount;availableFormatIdx++){
        if(SurfaceFormatsP[availableFormatIdx].format==VK_FORMAT_B8G8R8A8_SRGB && SurfaceFormatsP[availableFormatIdx].colorSpace==VK_COLORSPACE_SRGB_NONLINEAR_KHR){
            break;
        }
    }
    if(formatCount==availableFormatIdx){
        dprintf(DBGT_ERROR,"No suitable VK format available");
        exit(1);
    }
    vkRuntimeInfoP->swapChainFormat=SurfaceFormatsP[availableFormatIdx];

    //choose presentation mode
    uint32_t availableModeIdx;
    for(availableModeIdx=0;availableModeIdx<presentModeCount;availableModeIdx++){
        if(PresentModeP[availableModeIdx]==VK_PRESENT_MODE_FIFO_KHR ){
            break;
        }
    }
    if(presentModeCount==availableModeIdx){
        dprintf(DBGT_ERROR,"No suitable VK present mode available");
        exit(1);
    }

    int32_t glfw_height;
    int32_t glfw_width;
    glfwGetFramebufferSize(glfwWindowP,&glfw_height,&glfw_width);
    (vkRuntimeInfoP->swapChainImageExtent).height=clamp_uint32_t(surfaceCapabilities.minImageExtent.height,(uint32_t)glfw_height,surfaceCapabilities.maxImageExtent.height);
    (vkRuntimeInfoP->swapChainImageExtent).width=clamp_uint32_t(surfaceCapabilities.minImageExtent.width,(uint32_t)glfw_width,surfaceCapabilities.maxImageExtent.width);

    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext=NULL;
    swapchainCreateInfo.flags=0;
    swapchainCreateInfo.surface=vkRuntimeInfoP->surface;
    swapchainCreateInfo.minImageCount=surfaceCapabilities.minImageCount+1;
    swapchainCreateInfo.imageFormat=(vkRuntimeInfoP->swapChainFormat).format;
    swapchainCreateInfo.imageColorSpace=(vkRuntimeInfoP->swapChainFormat).colorSpace;
    swapchainCreateInfo.imageExtent=(vkRuntimeInfoP->swapChainImageExtent);
    swapchainCreateInfo.imageArrayLayers=1;
    swapchainCreateInfo.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;        //don't share swapchain image between multiple queues
    swapchainCreateInfo.preTransform=surfaceCapabilities.currentTransform; //not image transform
    swapchainCreateInfo.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; //don't blend with other application windows
    swapchainCreateInfo.presentMode=PresentModeP[availableModeIdx];
    swapchainCreateInfo.clipped=VK_TRUE;    //Don't render pixels that are obstructed by other windows
    swapchainCreateInfo.oldSwapchain=VK_NULL_HANDLE;

    if(vkCreateSwapchainKHR(vkRuntimeInfoP->device,&swapchainCreateInfo,NULL,&vkRuntimeInfoP->swapChain)!=VK_SUCCESS){
        dprintf(DBGT_ERROR,"Could not create swap chain");
        exit(1);
    }

    vkGetSwapchainImagesKHR(vkRuntimeInfoP->device,vkRuntimeInfoP->swapChain,&vkRuntimeInfoP->swapChainImageCount,NULL);
    vkRuntimeInfoP->swapChainImagesP=(VkImage*)malloc(vkRuntimeInfoP->swapChainImageCount*sizeof(VkImage));
    vkGetSwapchainImagesKHR(vkRuntimeInfoP->device,vkRuntimeInfoP->swapChain,&vkRuntimeInfoP->swapChainImageCount,vkRuntimeInfoP->swapChainImagesP);
}

void eng_createImageViews(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    vkRuntimeInfoP->swapChainImageViewsP=(VkImageView*)malloc(vkRuntimeInfoP->swapChainImageCount*sizeof(VkImageView));
    for(uint32_t imageIndex=0;imageIndex<vkRuntimeInfoP->swapChainImageCount;imageIndex++){
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext=NULL;
        imageViewCreateInfo.flags=0;
        imageViewCreateInfo.components.r=VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g=VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b=VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a=VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.format=(vkRuntimeInfoP->swapChainFormat).format;
        imageViewCreateInfo.subresourceRange.layerCount=1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer=0;
        imageViewCreateInfo.subresourceRange.baseMipLevel=0;
        imageViewCreateInfo.subresourceRange.levelCount=1;
        imageViewCreateInfo.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.viewType=VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.image=(vkRuntimeInfoP->swapChainImagesP)[imageIndex];

        if(vkCreateImageView(vkRuntimeInfoP->device,&imageViewCreateInfo,NULL,&(vkRuntimeInfoP->swapChainImageViewsP[imageIndex]))){
            dprintf(DBGT_ERROR,"Could not create Image View for swap chain image");
            exit(1);
        }
    }
}
void eng_createRenderPass(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    VkAttachmentDescription ColorAttachments={0};
    ColorAttachments.flags=0;
    //ColorAttachments.format=vkRuntimeInfoP->swapChainFormat;
    ColorAttachments.format=(vkRuntimeInfoP->swapChainFormat).format;
    ColorAttachments.samples=VK_SAMPLE_COUNT_1_BIT;
    ColorAttachments.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    ColorAttachments.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachments.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachments.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachments.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    ColorAttachments.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ColorAttachmentRef={0};
    ColorAttachmentRef.attachment=0;
    ColorAttachmentRef.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subPasses={0};
    subPasses.flags=0;
    subPasses.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPasses.colorAttachmentCount=1;
    subPasses.pColorAttachments=&ColorAttachmentRef;

    VkRenderPassCreateInfo RenderPassInfo={0};
    RenderPassInfo.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassInfo.flags=0;
    RenderPassInfo.pNext=NULL;
    RenderPassInfo.dependencyCount=0;
    RenderPassInfo.pDependencies=NULL;
    RenderPassInfo.attachmentCount=1;
    RenderPassInfo.pAttachments=&ColorAttachments;
    RenderPassInfo.subpassCount=1;
    RenderPassInfo.pSubpasses=&subPasses;

    if(vkCreateRenderPass(vkRuntimeInfoP->device,&RenderPassInfo,NULL,&vkRuntimeInfoP->renderPass)){
        dprintf(DBGT_ERROR,"Could not create Render pass");
        exit(1);
    }

}

void eng_createGraphicsPipeline(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    //VertexInput
    VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateInfo;
    PipelineVertexInputStateInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    PipelineVertexInputStateInfo.pNext=NULL;
    PipelineVertexInputStateInfo.flags=0;
    PipelineVertexInputStateInfo.vertexAttributeDescriptionCount=0;
    PipelineVertexInputStateInfo.vertexBindingDescriptionCount=0;
    //needs to be set if we supply vertex buffers to our shader
    PipelineVertexInputStateInfo.pVertexAttributeDescriptions=NULL;
    PipelineVertexInputStateInfo.pVertexBindingDescriptions=NULL;

    VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyInfo;
    PipelineInputAssemblyInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    PipelineInputAssemblyInfo.pNext=NULL;
    PipelineInputAssemblyInfo.flags=0;
    PipelineInputAssemblyInfo.primitiveRestartEnable=VK_FALSE;
    PipelineInputAssemblyInfo.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    //Vertex Shader
    VkPipelineShaderStageCreateInfo VertexShaderStageCreateInfo;
    VertexShaderStageCreateInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    VertexShaderStageCreateInfo.pNext=NULL;
    VertexShaderStageCreateInfo.module=vkRuntimeInfoP->VertexShaderModule;
    VertexShaderStageCreateInfo.stage=VK_SHADER_STAGE_VERTEX_BIT;
    VertexShaderStageCreateInfo.flags=0;
    VertexShaderStageCreateInfo.pSpecializationInfo=NULL;
    VertexShaderStageCreateInfo.pName="main";

    //Rasterizer
    //Scissor
    VkRect2D scissor;
    scissor.offset.x=0;
    scissor.offset.y=0;
    scissor.extent=vkRuntimeInfoP->swapChainImageExtent;
    //Viewport
    VkViewport viewport;
    viewport.x=0.0f;
    viewport.y=0.0f;
    viewport.minDepth=0.0f;
    viewport.maxDepth=1.0f;
    viewport.height=vkRuntimeInfoP->swapChainImageExtent.height;
    viewport.width=vkRuntimeInfoP->swapChainImageExtent.width;
    //ViewportInfo
    VkPipelineViewportStateCreateInfo PipelineViewportInfo;
    PipelineViewportInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    PipelineViewportInfo.pNext=NULL;
    PipelineViewportInfo.flags=0;
    PipelineViewportInfo.viewportCount=1;
    PipelineViewportInfo.scissorCount=1;
    PipelineViewportInfo.pScissors=&scissor;
    PipelineViewportInfo.pViewports=&viewport;
    //RasterizerInfo
    VkPipelineRasterizationStateCreateInfo RasterizationInfo;
    RasterizationInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterizationInfo.pNext=NULL;
    RasterizationInfo.flags=0;
    RasterizationInfo.depthClampEnable=VK_FALSE;
    RasterizationInfo.rasterizerDiscardEnable=VK_FALSE;
    RasterizationInfo.polygonMode=VK_POLYGON_MODE_FILL;
    RasterizationInfo.lineWidth=1.0f;
    RasterizationInfo.cullMode=VK_CULL_MODE_BACK_BIT;
    RasterizationInfo.frontFace=VK_FRONT_FACE_CLOCKWISE;
    RasterizationInfo.depthBiasEnable=VK_FALSE;
    RasterizationInfo.depthBiasConstantFactor=0.0f;
    RasterizationInfo.depthBiasClamp=0.0f;
    RasterizationInfo.depthBiasSlopeFactor=0.0f;
    //Multisampling
    VkPipelineMultisampleStateCreateInfo MultisampleInfo;
    MultisampleInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultisampleInfo.pNext=NULL;
    MultisampleInfo.flags=0;
    MultisampleInfo.sampleShadingEnable=VK_FALSE;
    MultisampleInfo.alphaToOneEnable=VK_FALSE;
    MultisampleInfo.minSampleShading=1.0f;
    MultisampleInfo.pSampleMask=NULL;
    MultisampleInfo.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    MultisampleInfo.sampleShadingEnable=VK_FALSE;
    MultisampleInfo.alphaToCoverageEnable=VK_FALSE;

    //Fragment
    VkPipelineShaderStageCreateInfo FragmentShaderStageCreateInfo;
    FragmentShaderStageCreateInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    FragmentShaderStageCreateInfo.pNext=NULL;
    FragmentShaderStageCreateInfo.module=vkRuntimeInfoP->FragmentShaderModule;
    FragmentShaderStageCreateInfo.stage=VK_SHADER_STAGE_FRAGMENT_BIT;
    FragmentShaderStageCreateInfo.flags=0;
    FragmentShaderStageCreateInfo.pSpecializationInfo=NULL;
    FragmentShaderStageCreateInfo.pName="main";

    //Blending
    VkPipelineColorBlendAttachmentState ColorBlendAttachment;
    ColorBlendAttachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    ColorBlendAttachment.blendEnable=VK_FALSE;
    ColorBlendAttachment.colorBlendOp=VK_BLEND_OP_ADD;
    ColorBlendAttachment.srcColorBlendFactor=VK_BLEND_FACTOR_ONE;
    ColorBlendAttachment.dstColorBlendFactor=VK_BLEND_FACTOR_ZERO;
    ColorBlendAttachment.alphaBlendOp=VK_BLEND_OP_ADD;
    ColorBlendAttachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
    ColorBlendAttachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;

    VkPipelineColorBlendStateCreateInfo ColorBlendInfo;
    ColorBlendInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendInfo.pNext=NULL;
    ColorBlendInfo.flags=0;
    ColorBlendInfo.logicOp=VK_FALSE;
    ColorBlendInfo.attachmentCount=1;
    ColorBlendInfo.pAttachments=&ColorBlendAttachment;
    ColorBlendInfo.blendConstants[0]=0.0f;
    ColorBlendInfo.blendConstants[1]=0.0f;
    ColorBlendInfo.blendConstants[2]=0.0f;
    ColorBlendInfo.blendConstants[3]=0.0f;

    //Pipeline Layout for Uniforms
    VkPipelineLayout PipelineLayout;
    VkPipelineLayoutCreateInfo PipelineLayoutInfo;
    PipelineLayoutInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutInfo.pNext=NULL;
    PipelineLayoutInfo.flags=0;
    PipelineLayoutInfo.setLayoutCount=0;
    PipelineLayoutInfo.pSetLayouts=NULL;
    PipelineLayoutInfo.pushConstantRangeCount=0;
    PipelineLayoutInfo.pPushConstantRanges=NULL;

    if(vkCreatePipelineLayout(vkRuntimeInfoP->device,&PipelineLayoutInfo,NULL,&PipelineLayout)){
        dprintf(DBGT_ERROR,"Could not create pipeline layout");
        exit(1);
    }

    //Assemble everything
    VkPipelineShaderStageCreateInfo Stages[2]={
        VertexShaderStageCreateInfo,
        FragmentShaderStageCreateInfo
    };

    VkGraphicsPipelineCreateInfo PipelineInfo;
    PipelineInfo.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineInfo.pNext=NULL;
    PipelineInfo.stageCount=2;
    PipelineInfo.pStages=Stages;
    PipelineInfo.pVertexInputState=&PipelineVertexInputStateInfo;
    PipelineInfo.pInputAssemblyState=&PipelineInputAssemblyInfo;
    PipelineInfo.pColorBlendState=&ColorBlendInfo;
    PipelineInfo.pDepthStencilState=NULL;
    PipelineInfo.pDynamicState=NULL;
    PipelineInfo.pMultisampleState=&MultisampleInfo;
    PipelineInfo.pRasterizationState=&RasterizationInfo;
    PipelineInfo.pTessellationState=NULL;
    PipelineInfo.pViewportState=&PipelineViewportInfo;

    PipelineInfo.layout=PipelineLayout;
    PipelineInfo.renderPass=vkRuntimeInfoP->renderPass;
    PipelineInfo.subpass=0;
    if(vkCreateGraphicsPipelines(vkRuntimeInfoP->device,VK_NULL_HANDLE,1,&PipelineInfo,NULL,&(vkRuntimeInfoP->graphicsPipeline))){
        dprintf(DBGT_ERROR,"Could not create graphics pipeline");
        exit(1);
    }
}

void eng_createFramebuffers(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    vkRuntimeInfoP->FramebufferCount=vkRuntimeInfoP->swapChainImageCount;
    vkRuntimeInfoP->FramebufferP=malloc(sizeof(VkFramebuffer)*vkRuntimeInfoP->FramebufferCount);
    for(uint32_t framebufferIdx=0;framebufferIdx<vkRuntimeInfoP->FramebufferCount;framebufferIdx++){
        VkFramebufferCreateInfo FramebufferInfo={0};
        FramebufferInfo.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FramebufferInfo.renderPass=vkRuntimeInfoP->renderPass;
        FramebufferInfo.height=vkRuntimeInfoP->swapChainImageExtent.height;
        FramebufferInfo.width=vkRuntimeInfoP->swapChainImageExtent.width;
        FramebufferInfo.layers=1;
        FramebufferInfo.attachmentCount=1;
        FramebufferInfo.pAttachments=&(vkRuntimeInfoP->swapChainImageViewsP[framebufferIdx]);
        if(vkCreateFramebuffer(vkRuntimeInfoP->device,&FramebufferInfo,NULL,&(vkRuntimeInfoP->FramebufferP[framebufferIdx]))){
            dprintf(DBGT_ERROR,"Could not create framebuffer");
            exit(1);
        }
    }
}

void eng_createCommandBuffer(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    vkRuntimeInfoP->CommandbufferCount=vkRuntimeInfoP->FramebufferCount;
    vkRuntimeInfoP->CommandbufferP=(VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*vkRuntimeInfoP->CommandbufferCount);

    VkCommandPoolCreateInfo CommandPoolInfo={0};
    CommandPoolInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolInfo.queueFamilyIndex=vkRuntimeInfoP->graphics_queue_family_idx;
    if(vkCreateCommandPool(vkRuntimeInfoP->device,&CommandPoolInfo,NULL,&(vkRuntimeInfoP->commandPool))){
        dprintf(DBGT_ERROR,"Could not create command pool");
        exit(1);
    }
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo={0};
    CommandBufferAllocateInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocateInfo.commandPool=vkRuntimeInfoP->commandPool;
    CommandBufferAllocateInfo.commandBufferCount=vkRuntimeInfoP->CommandbufferCount;
    CommandBufferAllocateInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    if(vkAllocateCommandBuffers(vkRuntimeInfoP->device,&CommandBufferAllocateInfo,vkRuntimeInfoP->CommandbufferP)){
        dprintf(DBGT_ERROR,"Could not allocate command buffer");
        exit(1);
    }
    for(uint32_t CommandBufferIdx=0;CommandBufferIdx<vkRuntimeInfoP->CommandbufferCount;CommandBufferIdx++){
        VkCommandBufferBeginInfo CommandBufferBeginInfo={0};
        CommandBufferBeginInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if(vkBeginCommandBuffer(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],&CommandBufferBeginInfo)){
            dprintf(DBGT_ERROR,"Could not begin recording to command buffer");
            exit(1);
        }

        VkClearValue clearColor={{0.0f,0.0f,0.0f,1.0f}};
        VkRenderPassBeginInfo RenderPassInfo={0};
        RenderPassInfo.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassInfo.framebuffer=vkRuntimeInfoP->FramebufferP[CommandBufferIdx];
        RenderPassInfo.clearValueCount=1;
        RenderPassInfo.pClearValues=&clearColor;
        RenderPassInfo.renderArea.extent=vkRuntimeInfoP->swapChainImageExtent;
        RenderPassInfo.renderArea.offset.x=0;
        RenderPassInfo.renderArea.offset.y=0;
        RenderPassInfo.renderPass=vkRuntimeInfoP->renderPass;
        vkCmdBeginRenderPass(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],&RenderPassInfo,VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],VK_PIPELINE_BIND_POINT_GRAPHICS,vkRuntimeInfoP->graphicsPipeline);
        vkCmdDraw(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],3,1,0,0);
        vkCmdEndRenderPass(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx]);
        vkEndCommandBuffer(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx]);
    }
}

void eng_createSynchronizationPrimitives(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    //Create Semaphores to sync vkAcquireNextImageKHR with start of buffer execution and
    //sync end of buffer executing with vkQueuePresentKHR
    VkSemaphoreCreateInfo SemaphoreInfo={0};
    SemaphoreInfo.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    //Create Fences to sync cpu gpu
    VkFenceCreateInfo FenceInfo={0};
    FenceInfo.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    FenceInfo.flags=VK_FENCE_CREATE_SIGNALED_BIT;
    vkRuntimeInfoP->imageAvailableSemaphoreP=(VkSemaphore*)malloc(sizeof(VkSemaphore)*vkRuntimeInfoP->CommandbufferCount);
    vkRuntimeInfoP->renderFinishedSemaphoreP=(VkSemaphore*)malloc(sizeof(VkSemaphore)*vkRuntimeInfoP->CommandbufferCount);
    vkRuntimeInfoP->ImageAlreadyProcessingFenceP=(VkFence*)malloc(sizeof(VkFence)*vkRuntimeInfoP->CommandbufferCount);

    for(uint32_t FrameIdx=0;FrameIdx<vkRuntimeInfoP->CommandbufferCount;FrameIdx++){
        if(vkCreateFence(vkRuntimeInfoP->device,&FenceInfo,NULL,(vkRuntimeInfoP->ImageAlreadyProcessingFenceP)+FrameIdx)){
            dprintf(DBGT_ERROR,"Could not create image processing fence");
            exit(1);
        }
        if(vkCreateSemaphore(vkRuntimeInfoP->device,&SemaphoreInfo,NULL,(vkRuntimeInfoP->imageAvailableSemaphoreP)+FrameIdx)){
            dprintf(DBGT_ERROR,"Could not create image available semaphore");
            exit(1);
        }
        if(vkCreateSemaphore(vkRuntimeInfoP->device,&SemaphoreInfo,NULL,(vkRuntimeInfoP->renderFinishedSemaphoreP)+FrameIdx)){
            dprintf(DBGT_ERROR,"Could not create image available semaphore");
            exit(1);
        }
    }

}

GLFWerrorfun error_callback(int code,const char* description){
    dprintf(DBGT_ERROR,"Error in glfw code: %d, \n String %s",code,description);
}

void eng_draw(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    static uint32_t nextImageIdx=0;
    //Work
    if(vkWaitForFences(vkRuntimeInfoP->device,1,(vkRuntimeInfoP->ImageAlreadyProcessingFenceP)+nextImageIdx,VK_TRUE,UINT64_MAX)){
        dprintf(DBGT_ERROR,"Waiting for fence timeout");
        exit(1);
    }
    if(vkResetFences(vkRuntimeInfoP->device,1,(vkRuntimeInfoP->ImageAlreadyProcessingFenceP)+nextImageIdx)){
        dprintf(DBGT_ERROR,"Could not reset fence");
        exit(1);
    }
    //Get next image from swapChain
    if(vkAcquireNextImageKHR(vkRuntimeInfoP->device,vkRuntimeInfoP->swapChain,UINT64_MAX,(vkRuntimeInfoP->imageAvailableSemaphoreP)[nextImageIdx],VK_NULL_HANDLE,&nextImageIdx)){
        dprintf(DBGT_ERROR,"Error while requesting swapchain image");
    }

    VkPipelineStageFlags waitStages[]={VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo SubmitInfo={0};
    SubmitInfo.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.waitSemaphoreCount=1;
    SubmitInfo.pWaitDstStageMask=waitStages;
    SubmitInfo.pWaitSemaphores=(vkRuntimeInfoP->imageAvailableSemaphoreP)+nextImageIdx;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers=(vkRuntimeInfoP->CommandbufferP)+nextImageIdx;
    SubmitInfo.signalSemaphoreCount=1;
    SubmitInfo.pSignalSemaphores=(vkRuntimeInfoP->renderFinishedSemaphoreP)+nextImageIdx;

    if(vkQueueSubmit(vkRuntimeInfoP->graphics_queue,1,&SubmitInfo,vkRuntimeInfoP->ImageAlreadyProcessingFenceP[nextImageIdx])){
        dprintf(DBGT_ERROR,"Could not submit command buffer to graphic queue");
        exit(1);
    }
    VkPresentInfoKHR PresentInfo={0};
    PresentInfo.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount=1;
    PresentInfo.pWaitSemaphores=(vkRuntimeInfoP->renderFinishedSemaphoreP)+nextImageIdx;
    PresentInfo.swapchainCount=1;
    PresentInfo.pSwapchains=&(vkRuntimeInfoP->swapChain);
    PresentInfo.pImageIndices=&nextImageIdx;
    if(vkQueuePresentKHR(vkRuntimeInfoP->graphics_queue,&PresentInfo)){
        dprintf(DBGT_ERROR,"Could not queue presentation");
    }

    nextImageIdx+=1;
    nextImageIdx%=vkRuntimeInfoP->CommandbufferCount;


}

int main(int argc, char** argv){
    glfwInit();
    glfwSetErrorCallback(error_callback);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    #ifdef DEBUG
        struct xmlTreeElement* eng_setupxmlP=eng_get_eng_setupxml("./res/vk_setup.xml",1);
    #else
        struct xmlTreeElement* eng_setupxmlP=eng_get_eng_setupxml("./res/vk_setup.xml",0);
    #endif
    struct xmlTreeElement* applicationNameXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,Dl_utf32_fromString("ApplicationName"),NULL,NULL,0,1);
    char* applicationNameCharP=Dl_utf32_toString(getFirstSubelementWith(applicationNameXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0)->content);
    GLFWwindow* mainWindowP = glfwCreateWindow(1920, 1080, applicationNameCharP, NULL, NULL);
    free(applicationNameCharP);

    if(GLFW_FALSE==glfwVulkanSupported()){
        dprintf(DBGT_ERROR,"Vulkan not supported on this device");
        exit(1);
    }

    struct VulkanRuntimeInfo engVkRuntimeInfo;
    eng_createInstance(&engVkRuntimeInfo,eng_setupxmlP);
    uint8_t* deviceRankingP=eng_vulkan_generate_device_ranking(&engVkRuntimeInfo,eng_setupxmlP);
    eng_createSurface(&engVkRuntimeInfo,mainWindowP);
    eng_createDevice(&engVkRuntimeInfo,deviceRankingP);
    eng_createSwapChain(&engVkRuntimeInfo,mainWindowP);
    eng_createImageViews(&engVkRuntimeInfo);
    eng_createShaderModule(&engVkRuntimeInfo,"./res/shader1.xml");
    eng_createRenderPass(&engVkRuntimeInfo);
    eng_createGraphicsPipeline(&engVkRuntimeInfo);
    eng_createFramebuffers(&engVkRuntimeInfo);
    eng_createCommandBuffer(&engVkRuntimeInfo);
    eng_createSynchronizationPrimitives(&engVkRuntimeInfo);
    dprintf(DBGT_INFO,"Got here");
    while (!glfwWindowShouldClose(mainWindowP)) {
        eng_draw(&engVkRuntimeInfo);
        glfwPollEvents();
    }
}
