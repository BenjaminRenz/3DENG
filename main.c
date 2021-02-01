#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define GLFW_INCLUDE_VULKAN
#include "vulkan/vulkan.h"
#include "shaderc/shaderc.h"
#include "inc/linmath.h"
#include "submodules/glfw/glfw3.h"

#include "submodules/xmlReader/debug.h"
#include "submodules/xmlReader/xmlReader.h"
#include "submodules/xmlReader/stringutils.h"

uint32_t eng_get_version_number_from_xmlemnt(struct xmlTreeElement* currentReqXmlP);
uint32_t eng_get_version_number_from_UTF32DynlistP(struct DynamicList* inputStringP);
struct xmlTreeElement* eng_get_eng_setupxml(char* FilePath,int debug_enabled);



struct DataFromDae{
    struct DynamicList* PositionsDlP;
    struct DynamicList* PositionsAndNormalIndicesDlP; //alternating, offset by 1
    struct DynamicList* NormalsDlP;
};

struct eng3DPINBuffer{
    VkDeviceSize            TotalSizeWithPadding;

    VkBuffer                PINBufferHandle;
    VkMemoryRequirements    PINBufferMemoryRequirements;

    VkDeviceSize            PositionBufferSize;
    VkDeviceSize            IndexBufferSize;        //combines position and normal indices for each face
    VkDeviceSize            NormalBufferSize;
};

struct eng3dObject{
    struct DataFromDae    daeData;
    struct eng3DPINBuffer stagingBuffers;
    struct eng3DPINBuffer deviceBuffers;
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
    uint32_t imagesInFlightCount;
    VkImage* swapChainImagesP;
    VkExtent2D swapChainImageExtent;
    VkSurfaceFormatKHR swapChainFormat;
    VkImageView* swapChainImageViewsP;

    VkRenderPass renderPass;

    VkShaderModule VertexShaderModule;
    VkShaderModule FragmentShaderModule;

    VkPipeline graphicsPipeline;
    VkPipelineLayout graphicsPipelineLayout;

    VkFramebuffer* FramebufferP;

    VkCommandBuffer* CommandbufferP;

    VkCommandPool commandPool;
    VkCommandBuffer primaryCommandBuffer;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet* descriptorSetsP;
    VkDescriptorSetLayout* descriptorSetLayoutsP;

    VkSemaphore* imageAvailableSemaphoreP;
    VkSemaphore* renderFinishedSemaphoreP;
    VkFence*     ImageAlreadyProcessingFenceP;

    VkDeviceMemory StaticPINBufferMemory;
    VkBuffer UniformBufferHandle;
    VkDeviceMemory UniformBufferMemory;
};

struct eng3dObject ObjectToBeLoaded;



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

