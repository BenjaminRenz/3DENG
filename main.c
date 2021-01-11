#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void eng_createShaderModule(char* ShaderFileLocation){
    FILE* ShaderXmlFileP=fopen(ShaderFileLocation,"rb");
    if(ShaderXmlFileP==NULL){
        dprintf(DBGT_ERROR,"Could not open file %s for compilation",ShaderFileLocation);
        exit(1);
    }
    struct xmlTreeElement* xmlRootElementP;
    readXML(ShaderXmlFileP,&xmlRootElementP);
    struct xmlTreeElement* VertexShaderXmlElmntP=getFirstSubelementWith(xmlRootElementP,Dl_utf32_fromString("vertex"),NULL,NULL,NULL,1);
    struct xmlTreeElement* VertexShaderContentXmlElmntP=getFirstSubelementWith(VertexShaderXmlElmntP,NULL,NULL,NULL,xmltype_chardata,1);
    uint8_t* VertexShaderAsciiSourceP=(char*)malloc((VertexShaderContentXmlElmntP->content->itemcnt+1)*sizeof(char));
    uint32_t sourceLength=utf32CutASCII(VertexShaderContentXmlElmntP->content->items,VertexShaderContentXmlElmntP->content->itemcnt,VertexShaderAsciiSourceP);
    dprintf(DBGT_INFO,"Raw shader string:\n%s",VertexShaderAsciiSourceP);
    shaderc_compiler_t shaderCompilerObj=shaderc_compiler_initialize();
    shaderc_compilation_result_t compResult=shaderc_compile_into_spv(shaderCompilerObj,VertexShaderAsciiSourceP,sourceLength,shaderc_glsl_vertex_shader,ShaderFileLocation,"main",NULL);
    if(shaderc_result_get_compilation_status(compResult)){
        dprintf(DBGT_ERROR,"Shader compilation failed");
        if(shaderc_result_get_num_errors(compResult)){
            dprintf(DBGT_ERROR,"Error was:\n%s",shaderc_result_get_error_message(compResult));
            exit(1);
        }
    }
    dprintf(DBGT_INFO,"While compiling %s there were %d warnings.",ShaderFileLocation,shaderc_result_get_num_warnings(compResult));
    shaderc_compiler_release(shaderCompilerObj);
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


    VkCommandPool commandPool;
    VkCommandBuffer primaryCommandBuffer;
};



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
        if(PresentModeP[availableModeIdx]==VK_PRESENT_MODE_IMMEDIATE_KHR){
            break;
        }
    }
    if(presentModeCount==availableModeIdx){
        dprintf(DBGT_ERROR,"No suitable VK present mode available");
        exit(1);
    }

    uint32_t glfw_height;
    uint32_t glfw_width;
    glfwGetFramebufferSize(glfwWindowP,&glfw_height,&glfw_width);
    (vkRuntimeInfoP->swapChainImageExtent).height=clamp_uint32_t(surfaceCapabilities.minImageExtent.height,glfw_height,surfaceCapabilities.maxImageExtent.height);
    (vkRuntimeInfoP->swapChainImageExtent).width=clamp_uint32_t(surfaceCapabilities.minImageExtent.width,glfw_width,surfaceCapabilities.maxImageExtent.width);

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
void createGraphicsPipeline(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    VkShaderModule VertexShader;
    VkShaderModuleCreateInfo ShaderModuleCreateInfo;
    ShaderModuleCreateInfo.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.pNext=NULL;
    ShaderModuleCreateInfo.flags=0;
    //ShaderModuleCreateInfo.codeSize
    //ShaderModuleCreateInfo.pCode

    //vkCreateShaderModule(vkRuntimeInfoP->device,&ShaderModuleCreateInfo,NULL,&VertexShader);
}


GLFWerrorfun error_callback(int code,const char* description){
    dprintf(DBGT_ERROR,"Error in glfw code: %d, \n String %s",code,description);
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

    if(GLFW_FALSE==glfwVulkanSupported){
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
    eng_createShaderModule("./res/shader1.xml");
    dprintf(DBGT_INFO,"Got here");
    while (!glfwWindowShouldClose(mainWindowP)) {
        glfwPollEvents();
    }
}