uint32_t clamp_uint32_t(uint32_t lower_bound,uint32_t clampedValueInput,uint32_t upper_bound){
    return max_uint32_t(min_uint32_t(upper_bound,clampedValueInput),lower_bound);
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



void loadDaeObject(char* filePath,char* meshId,struct DataFromDae* outputDataP){
    dprintf(DBGT_INFO,"%llu,%llu,%llu,%llu",sizeof(uint32_t),sizeof(void*),sizeof(void**),sizeof(unsigned int));
    struct DynamicList* meshIdDlp=Dl_utf32_fromString(meshId);

    FILE* cylinderDaeFileP=fopen(filePath,"rb");
    struct xmlTreeElement* xmlDaeRootP=0;
    readXML(cylinderDaeFileP,&xmlDaeRootP);
    fclose(cylinderDaeFileP);
    //printXMLsubelements(xmlDaeRootP);

    struct xmlTreeElement* xmlColladaElementP=getNthChildElmntOrChardata(xmlDaeRootP,0);
    struct xmlTreeElement* xmlLibGeoElementP=getFirstSubelementWith_freeArg234(xmlColladaElementP,Dl_utf32_fromString("library_geometries"),NULL,NULL,0,0);
    struct xmlTreeElement* xmlGeoElementP=getFirstSubelementWith_freeArg234(xmlLibGeoElementP,Dl_utf32_fromString("geometry"),Dl_utf32_fromString("id"),DlDuplicate(sizeof(uint32_t),meshIdDlp),0,0); //does  not work?
    if(!xmlGeoElementP){
        dprintf(DBGT_ERROR,"The collada file does not contain a mesh with the name %s",meshId);
        exit(1);
    }
    struct xmlTreeElement* xmlMeshElementP=getFirstSubelementWith_freeArg234(xmlGeoElementP,Dl_utf32_fromString("mesh"),NULL,NULL,0,0);

    //Get Triangles
    struct xmlTreeElement* xmlTrianglesP=getFirstSubelementWith_freeArg234(xmlMeshElementP,Dl_utf32_fromString("triangles"),NULL,NULL,0,0);
    struct DynamicList* TrianglesCountString=getValueFromKeyName_freeArg2(xmlTrianglesP->attributes,Dl_utf32_fromString("count"));
    struct DynamicList* TrianglesCount=Dl_utf32_to_Dl_int64_freeArg1(Dl_CMatch_create(2,' ',' '),TrianglesCountString);
    dprintf(DBGT_INFO,"Model has %lld triangles",((int64_t*)TrianglesCount->items)[0]);

    struct xmlTreeElement* xmlTrianglesOrderP=getFirstSubelementWith_freeArg234(xmlTrianglesP,Dl_utf32_fromString("p"),NULL,NULL,0,0);
    if(!xmlTrianglesOrderP){
        dprintf(DBGT_ERROR,"No Triangles Order list found");exit(1);
    }
    printXMLsubelements(xmlTrianglesOrderP);
    struct xmlTreeElement* xmlTrianglesOrderContentP=getNthChildElmntOrChardata(xmlTrianglesOrderP,0);   //TODO does not work because getNthSubelement only considers non chardata elements...
    if(!xmlTrianglesOrderContentP->content->itemcnt){
        dprintf(DBGT_ERROR,"Triangles Order List empty");exit(1);
    }
    outputDataP->PositionsAndNormalIndicesDlP=Dl_utf32_to_Dl_int64_freeArg1(Dl_CMatch_create(2,' ',' '),xmlTrianglesOrderContentP->content);
    dprintf(DBGT_INFO,"Found Triangle order list with %d entries",outputDataP->PositionsAndNormalIndicesDlP->itemcnt);

    //Get Normals
    struct xmlTreeElement* xmlNormalsSourceP=getFirstSubelementWith_freeArg234(xmlMeshElementP,Dl_utf32_fromString("source"),
                                                                                                Dl_utf32_fromString("id"),
                                                                                                DlCombine_freeArg3(sizeof(uint32_t),meshIdDlp,Dl_utf32_fromString("-normals")),
                                                                                                0,0);
    struct xmlTreeElement* xmlNormalsFloatP=getFirstSubelementWith_freeArg234(xmlNormalsSourceP,Dl_utf32_fromString("float_array"),
                                                                                                Dl_utf32_fromString("id"),
                                                                                                DlCombine_freeArg3(sizeof(uint32_t),meshIdDlp,Dl_utf32_fromString("-normals-array")),
                                                                                                0,0);
    struct DynamicList* NormalsCountString=getValueFromKeyName_freeArg2(xmlNormalsFloatP->attributes,Dl_utf32_fromString("count"));
    Dl_utf32_print(NormalsCountString);
    struct DynamicList* NormalsCount=Dl_utf32_to_Dl_int64_freeArg1(Dl_CMatch_create(2,' ',' '),NormalsCountString);
    dprintf(DBGT_INFO,"Model has count %lld normal coordinates",((int64_t*)NormalsCount->items)[0]);
    struct xmlTreeElement* xmlNormalsFloatContentP=getNthChildElmntOrChardata(xmlNormalsFloatP,0);
    outputDataP->NormalsDlP=Dl_utf32_to_Dl_float_freeArg123(Dl_CMatch_create(4,' ',' ','\t','\t'),Dl_CMatch_create(4,'e','e','E','E'),Dl_CMatch_create(2,'.','.'),xmlNormalsFloatContentP->content);
    dprintf(DBGT_INFO,"Model has %d normal coordinates",outputDataP->NormalsDlP->itemcnt);

    //Get Positions
    struct xmlTreeElement* xmlPositionsSourceP=getFirstSubelementWith_freeArg234(xmlMeshElementP,Dl_utf32_fromString("source"),
                                                                     Dl_utf32_fromString("id"),DlCombine_freeArg3(sizeof(uint32_t),meshIdDlp,Dl_utf32_fromString("-positions")),0,0);

    struct xmlTreeElement* xmlPositionsFloatP=getFirstSubelementWith_freeArg234(xmlPositionsSourceP,Dl_utf32_fromString("float_array"),
                                                                    Dl_utf32_fromString("id"),DlCombine_freeArg3(sizeof(uint32_t),meshIdDlp,Dl_utf32_fromString("-positions-array")),0,0);

    struct DynamicList* PositionsCountString=getValueFromKeyName_freeArg2(xmlPositionsFloatP->attributes,Dl_utf32_fromString("count"));
    struct DynamicList* PositionsCount=Dl_utf32_to_Dl_int64_freeArg1(Dl_CMatch_create(2,' ',' '),PositionsCountString);
    //if(xmlPositionsFloatP->content->type!=dynlisttype_utf32chars){dprintf(DBGT_ERROR,"Invalid content type");return 0;}
    struct xmlTreeElement* xmlPositionsFloatContentP=getNthChildElmntOrChardata(xmlPositionsFloatP,0);
    outputDataP->PositionsDlP=Dl_utf32_to_Dl_float_freeArg123(Dl_CMatch_create(4,' ',' ','\t','\t'),Dl_CMatch_create(4,'e','e','E','E'),Dl_CMatch_create(2,'.','.'),xmlPositionsFloatContentP->content);

    //TODO Remove xml from memory
}

uint32_t countBitsInUint32(uint32_t input){
    //add subbits in increasing bin sizes (bit0+bit1),(bit2+bit3),...
    input=(input&0x55555555)+((input>>1)&0x55555555);
    input=(input&0x33333333)+((input>>2)&0x33333333);
    input=(input&0x0f0f0f0f)+((input>>4)&0x0f0f0f0f);
    input=(input&0x00ff00ff)+((input>>8)&0x00ff00ff);
    input=(input&0x0000ffff)+(input>>16);
    return input;
}

//ToDo do actual memory usage calculations for each heap
int32_t findBestMemoryType(struct VulkanRuntimeInfo* vkRuntimeInfoP,VkMemoryPropertyFlags forbiddenBitfield,VkMemoryPropertyFlags requiredBitfield, VkMemoryPropertyFlags uprankBitfield, VkMemoryPropertyFlags* ReturnBitfieldP, VkDeviceSize minsize){
    //Get information about all available memory types
    VkPhysicalDeviceMemoryProperties DeviceMemProperties;
    vkGetPhysicalDeviceMemoryProperties(vkRuntimeInfoP->physSelectedDevice,&DeviceMemProperties);
    int32_t bestRankingMemoryTypeIdx=-1;
    uint32_t bestRanking=0;
    for(uint32_t MemoryTypeIdx=0;MemoryTypeIdx<DeviceMemProperties.memoryTypeCount;MemoryTypeIdx++){
        uint32_t currentRank=1;
        //Check if memory has the required size
        uint32_t CurrentMemoryTypeHeapIdx=DeviceMemProperties.memoryTypes[MemoryTypeIdx].heapIndex;
        if(DeviceMemProperties.memoryHeaps[CurrentMemoryTypeHeapIdx].size<minsize){
            continue;
        }
        //Check if memory is has any forbiddenProperty set
        if(DeviceMemProperties.memoryTypes[MemoryTypeIdx].propertyFlags&forbiddenBitfield){
            continue;
        }
        //Check if memory has all requiriedProperty bits set
        if((DeviceMemProperties.memoryTypes[MemoryTypeIdx].propertyFlags&requiredBitfield)!=requiredBitfield){
            continue;
        }

        //Uprank memory that has VkMemoryPropertyFlags uprankBitfield set
        currentRank+=countBitsInUint32(DeviceMemProperties.memoryTypes[MemoryTypeIdx].propertyFlags&uprankBitfield);
        if(currentRank>bestRanking){
            bestRanking=currentRank;
            bestRankingMemoryTypeIdx=(int32_t)MemoryTypeIdx;//we don't expect memory types over 2^31, so use the extra bit for error handling
            if(ReturnBitfieldP){
                *ReturnBitfieldP=DeviceMemProperties.memoryTypes[MemoryTypeIdx].propertyFlags;
            }
        }
    }
    if(!bestRanking){
        dprintf(DBGT_ERROR,"No suitable memory type found");
        exit(1);
    }
    return bestRankingMemoryTypeIdx;
}


void eng_ceatePINBufferAndCalcSize(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng3DPINBuffer* PINBuffersP,VkBufferUsageFlags usageTransferSourceOrDest){

    VkBufferCreateInfo BufferInfo={0};
    BufferInfo.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferInfo.usage=
        usageTransferSourceOrDest|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    BufferInfo.sharingMode=VK_SHARING_MODE_EXCLUSIVE;

    //Create combined Position-, Buffer
    BufferInfo.size=
        PINBuffersP->PositionBufferSize+
        PINBuffersP->NormalBufferSize+
        PINBuffersP->IndexBufferSize;

    if(vkCreateBuffer(vkRuntimeInfoP->device,&BufferInfo,NULL,&(PINBuffersP->PINBufferHandle))){
        dprintf(DBGT_ERROR,"Could not create buffer");
        exit(1);
    }
    //get memory requirements
    vkGetBufferMemoryRequirements(vkRuntimeInfoP->device,PINBuffersP->PINBufferHandle,&(PINBuffersP->PINBufferMemoryRequirements));
    //Check if already aligned
    if((PINBuffersP->PINBufferMemoryRequirements.size)%(PINBuffersP->PINBufferMemoryRequirements.alignment)){
        //calculate multiples of alignment
        PINBuffersP->TotalSizeWithPadding=((PINBuffersP->PINBufferMemoryRequirements.size/PINBuffersP->PINBufferMemoryRequirements.alignment)+1);
        PINBuffersP->TotalSizeWithPadding*=PINBuffersP->PINBufferMemoryRequirements.alignment;
    }else{
        PINBuffersP->TotalSizeWithPadding=PINBuffersP->PINBufferMemoryRequirements.size;
    }
}

void eng_load_static_models(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    //Iterate over dae files and get number of vertices/indices and calculate their size
    VkDeviceSize totalStagingPIUBufferSize=0;

    //TODO remove this an add real object loader
    //TODO add for loop over all models we wish to load
    //struct eng3dObject ObjectToBeLoaded;    TODO remove this global hack
    loadDaeObject("./res/kegel_ohne_camera.dae","Cylinder-mesh",&(ObjectToBeLoaded.daeData));
    ObjectToBeLoaded.stagingBuffers.PositionBufferSize=     ((ObjectToBeLoaded.daeData.PositionsDlP->itemcnt        *sizeof(float))/3)*4; //multidimensional vectors need to be aligned to N,2N or 4N of element size
    ObjectToBeLoaded.stagingBuffers.IndexBufferSize=((ObjectToBeLoaded.daeData.PositionsAndNormalIndicesDlP->itemcnt*sizeof(uint32_t))/2); //only every second index is for the positions
    ObjectToBeLoaded.stagingBuffers.NormalBufferSize=       ((ObjectToBeLoaded.daeData.NormalsDlP->itemcnt          *sizeof(float))/3)*4;          //multidimensional vectors need to be aligned to N,2N or 4N of element size


    //
    //Setup Uniform Buffer
    //
    //Create Uniform Buffers Handle, this time only host_visible and not neccesarily device local
    VkBufferCreateInfo UniformBufferCreateInfo={0};
    UniformBufferCreateInfo.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    UniformBufferCreateInfo.queueFamilyIndexCount=0;
    UniformBufferCreateInfo.pQueueFamilyIndices=&(vkRuntimeInfoP->graphics_queue_family_idx);
    UniformBufferCreateInfo.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    UniformBufferCreateInfo.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    UniformBufferCreateInfo.size=sizeof(mat4x4)*vkRuntimeInfoP->imagesInFlightCount; //ToDo support per object mvp matrices
    vkCreateBuffer(vkRuntimeInfoP->device,&UniformBufferCreateInfo,NULL,&vkRuntimeInfoP->UniformBufferHandle);
    //Get Memory Requirements
    VkMemoryRequirements UniformBufferMemoryRequirements;
    vkGetBufferMemoryRequirements(vkRuntimeInfoP->device,vkRuntimeInfoP->UniformBufferHandle,&UniformBufferMemoryRequirements);
    VkDeviceSize UniformBufferMemorySizeWithPadding=0;
    UniformBufferMemorySizeWithPadding=UniformBufferMemoryRequirements.size;
    if(UniformBufferMemoryRequirements.size%UniformBufferMemoryRequirements.alignment){ //End of buffer is not aligned, so pad size so that it is
        UniformBufferMemorySizeWithPadding/=UniformBufferMemoryRequirements.alignment;
        UniformBufferMemorySizeWithPadding++;
        UniformBufferMemorySizeWithPadding*=UniformBufferMemoryRequirements.alignment;
    }
    //Allocate Memory for unifrom buffer
    VkMemoryAllocateInfo UniformBufferAllocateInfo={0};
    UniformBufferAllocateInfo.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    UniformBufferAllocateInfo.allocationSize=UniformBufferMemorySizeWithPadding;
    UniformBufferAllocateInfo.memoryTypeIndex=findBestMemoryType(vkRuntimeInfoP,
                                                                 ~UniformBufferMemoryRequirements.memoryTypeBits,
                                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                                 NULL,
                                                                 UniformBufferMemorySizeWithPadding);
    if(vkAllocateMemory(vkRuntimeInfoP->device,&UniformBufferAllocateInfo,NULL,&(vkRuntimeInfoP->UniformBufferMemory))){
        dprintf(DBGT_ERROR,"Error could not allocate memory for uniform buffer, probably out of ram");
        exit(1);
    }

    //Bind the buffer handle to memory
    vkBindBufferMemory(vkRuntimeInfoP->device,vkRuntimeInfoP->UniformBufferHandle,vkRuntimeInfoP->UniformBufferMemory,0);


    //Calculate size requirements for staging buffer on cpu side
    //for{
    eng_ceatePINBufferAndCalcSize(vkRuntimeInfoP,&(ObjectToBeLoaded.stagingBuffers),VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    totalStagingPIUBufferSize+= ObjectToBeLoaded.stagingBuffers.TotalSizeWithPadding;
    //}

    //Create staging buffer on cpu side
    VkDeviceMemory TemporaryStagingBufferMem;
    VkMemoryAllocateInfo MemoryAllocateInfo={0};
    MemoryAllocateInfo.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    dprintf(DBGT_INFO,"Supported memory types for temporary staging buffer %x",ObjectToBeLoaded.stagingBuffers.PINBufferMemoryRequirements.memoryTypeBits);
    //TODO parse the return to check if we the memory for the device is directly host accessible
    int32_t bestMemType=findBestMemoryType(vkRuntimeInfoP,
                                           ~ObjectToBeLoaded.stagingBuffers.PINBufferMemoryRequirements.memoryTypeBits, //forbidden
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,                                         //required
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,                                         //uprank
                                           NULL,                                                                        //return
                                           ObjectToBeLoaded.stagingBuffers.PINBufferMemoryRequirements.size);           //size
    MemoryAllocateInfo.memoryTypeIndex=(uint32_t)bestMemType;
    MemoryAllocateInfo.allocationSize=totalStagingPIUBufferSize;
    if(vkAllocateMemory(vkRuntimeInfoP->device,&MemoryAllocateInfo,NULL,&TemporaryStagingBufferMem)){
        dprintf(DBGT_ERROR,"Could not allocate memory");
        exit(1);
    }
    //TODO check if gpu has unified memory

    ObjectToBeLoaded.deviceBuffers.PositionBufferSize=ObjectToBeLoaded.stagingBuffers.PositionBufferSize;
    ObjectToBeLoaded.deviceBuffers.IndexBufferSize=ObjectToBeLoaded.stagingBuffers.IndexBufferSize;
    ObjectToBeLoaded.deviceBuffers.NormalBufferSize=ObjectToBeLoaded.stagingBuffers.NormalBufferSize;

    //Calculate size requirements for device buffer on gpu side
    VkDeviceSize totalDevicePIUBufferSize=0;
    //for{
    eng_ceatePINBufferAndCalcSize(vkRuntimeInfoP,&(ObjectToBeLoaded.deviceBuffers),VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    totalDevicePIUBufferSize+=ObjectToBeLoaded.deviceBuffers.TotalSizeWithPadding;
    //}

    //Create device buffer on gpu side
    bestMemType=findBestMemoryType(vkRuntimeInfoP,
                                   ~ObjectToBeLoaded.deviceBuffers.PINBufferMemoryRequirements.memoryTypeBits,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   0,
                                   NULL,
                                   totalStagingPIUBufferSize);
    if(bestMemType<0){
        dprintf(DBGT_ERROR,"No memory on vram with the necessary size and attributes found");
        exit(1);
    }
    MemoryAllocateInfo.memoryTypeIndex=(uint32_t)bestMemType;
    MemoryAllocateInfo.allocationSize=totalDevicePIUBufferSize;
    if(vkAllocateMemory(vkRuntimeInfoP->device,&MemoryAllocateInfo,NULL,&(vkRuntimeInfoP->StaticPINBufferMemory))){
        dprintf(DBGT_ERROR,"Could not allocate memory");
        exit(1);
    }

    //Move data into cpu side buffer
    //Also setup binds for both cpu and gpu object buffers
    //for ObjectToBeLoaded{
        //Bind Memory To Buffers
        vkBindBufferMemory(vkRuntimeInfoP->device,ObjectToBeLoaded.stagingBuffers.PINBufferHandle,TemporaryStagingBufferMem,0);
        dprintf(DBGT_INFO,"Id of Staging Buffer %x",TemporaryStagingBufferMem);
        vkBindBufferMemory(vkRuntimeInfoP->device,ObjectToBeLoaded.deviceBuffers.PINBufferHandle,vkRuntimeInfoP->StaticPINBufferMemory,0);


        void* mappedMemoryP;
        //copy position data
        vkMapMemory(vkRuntimeInfoP->device,TemporaryStagingBufferMem,0,ObjectToBeLoaded.stagingBuffers.PositionBufferSize,0,&mappedMemoryP);
        for(uint32_t numberOfPosition=0;numberOfPosition<ObjectToBeLoaded.daeData.PositionsDlP->itemcnt/3;numberOfPosition++){
            ((float*)mappedMemoryP)[numberOfPosition*4+0]=((float*)ObjectToBeLoaded.daeData.PositionsDlP->items)[numberOfPosition*3+0];
            ((float*)mappedMemoryP)[numberOfPosition*4+1]=((float*)ObjectToBeLoaded.daeData.PositionsDlP->items)[numberOfPosition*3+1];
            ((float*)mappedMemoryP)[numberOfPosition*4+2]=((float*)ObjectToBeLoaded.daeData.PositionsDlP->items)[numberOfPosition*3+2];
            ((float*)mappedMemoryP)[numberOfPosition*4+3]=0.0f; //Vulkan requires Positions with vec3 to be padded to vec4
        }
        vkUnmapMemory(vkRuntimeInfoP->device,TemporaryStagingBufferMem);
        //copy combined index data
        vkMapMemory(vkRuntimeInfoP->device,TemporaryStagingBufferMem,ObjectToBeLoaded.stagingBuffers.PositionBufferSize,ObjectToBeLoaded.stagingBuffers.IndexBufferSize,0,&mappedMemoryP);
        for(uint32_t numberOfIndex=0;numberOfIndex<ObjectToBeLoaded.daeData.PositionsAndNormalIndicesDlP->itemcnt/2;numberOfIndex++){
            ((uint32_t*)mappedMemoryP)[numberOfIndex]=((uint64_t*)ObjectToBeLoaded.daeData.PositionsAndNormalIndicesDlP->items)[numberOfIndex*2+0];
        }
        vkUnmapMemory(vkRuntimeInfoP->device,TemporaryStagingBufferMem);
        //copy normal data
        vkMapMemory(vkRuntimeInfoP->device,TemporaryStagingBufferMem,ObjectToBeLoaded.stagingBuffers.PositionBufferSize+ObjectToBeLoaded.stagingBuffers.IndexBufferSize,ObjectToBeLoaded.stagingBuffers.NormalBufferSize,0,&mappedMemoryP);
        for(uint32_t numberOfNormal=0;numberOfNormal<ObjectToBeLoaded.daeData.NormalsDlP->itemcnt/3;numberOfNormal++){
            ((float*)mappedMemoryP)[numberOfNormal*4+0]=((float*)ObjectToBeLoaded.daeData.NormalsDlP->items)[numberOfNormal*3+0];
            ((float*)mappedMemoryP)[numberOfNormal*4+1]=((float*)ObjectToBeLoaded.daeData.NormalsDlP->items)[numberOfNormal*3+1];
            ((float*)mappedMemoryP)[numberOfNormal*4+2]=((float*)ObjectToBeLoaded.daeData.NormalsDlP->items)[numberOfNormal*3+2];
            ((float*)mappedMemoryP)[numberOfNormal*4+3]=0.0f; //Vulkan requires Normals with vec3 to be padded to vec4
        }
        vkUnmapMemory(vkRuntimeInfoP->device,TemporaryStagingBufferMem);


    //}

    //
    //schedule upload to gpu side
    //

    //create command buffer
    VkCommandBuffer UploadCommandBuffer;
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo={0};
    CommandBufferAllocateInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocateInfo.commandBufferCount=1;
    CommandBufferAllocateInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferAllocateInfo.commandPool=vkRuntimeInfoP->commandPool;
    vkAllocateCommandBuffers(vkRuntimeInfoP->device,&CommandBufferAllocateInfo,&UploadCommandBuffer);
    //start recording
    VkCommandBufferBeginInfo CommandBufferBeginInfo={0};
    CommandBufferBeginInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(UploadCommandBuffer,&CommandBufferBeginInfo);
    //copy command for every object
    VkBufferCopy copyRegion={0};


    //for ObjectToBeLoaded{
        //TODO check if the size of device local and cpu side buffer are equivalent
        copyRegion.size=ObjectToBeLoaded.deviceBuffers.IndexBufferSize+ObjectToBeLoaded.deviceBuffers.PositionBufferSize+ObjectToBeLoaded.deviceBuffers.NormalBufferSize;
        vkCmdCopyBuffer(UploadCommandBuffer,ObjectToBeLoaded.stagingBuffers.PINBufferHandle,ObjectToBeLoaded.deviceBuffers.PINBufferHandle,1,&copyRegion);
    //}
    //end recording
    vkEndCommandBuffer(UploadCommandBuffer);
    //submit to gpu with no synchronisation
    VkSubmitInfo SubmitInfo={0};
    SubmitInfo.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers=&UploadCommandBuffer;
    vkQueueSubmit(vkRuntimeInfoP->graphics_queue,1,&SubmitInfo,VK_NULL_HANDLE);
    vkQueueWaitIdle(vkRuntimeInfoP->graphics_queue);
    vkFreeCommandBuffers(vkRuntimeInfoP->device,vkRuntimeInfoP->commandPool,1,&UploadCommandBuffer);
    //for ObjectToBeLoaded{
    vkDestroyBuffer(vkRuntimeInfoP->device,ObjectToBeLoaded.stagingBuffers.PINBufferHandle,NULL);
    //end recording
    vkFreeMemory(vkRuntimeInfoP->device,TemporaryStagingBufferMem,NULL);


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
    CreateInfo.ppEnabledLayerNames=     (const char* const*)vkRuntimeInfoP->InstLayerNamesPP;

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
    DevCreateInfo.ppEnabledExtensionNames=  (const char* const*)vkRuntimeInfoP->DevExtensionNamesPP;
    DevCreateInfo.enabledLayerCount=        vkRuntimeInfoP->DevLayerCount;
    DevCreateInfo.ppEnabledLayerNames=      (const char* const*)vkRuntimeInfoP->DevLayerNamesPP;
    DevCreateInfo.pEnabledFeatures=         NULL;
    DevCreateInfo.flags=                    0;
    if(vkCreateDevice(vkRuntimeInfoP->physSelectedDevice,&DevCreateInfo,NULL,&(vkRuntimeInfoP->device))){
        dprintf(DBGT_ERROR,"Could not create Vulkan logical device");
        exit(1);
    }



    //Get handle for graphics queue
    vkGetDeviceQueue(vkRuntimeInfoP->device,vkRuntimeInfoP->graphics_queue_family_idx,0,&vkRuntimeInfoP->graphics_queue);


}

void eng_createCommandPool(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    VkCommandPoolCreateInfo CommandPoolInfo={0};
    CommandPoolInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolInfo.queueFamilyIndex=vkRuntimeInfoP->graphics_queue_family_idx;
    if(vkCreateCommandPool(vkRuntimeInfoP->device,&CommandPoolInfo,NULL,&(vkRuntimeInfoP->commandPool))){
        dprintf(DBGT_ERROR,"Could not create command pool");
        exit(1);
    }
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

    vkGetSwapchainImagesKHR(vkRuntimeInfoP->device,vkRuntimeInfoP->swapChain,&vkRuntimeInfoP->imagesInFlightCount,NULL);
    vkRuntimeInfoP->swapChainImagesP=(VkImage*)malloc(vkRuntimeInfoP->imagesInFlightCount*sizeof(VkImage));
    vkGetSwapchainImagesKHR(vkRuntimeInfoP->device,vkRuntimeInfoP->swapChain,&vkRuntimeInfoP->imagesInFlightCount,vkRuntimeInfoP->swapChainImagesP);
}

void eng_createImageViews(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    vkRuntimeInfoP->swapChainImageViewsP=(VkImageView*)malloc(vkRuntimeInfoP->imagesInFlightCount*sizeof(VkImageView));
    for(uint32_t imageIndex=0;imageIndex<vkRuntimeInfoP->imagesInFlightCount;imageIndex++){
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

void eng_createDescriptorPoolAndSets(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    //Create Descriptor Pool
    //It will hold one descriptor set for each frame in flight
    //to calculate the total pool size pPoolSizes will point to an array specifying the number for each descriptor type for the whole pool
    VkDescriptorPoolSize DescriptorPoolSize={0};
    DescriptorPoolSize.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    DescriptorPoolSize.descriptorCount=vkRuntimeInfoP->imagesInFlightCount;  //all descriptors of !ALL! sets in this pool can only total up to two

    VkDescriptorPoolCreateInfo DescriptorPoolInfo={0};
    DescriptorPoolInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    DescriptorPoolInfo.poolSizeCount=1;
    DescriptorPoolInfo.pPoolSizes=&DescriptorPoolSize;
    DescriptorPoolInfo.maxSets=vkRuntimeInfoP->imagesInFlightCount; //the descriptor pool can store two sets
    vkCreateDescriptorPool(vkRuntimeInfoP->device,&DescriptorPoolInfo,NULL,&(vkRuntimeInfoP->descriptorPool));

    VkDescriptorSetLayoutBinding LayoutBinding={0};
    LayoutBinding.stageFlags=VK_SHADER_STAGE_VERTEX_BIT;
    LayoutBinding.binding=1;    //for mvp in binding 1
    LayoutBinding.descriptorCount=1;
    LayoutBinding.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    //Create descriptor layouts and fill sets
    VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutInfo={0};
    DescriptorSetLayoutInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    DescriptorSetLayoutInfo.bindingCount=1;
    DescriptorSetLayoutInfo.pBindings=&LayoutBinding;
    vkRuntimeInfoP->descriptorSetLayoutsP=(VkDescriptorSetLayout*)malloc(vkRuntimeInfoP->imagesInFlightCount*sizeof(VkDescriptorSetLayout));
    for(uint32_t image=0;image<vkRuntimeInfoP->imagesInFlightCount;image++){
        vkCreateDescriptorSetLayout(vkRuntimeInfoP->device,&DescriptorSetLayoutInfo,NULL,&(vkRuntimeInfoP->descriptorSetLayoutsP[image]));
    }

    //Allocate descriptor set
    VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo={0};
    DescriptorSetAllocateInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DescriptorSetAllocateInfo.descriptorPool=vkRuntimeInfoP->descriptorPool;
    DescriptorSetAllocateInfo.descriptorSetCount=vkRuntimeInfoP->imagesInFlightCount; //one mvp matrix and hence one descriptor set per frame in flight
    DescriptorSetAllocateInfo.pSetLayouts=vkRuntimeInfoP->descriptorSetLayoutsP;
    vkRuntimeInfoP->descriptorSetsP=(VkDescriptorSet*)malloc(vkRuntimeInfoP->imagesInFlightCount*sizeof(VkDescriptorSet));
    vkAllocateDescriptorSets(vkRuntimeInfoP->device,&DescriptorSetAllocateInfo,vkRuntimeInfoP->descriptorSetsP);

    for(uint32_t image=0;image<vkRuntimeInfoP->imagesInFlightCount;image++){
        //Write descriptor set
        VkDescriptorBufferInfo BufferInfo;
        BufferInfo.buffer=vkRuntimeInfoP->UniformBufferHandle;
        BufferInfo.offset=sizeof(mat4x4)*image;
        BufferInfo.range=sizeof(mat4x4);
        VkWriteDescriptorSet WriteDescriptorSet={0};
        WriteDescriptorSet.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        WriteDescriptorSet.descriptorCount=1;
        WriteDescriptorSet.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        WriteDescriptorSet.dstBinding=1;
        WriteDescriptorSet.dstSet=vkRuntimeInfoP->descriptorSetsP[image];
        WriteDescriptorSet.pBufferInfo=&BufferInfo;
        vkUpdateDescriptorSets(vkRuntimeInfoP->device,1,&WriteDescriptorSet,0,NULL);
    }

}

void eng_createGraphicsPipeline(struct VulkanRuntimeInfo* vkRuntimeInfoP){

    VkVertexInputAttributeDescription InputAttributeDescriptionArray[2];
    InputAttributeDescriptionArray[0].location=0;   //will be used for positions vec3 (location=0) in shader
    InputAttributeDescriptionArray[0].binding=0;    //binding used to cross reference to the InputBindingDescription
    InputAttributeDescriptionArray[0].format=VK_FORMAT_R32G32B32_SFLOAT;    //is equivalent to vec3
    InputAttributeDescriptionArray[0].offset=0;     //positions are at the start of our static buffer

    VkVertexInputBindingDescription InputBindingDescriptionArray[2];
    InputBindingDescriptionArray[0].binding=0;  //binding used to cross reference to the InputAttributeDescription
    InputBindingDescriptionArray[0].inputRate=VK_VERTEX_INPUT_RATE_VERTEX;    //jump to next vertex for every new triangle in the index buffer, not every vertex
    InputBindingDescriptionArray[0].stride=sizeof(float)*4;                     //stride is sizeof(vec4)
    //VertexInput
    VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateInfo;
    PipelineVertexInputStateInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    PipelineVertexInputStateInfo.pNext=NULL;
    PipelineVertexInputStateInfo.flags=0;
    PipelineVertexInputStateInfo.vertexAttributeDescriptionCount=1;
    PipelineVertexInputStateInfo.vertexBindingDescriptionCount=1;
    //needs to be set if we supply vertex buffers to our shader
    PipelineVertexInputStateInfo.pVertexAttributeDescriptions=InputAttributeDescriptionArray;
    PipelineVertexInputStateInfo.pVertexBindingDescriptions=InputBindingDescriptionArray;

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
    VkPipelineLayoutCreateInfo PipelineLayoutInfo;
    PipelineLayoutInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutInfo.pNext=NULL;
    PipelineLayoutInfo.flags=0;
    PipelineLayoutInfo.setLayoutCount=2;
    PipelineLayoutInfo.pSetLayouts=vkRuntimeInfoP->descriptorSetsP;  //TODO errors here, needs to be initialized, check function order!!!
    PipelineLayoutInfo.pushConstantRangeCount=0;
    PipelineLayoutInfo.pPushConstantRanges=NULL;

    if(vkCreatePipelineLayout(vkRuntimeInfoP->device,&PipelineLayoutInfo,NULL,&(vkRuntimeInfoP->graphicsPipelineLayout))){
        dprintf(DBGT_ERROR,"Could not create pipeline layout");
        exit(1);
    }

    //Assemble everything
    VkPipelineShaderStageCreateInfo Stages[2]={
        VertexShaderStageCreateInfo,
        FragmentShaderStageCreateInfo
    };

    VkGraphicsPipelineCreateInfo PipelineInfo={0};
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

    PipelineInfo.layout=vkRuntimeInfoP->graphicsPipelineLayout;
    PipelineInfo.renderPass=vkRuntimeInfoP->renderPass;
    PipelineInfo.subpass=0;
    if(vkCreateGraphicsPipelines(vkRuntimeInfoP->device,VK_NULL_HANDLE,1,&PipelineInfo,NULL,&(vkRuntimeInfoP->graphicsPipeline))){
        dprintf(DBGT_ERROR,"Could not create graphics pipeline");
        exit(1);
    }
}

void eng_createFramebuffers(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    vkRuntimeInfoP->FramebufferP=malloc(sizeof(VkFramebuffer)*vkRuntimeInfoP->imagesInFlightCount);
    for(uint32_t framebufferIdx=0;framebufferIdx<vkRuntimeInfoP->imagesInFlightCount;framebufferIdx++){
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

void eng_createRenderCommandBuffers(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    vkRuntimeInfoP->CommandbufferP=(VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*vkRuntimeInfoP->imagesInFlightCount);

    VkCommandBufferAllocateInfo CommandBufferAllocateInfo={0};
    CommandBufferAllocateInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocateInfo.commandPool=vkRuntimeInfoP->commandPool;
    CommandBufferAllocateInfo.commandBufferCount=vkRuntimeInfoP->imagesInFlightCount;
    CommandBufferAllocateInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    if(vkAllocateCommandBuffers(vkRuntimeInfoP->device,&CommandBufferAllocateInfo,vkRuntimeInfoP->CommandbufferP)){
        dprintf(DBGT_ERROR,"Could not allocate command buffer");
        exit(1);
    }
    for(uint32_t CommandBufferIdx=0;CommandBufferIdx<vkRuntimeInfoP->imagesInFlightCount;CommandBufferIdx++){
        VkCommandBufferBeginInfo CommandBufferBeginInfo={0};
        CommandBufferBeginInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if(vkBeginCommandBuffer(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],&CommandBufferBeginInfo)){
            dprintf(DBGT_ERROR,"Could not begin recording to command buffer");
            exit(1);
        }

        VkClearValue clearColor={{{0.0f,0.0f,0.0f,1.0f}}};
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
        VkDeviceSize PIUBufferOffset=0;
        vkCmdBindVertexBuffers(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],CommandBufferIdx,1,&(ObjectToBeLoaded.deviceBuffers.PINBufferHandle),&PIUBufferOffset);
        vkCmdBindIndexBuffer(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],ObjectToBeLoaded.deviceBuffers.PINBufferHandle,ObjectToBeLoaded.deviceBuffers.PositionBufferSize,VK_INDEX_TYPE_UINT32);
        vkCmdBindPipeline(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],VK_PIPELINE_BIND_POINT_GRAPHICS,vkRuntimeInfoP->graphicsPipeline);
        vkCmdBindDescriptorSets(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],VK_PIPELINE_BIND_POINT_GRAPHICS,vkRuntimeInfoP->graphicsPipelineLayout,0,1,vkRuntimeInfoP->descriptorSetsP,0,NULL);
        vkCmdDrawIndexed(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],(ObjectToBeLoaded.deviceBuffers.IndexBufferSize)/sizeof(uint32_t),1,0,0,0);
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
    vkRuntimeInfoP->imageAvailableSemaphoreP=(VkSemaphore*)malloc(sizeof(VkSemaphore)*vkRuntimeInfoP->imagesInFlightCount);
    vkRuntimeInfoP->renderFinishedSemaphoreP=(VkSemaphore*)malloc(sizeof(VkSemaphore)*vkRuntimeInfoP->imagesInFlightCount);
    vkRuntimeInfoP->ImageAlreadyProcessingFenceP=(VkFence*)malloc(sizeof(VkFence)*vkRuntimeInfoP->imagesInFlightCount);

    for(uint32_t FrameIdx=0;FrameIdx<vkRuntimeInfoP->imagesInFlightCount;FrameIdx++){
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

void error_callback(int code,const char* description){
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

    //create new MVP
    //Model
    mat4x4 MMatrix;
    mat4x4 MVPMatrix;
    mat4x4_identity(MMatrix);
    mat4x4_rotate_X(MVPMatrix,MMatrix,0.3f);
    //View
    mat4x4 VMatrix;
    vec3 eye={0.0f,0.0f,2.0f};
    vec3 center={0.0f,0.0f,0.0f};
    vec3 up={0.0f,0.0f,1.0f};
    mat4x4_look_at(VMatrix,eye,center,up);
    mat4x4_mul(MVPMatrix,VMatrix,MVPMatrix);
    //projection
    mat4x4 PMatrix;
    float aspRatio=((float)vkRuntimeInfoP->swapChainImageExtent.width)/vkRuntimeInfoP->swapChainImageExtent.height;
    mat4x4_perspective(PMatrix,1.6f,aspRatio,0.1f,10.0f);
    mat4x4_mul(MVPMatrix,PMatrix,MVPMatrix);
    //copy in buffer
    void* mappedUniformSliceP;
    vkMapMemory(vkRuntimeInfoP->device,vkRuntimeInfoP->UniformBufferMemory,sizeof(mat4x4)*nextImageIdx,sizeof(mat4x4),0,&mappedUniformSliceP);
    memcpy(mappedUniformSliceP,&MVPMatrix,sizeof(mat4x4));
    vkUnmapMemory(vkRuntimeInfoP->device,vkRuntimeInfoP->UniformBufferMemory);

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
    nextImageIdx%=vkRuntimeInfoP->imagesInFlightCount;


}

int main(int argc, char** argv){
    (void)argc; //avoid unused arguments warning
    (void)argv;

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
    eng_createImageViews(&engVkRuntimeInfo);                                //depends on eng_createSwapChain
    eng_createShaderModule(&engVkRuntimeInfo,"./res/shader1.xml");          //depends on eng_createDevice
    eng_createRenderPass(&engVkRuntimeInfo);                                //depends on eng_createSwapChain
    eng_createGraphicsPipeline(&engVkRuntimeInfo);                          //depends on eng_createShaderModule and eng_createImageViews
    eng_createFramebuffers(&engVkRuntimeInfo);                              //depends on eng_createRenderPass   and eng_createImageViews
    eng_createCommandPool(&engVkRuntimeInfo);                               //depends on eng_createDevice
    eng_load_static_models(&engVkRuntimeInfo);                              //depends on eng_createCommandPool and creates vertex buffer
    eng_createDescriptorPoolAndSets(&engVkRuntimeInfo);                     //depends on
    eng_createRenderCommandBuffers(&engVkRuntimeInfo);                      //depends on eng_createCommandPool and eng_createFramebuffers and eng_load_static_models and createPipeline
    eng_createSynchronizationPrimitives(&engVkRuntimeInfo);
    dprintf(DBGT_INFO,"Got here");
    while (!glfwWindowShouldClose(mainWindowP)) {
        eng_draw(&engVkRuntimeInfo);
        glfwPollEvents();
    }
}
