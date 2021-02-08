#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define GLFW_INCLUDE_VULKAN
#include "vulkan/vulkan.h"
#include "shaderc/shaderc.h"

#include "glfw/glfw3.h"
#include "mathHelper/mathHelper.h"      //for countBitsInUint32
#include "linmath/linmath.h"
#include <debugPrint/debugPrint.h>
#include "xmlReader/xmlReader.h"
#include "daeLoader/daeLoader.h"

#define CHK_VK(function)                                                \
do {                                                                    \
    uint32_t errorcode=function;                                        \
    if(errorcode){                                                      \
        printf("Error code %d occured in line %d\n",errorcode,__LINE__);\
        exit(1);                                                        \
    }                                                                   \
} while (0)

#define CHK_VK_ACK(function,action)                                     \
do {                                                                    \
    uint32_t errorcode=function;                                        \
    if(errorcode){                                                      \
        printf("Error code %d occured in line %d\n",errorcode,__LINE__);\
        action;                                                         \
    }                                                                   \
} while (0)

uint32_t eng_get_version_number_from_xmlemnt(struct xmlTreeElement* currentReqXmlP);
uint32_t eng_get_version_number_from_UTF32DynlistP(struct DynamicList* inputStringP);
struct xmlTreeElement* eng_get_eng_setupxml(char* FilePath,int debug_enabled);






struct engBufferHandle{
    //this field must be filled before buffer handle creation
    VkDeviceSize            ContentSizeInBytes;
    //the fields below are while creating the buffer handle
    VkBuffer                BufferHandle;
    VkMemoryRequirements    MemoryRequirements;
    //TODO
    VkMemoryPropertyFlags   MemoryFlags;
    //the fields below are only filled while allocating binding the buffer to memory
    VkDeviceSize            OffsetInMemoryInBytes;
    VkDeviceMemory          Memory;
};

struct eng3dObject{
    struct DataFromDae      daeData;
    struct engBufferHandle* writeVertexBufferP;
    struct engBufferHandle* readVertexBufferP;  //readVertexBufferP can be the same pointer as writeVertexBufferP for unified memory
    VkDeviceSize PosNormUvInBufOffset;
    VkDeviceSize IdxInBufOffset;
    uint32_t vertexCount;
};



struct _engExtensionsAndLayers{
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

    VkInstance instance;
    struct _engExtensionsAndLayers;

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

    struct engBufferHandle FastUpdatingUniformBuffer;
    struct engBufferHandle DeviceVertexBuffer;
};

VkCommandBuffer _eng_cmdBuf_startSingleUse(struct VulkanRuntimeInfo* vkRuntimeInfoP);
void _eng_cmdBuf_endAndSubmitSingleUse(struct VulkanRuntimeInfo* vkRuntimeInfoP,VkCommandBuffer SingleUseBufferP);

struct eng3dObject ObjectToBeLoaded;

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
    CHK_VK(vkCreateShaderModule(vkRuntimeInfoP->device,&ShaderModuleCreateInfo,NULL,&(vkRuntimeInfoP->VertexShaderModule)));
    //Recycle ShaderModuleCreateInfo

    ShaderModuleCreateInfo.pCode=(uint32_t*)shaderc_result_get_bytes(compResultFragment);
    ShaderModuleCreateInfo.codeSize=shaderc_result_get_length(compResultFragment);
    CHK_VK(vkCreateShaderModule(vkRuntimeInfoP->device,&ShaderModuleCreateInfo,NULL,&(vkRuntimeInfoP->FragmentShaderModule)));

    //shaderc_result_release(compResultVertex);
    //shaderc_result_release(compResultFragment);
}





//ToDo do actual memory usage calculations for each heap
int32_t _eng_Memory_findBestType(struct VulkanRuntimeInfo* vkRuntimeInfoP,VkMemoryPropertyFlags forbiddenBitfield,VkMemoryPropertyFlags requiredBitfield, VkMemoryPropertyFlags uprankBitfield, VkMemoryPropertyFlags downrankBitfield,VkMemoryPropertyFlags* ReturnBitfieldP, VkDeviceSize minsize){
    //Get information about all available memory types
    VkPhysicalDeviceMemoryProperties DeviceMemProperties;
    vkGetPhysicalDeviceMemoryProperties(vkRuntimeInfoP->physSelectedDevice,&DeviceMemProperties);
    int32_t bestRankingMemoryTypeIdx=-1;
    uint32_t bestRanking=0;
    for(uint32_t MemoryTypeIdx=0;MemoryTypeIdx<DeviceMemProperties.memoryTypeCount;MemoryTypeIdx++){
        uint32_t currentRank=1+sizeof(uint32_t)*4;
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
        currentRank-=countBitsInUint32(DeviceMemProperties.memoryTypes[MemoryTypeIdx].propertyFlags&downrankBitfield);
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

void _eng_VertexBuffer_createHandle(struct VulkanRuntimeInfo* vkRuntimeInfoP, struct engBufferHandle* BufferWithSpecifiedSizeP,uint32_t srcOrDstBit){
    //Create handle for new uniform buffer
    VkBufferCreateInfo UniformBufferCreateInfo={0};
    UniformBufferCreateInfo.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    UniformBufferCreateInfo.queueFamilyIndexCount=1;
    UniformBufferCreateInfo.pQueueFamilyIndices=&(vkRuntimeInfoP->graphics_queue_family_idx);
    UniformBufferCreateInfo.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    UniformBufferCreateInfo.usage=  VK_BUFFER_USAGE_INDEX_BUFFER_BIT|
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|
                                    srcOrDstBit;
    UniformBufferCreateInfo.size=BufferWithSpecifiedSizeP->ContentSizeInBytes;

    CHK_VK(vkCreateBuffer(vkRuntimeInfoP->device,&UniformBufferCreateInfo,NULL,&(BufferWithSpecifiedSizeP->BufferHandle)));

    vkGetBufferMemoryRequirements(vkRuntimeInfoP->device,BufferWithSpecifiedSizeP->BufferHandle,&(BufferWithSpecifiedSizeP->MemoryRequirements));
}

void _eng_Memory_allocate(struct VulkanRuntimeInfo* vkRuntimeInfoP,VkDeviceMemory* VertexDeviceMemoryP,VkDeviceSize VertexMemoryAllocationSize,uint32_t MemoryTypeIdx){
    //Allocate Memory for host local buffer
    VkMemoryAllocateInfo UniformBufferAllocateInfo={0};
    UniformBufferAllocateInfo.sType             =VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    UniformBufferAllocateInfo.allocationSize    =VertexMemoryAllocationSize;
    UniformBufferAllocateInfo.memoryTypeIndex   =MemoryTypeIdx;
    CHK_VK(vkAllocateMemory(vkRuntimeInfoP->device,&UniformBufferAllocateInfo,NULL,VertexDeviceMemoryP));
}

void _eng_DynamicUnifBuf_allocateAndBind(struct VulkanRuntimeInfo* vkRuntimeInfoP, struct engBufferHandle* BufferWithSpecifiedSizeP){
    //Create handle for new uniform buffer
    VkBufferCreateInfo UniformBufferCreateInfo={0};
    UniformBufferCreateInfo.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    UniformBufferCreateInfo.queueFamilyIndexCount=1;
    UniformBufferCreateInfo.pQueueFamilyIndices=&(vkRuntimeInfoP->graphics_queue_family_idx);
    UniformBufferCreateInfo.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    UniformBufferCreateInfo.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    UniformBufferCreateInfo.size=BufferWithSpecifiedSizeP->ContentSizeInBytes;

    CHK_VK(vkCreateBuffer(vkRuntimeInfoP->device,&UniformBufferCreateInfo,NULL,&(BufferWithSpecifiedSizeP->BufferHandle)));

    //Get memory requirements
    VkMemoryRequirements UniformBufferMemoryRequirements;
    vkGetBufferMemoryRequirements(vkRuntimeInfoP->device,BufferWithSpecifiedSizeP->BufferHandle,&UniformBufferMemoryRequirements);
    VkDeviceSize UniformBufferMemorySizeWithPadding=0;
    UniformBufferMemorySizeWithPadding=UniformBufferMemoryRequirements.size;
    if(UniformBufferMemoryRequirements.size%UniformBufferMemoryRequirements.alignment){ //End of buffer is not aligned, so pad size so that it is
        UniformBufferMemorySizeWithPadding/=UniformBufferMemoryRequirements.alignment;
        UniformBufferMemorySizeWithPadding++;
        UniformBufferMemorySizeWithPadding*=UniformBufferMemoryRequirements.alignment;
    }

    uint32_t bestMemoryTypeIdx=_eng_Memory_findBestType(vkRuntimeInfoP,
                                ~UniformBufferMemoryRequirements.memoryTypeBits,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, //Memory needs to be Host visible
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                                &(BufferWithSpecifiedSizeP->MemoryFlags),
                                UniformBufferMemorySizeWithPadding);

    //Allocate Memory for unifrom buffer
    VkMemoryAllocateInfo UniformBufferAllocateInfo={0};
    UniformBufferAllocateInfo.sType             =VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    UniformBufferAllocateInfo.allocationSize    =UniformBufferMemorySizeWithPadding;
    UniformBufferAllocateInfo.memoryTypeIndex   =bestMemoryTypeIdx;
    CHK_VK(vkAllocateMemory(vkRuntimeInfoP->device,&UniformBufferAllocateInfo,NULL,&(BufferWithSpecifiedSizeP->Memory)));

    //Bind the buffer handle to memory
    CHK_VK(vkBindBufferMemory(vkRuntimeInfoP->device,BufferWithSpecifiedSizeP->BufferHandle,BufferWithSpecifiedSizeP->Memory,0));
}

void eng_load_static_models(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    //struct eng3dObject ObjectToBeLoaded;    TODO remove this global hack
    VkDeviceSize RoughMemoryEstimate=0;
    for(int ObjectNum=0;ObjectNum<1;ObjectNum++){
        daeLoader_load("./res/untitled.dae","Cube-mesh",&(ObjectToBeLoaded.daeData));
        ObjectToBeLoaded.readVertexBufferP=(struct engBufferHandle*)malloc(sizeof(struct engBufferHandle));
        ObjectToBeLoaded.PosNormUvInBufOffset=0;
        ObjectToBeLoaded.IdxInBufOffset=ObjectToBeLoaded.daeData.CombinedPsNrUvDlP->itemcnt*sizeof(float);

        ObjectToBeLoaded.readVertexBufferP->ContentSizeInBytes=((ObjectToBeLoaded.daeData.CombinedPsNrUvDlP->itemcnt*sizeof(float))
                                                                +(ObjectToBeLoaded.daeData.IndexingDlP->itemcnt*sizeof(uint32_t)));
        _eng_VertexBuffer_createHandle(vkRuntimeInfoP,ObjectToBeLoaded.readVertexBufferP,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        RoughMemoryEstimate+=ObjectToBeLoaded.readVertexBufferP->MemoryRequirements.size+ObjectToBeLoaded.readVertexBufferP->MemoryRequirements.alignment*16;
    }

    //Check for unified memory
    uint32_t resultMemoryBits;
    uint32_t DeviceLocalMemoryType=_eng_Memory_findBestType(vkRuntimeInfoP,~(ObjectToBeLoaded.readVertexBufferP->MemoryRequirements.memoryTypeBits),
                                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                            0,
                                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,   //favor non host visible memory
                                                            &(resultMemoryBits),
                                                            RoughMemoryEstimate);
    int unifiedMemoryFlag=(resultMemoryBits&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    for(int ObjectNum=0;ObjectNum<1;ObjectNum++){
        if(unifiedMemoryFlag){
            ObjectToBeLoaded.writeVertexBufferP=ObjectToBeLoaded.readVertexBufferP;
        }else{
            ObjectToBeLoaded.writeVertexBufferP=(struct engBufferHandle*)malloc(sizeof(struct engBufferHandle));
            ObjectToBeLoaded.writeVertexBufferP->ContentSizeInBytes=ObjectToBeLoaded.readVertexBufferP->ContentSizeInBytes;
            _eng_VertexBuffer_createHandle(vkRuntimeInfoP,ObjectToBeLoaded.writeVertexBufferP,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        }
    }

    //get maximum alignment required
    VkDeviceSize MaxAlignment=max_uint32_t(ObjectToBeLoaded.writeVertexBufferP->MemoryRequirements.alignment,
                                           ObjectToBeLoaded.readVertexBufferP->MemoryRequirements.alignment);

    //Calculate the required size and handle alignment
    VkDeviceSize TotalAllocationSize=0;
    for(int ObjectNum=0;ObjectNum<1;ObjectNum++){
        //Align Start of Buffer
        VkDeviceSize LastBufferAlignmentOvershoot=TotalAllocationSize%MaxAlignment;
        if(LastBufferAlignmentOvershoot){
            TotalAllocationSize+=(MaxAlignment-LastBufferAlignmentOvershoot);
        }
        ObjectToBeLoaded.writeVertexBufferP->OffsetInMemoryInBytes=TotalAllocationSize;
        ObjectToBeLoaded.readVertexBufferP->OffsetInMemoryInBytes=TotalAllocationSize;
        //Account for buffer length
        TotalAllocationSize+=max_uint32_t(ObjectToBeLoaded.writeVertexBufferP->MemoryRequirements.size,
                                          ObjectToBeLoaded.readVertexBufferP->MemoryRequirements.size);
    }

    //Get device side memory
    VkDeviceMemory VertexDeviceMemory;
    VkDeviceMemory VertexHostMemory;
    _eng_Memory_allocate(vkRuntimeInfoP,&VertexDeviceMemory,TotalAllocationSize,DeviceLocalMemoryType);
    if(!unifiedMemoryFlag){
        uint32_t HostLocalMemoryType=_eng_Memory_findBestType(vkRuntimeInfoP,
                                                                ~ObjectToBeLoaded.writeVertexBufferP->MemoryRequirements.memoryTypeBits,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                                0,
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                NULL,
                                                                TotalAllocationSize);
        _eng_Memory_allocate(vkRuntimeInfoP,&VertexHostMemory,TotalAllocationSize,HostLocalMemoryType);
    }

    //Bind Buffers
    for(int ObjectNum=0;ObjectNum<1;ObjectNum++){

        ObjectToBeLoaded.readVertexBufferP->Memory=VertexDeviceMemory;
        CHK_VK(vkBindBufferMemory(vkRuntimeInfoP->device,ObjectToBeLoaded.readVertexBufferP->BufferHandle,
                                                         ObjectToBeLoaded.readVertexBufferP->Memory,
                                                         ObjectToBeLoaded.readVertexBufferP->OffsetInMemoryInBytes));
        if(!unifiedMemoryFlag){
            ObjectToBeLoaded.writeVertexBufferP->Memory=VertexHostMemory;
            CHK_VK(vkBindBufferMemory(vkRuntimeInfoP->device,ObjectToBeLoaded.writeVertexBufferP->BufferHandle,
                                                             ObjectToBeLoaded.writeVertexBufferP->Memory,
                                                             ObjectToBeLoaded.writeVertexBufferP->OffsetInMemoryInBytes));
        }
    }


    //Setup Uniform Buffer
    //ToDo support per object mvp matrices
    vkRuntimeInfoP->FastUpdatingUniformBuffer.ContentSizeInBytes=sizeof(mat4x4)*vkRuntimeInfoP->imagesInFlightCount;
    _eng_DynamicUnifBuf_allocateAndBind(vkRuntimeInfoP,&(vkRuntimeInfoP->FastUpdatingUniformBuffer));


    //Move data into write buffer
    for(int ObjectNum=0;ObjectNum<1;ObjectNum++){
        //Bind Memory To Buffers
        void* mappedMemoryP;
        //map object buffer
        CHK_VK(vkMapMemory(vkRuntimeInfoP->device,ObjectToBeLoaded.writeVertexBufferP->Memory,ObjectToBeLoaded.writeVertexBufferP->OffsetInMemoryInBytes,ObjectToBeLoaded.writeVertexBufferP->ContentSizeInBytes,0,&mappedMemoryP));
        //copy position,normal,uv data
        memcpy(((char*)mappedMemoryP)+ObjectToBeLoaded.PosNormUvInBufOffset,ObjectToBeLoaded.daeData.CombinedPsNrUvDlP->items,ObjectToBeLoaded.daeData.CombinedPsNrUvDlP->itemcnt*sizeof(float));
        //copy combined index
        memcpy(((char*)mappedMemoryP)+ObjectToBeLoaded.IdxInBufOffset,ObjectToBeLoaded.daeData.IndexingDlP->items,ObjectToBeLoaded.daeData.IndexingDlP->itemcnt*sizeof(uint32_t));
        vkUnmapMemory(vkRuntimeInfoP->device,ObjectToBeLoaded.writeVertexBufferP->Memory);
        DlDelete(ObjectToBeLoaded.daeData.CombinedPsNrUvDlP);
        ObjectToBeLoaded.vertexCount=ObjectToBeLoaded.daeData.IndexingDlP->itemcnt;
        DlDelete(ObjectToBeLoaded.daeData.IndexingDlP);
        //copy normal data
    }

    //schedule upload to gpu side
    VkCommandBuffer UploadCommandBuffer=_eng_cmdBuf_startSingleUse(vkRuntimeInfoP);
    //copy command for every object
    VkBufferCopy copyRegion={0};
    for(int ObjectNum=0;ObjectNum<1;ObjectNum++){
        copyRegion.size=ObjectToBeLoaded.writeVertexBufferP->MemoryRequirements.size;
        vkCmdCopyBuffer(UploadCommandBuffer,ObjectToBeLoaded.writeVertexBufferP->BufferHandle,ObjectToBeLoaded.readVertexBufferP->BufferHandle,1,&copyRegion);
    }
    //end recording
    _eng_cmdBuf_endAndSubmitSingleUse(vkRuntimeInfoP,UploadCommandBuffer);

    //Cleanup
    if(!unifiedMemoryFlag){
        for(int ObjectNum=0;ObjectNum<1;ObjectNum++){
            vkDestroyBuffer(vkRuntimeInfoP->device,ObjectToBeLoaded.writeVertexBufferP->BufferHandle,NULL);
        }
        vkFreeMemory(vkRuntimeInfoP->device,VertexHostMemory,NULL);
    }
}

VkCommandBuffer _eng_cmdBuf_startSingleUse(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    //create command buffer
    VkCommandBuffer UploadCommandBuffer;
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo={0};
    CommandBufferAllocateInfo.sType             =VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocateInfo.commandBufferCount=1;
    CommandBufferAllocateInfo.level             =VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferAllocateInfo.commandPool       =vkRuntimeInfoP->commandPool;
    CHK_VK(vkAllocateCommandBuffers(vkRuntimeInfoP->device,&CommandBufferAllocateInfo,&UploadCommandBuffer));
    //start recording
    VkCommandBufferBeginInfo CommandBufferBeginInfo={0};
    CommandBufferBeginInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CHK_VK(vkBeginCommandBuffer(UploadCommandBuffer,&CommandBufferBeginInfo));
    return UploadCommandBuffer;
}

void _eng_cmdBuf_endAndSubmitSingleUse(struct VulkanRuntimeInfo* vkRuntimeInfoP,VkCommandBuffer SingleUseBufferP){
    //end recording
    vkEndCommandBuffer(SingleUseBufferP);
    //submit to gpu with no synchronisation
    VkSubmitInfo SubmitInfo={0};
    SubmitInfo.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers=&SingleUseBufferP;
    CHK_VK(vkQueueSubmit(vkRuntimeInfoP->graphics_queue,1,&SubmitInfo,VK_NULL_HANDLE));
    CHK_VK(vkQueueWaitIdle(vkRuntimeInfoP->graphics_queue));
    vkFreeCommandBuffers(vkRuntimeInfoP->device,vkRuntimeInfoP->commandPool,1,&SingleUseBufferP);
}

void eng_createInstance(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct xmlTreeElement* eng_setupxmlP){
    //get required information from xml object in memory
    //engine and app name
    struct xmlTreeElement* engNameXmlElmntP=getFirstSubelementWithASCII(eng_setupxmlP,"EngineName",NULL,NULL,0,0);
    struct xmlTreeElement* engNameContentXmlElmntP=getFirstSubelementWith(engNameXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0);
    engNameContentXmlElmntP->content=Dl_utf32_StripSpaces_freeArg1(engNameContentXmlElmntP->content);
    char* engNameCharP=Dl_utf32_toString(engNameContentXmlElmntP->content);

    struct xmlTreeElement* appNameXmlElmntP=getFirstSubelementWithASCII(eng_setupxmlP,"ApplicationName",NULL,NULL,0,0);
    struct xmlTreeElement* appNameContentXmlElmntP=getFirstSubelementWith(appNameXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0);
    appNameContentXmlElmntP->content=Dl_utf32_StripSpaces_freeArg1(appNameContentXmlElmntP->content);
    char* appNameCharP=Dl_utf32_toString(appNameContentXmlElmntP->content);

    //engine and app version
    struct xmlTreeElement* engVersionXmlElmntP=getFirstSubelementWithASCII(eng_setupxmlP,"EngineVersion",NULL,NULL,0,0);
    struct xmlTreeElement* engVersionContentXmlElmntP=getFirstSubelementWith(engVersionXmlElmntP,NULL,NULL,NULL,xmltype_chardata,0);
    engVersionContentXmlElmntP->content=Dl_utf32_StripSpaces_freeArg1(engVersionContentXmlElmntP->content);
    uint32_t engVersion=eng_get_version_number_from_UTF32DynlistP(engVersionContentXmlElmntP->content);

    struct xmlTreeElement* appVersionXmlElmntP=getFirstSubelementWithASCII(eng_setupxmlP,"ApplicationVersion",NULL,NULL,0,0);
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
    struct xmlTreeElement* reqInstLayerXmlElmntP=getFirstSubelementWithASCII(eng_setupxmlP,"RequiredInstanceLayers",NULL,NULL,0,0);
    struct DynamicList* reqInstLayerDynlistP=getAllSubelementsWithASCII(reqInstLayerXmlElmntP,"Layer",NULL,NULL,0,0);
    struct xmlTreeElement* reqInstExtensionXmlElmntP=getFirstSubelementWithASCII(eng_setupxmlP,"RequiredInstanceExtensions",NULL,NULL,0,0);
    struct DynamicList* reqInstExtensionDynlistP=getAllSubelementsWithASCII(reqInstExtensionXmlElmntP,"Extension",NULL,NULL,0,0);

    //Check layer support
    uint32_t layerCount=0;
    vkEnumerateInstanceLayerProperties(&layerCount,NULL);
    VkLayerProperties* LayerProptertiesP=(VkLayerProperties*)malloc(layerCount*sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layerCount,LayerProptertiesP);


    for(unsigned int required_layer_idx=0;required_layer_idx<reqInstLayerDynlistP->itemcnt;required_layer_idx++){
        unsigned int available_layer_idx;
        struct xmlTreeElement* currentLayerXmlElmntP=((struct xmlTreeElement**)(reqInstLayerDynlistP->items))[required_layer_idx];
        struct DynamicList* reqLayerNameDynlistP=getValueFromKeyNameASCII(currentLayerXmlElmntP->attributes,"name");
        //Dl_utf32_print(reqLayerNameDynlistP);
        char* reqLayerNameCharP=Dl_utf32_toString(reqLayerNameDynlistP);
        //printf("%s",reqLayerNameCharP);
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
        char* reqExtensionNameCharP=Dl_utf32_toString(getValueFromKeyNameASCII(currentExtensionXmlElmntP->attributes,"name"));
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
        vkRuntimeInfoP->InstExtensionNamesPP[InstExtensionIdx]=Dl_utf32_toString(getValueFromKeyNameASCII(currentExtensionXmlElmntP->attributes,"name"));
        dprintf(DBGT_INFO,"Requesting inst extension %s",vkRuntimeInfoP->InstExtensionNamesPP[InstExtensionIdx]);
    }

    //generate InstLayerNames and Count
    vkRuntimeInfoP->InstLayerCount=reqInstLayerDynlistP->itemcnt;
    vkRuntimeInfoP->InstLayerNamesPP=(char**)malloc(vkRuntimeInfoP->InstLayerCount*sizeof(char*));
    for(uint32_t InstLayerIdx=0;InstLayerIdx<vkRuntimeInfoP->InstLayerCount;InstLayerIdx++){
        struct xmlTreeElement* currentExtensionXmlElmntP=((struct xmlTreeElement**)(reqInstLayerDynlistP->items))[InstLayerIdx];
        vkRuntimeInfoP->InstLayerNamesPP[InstLayerIdx]=Dl_utf32_toString(getValueFromKeyNameASCII(currentExtensionXmlElmntP->attributes,"name"));
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

    CHK_VK(vkCreateInstance(&CreateInfo,NULL,&(vkRuntimeInfoP->instance)));
}

uint8_t* eng_vulkan_generate_device_ranking(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct xmlTreeElement* eng_setupxmlP){

    struct xmlTreeElement* reqDevLayerXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,Dl_utf32_fromString("RequiredDeviceLayers"),NULL,NULL,0,1);
    struct DynamicList* reqDevLayerDynlistP=getAllSubelementsWith_freeArg234(reqDevLayerXmlElmntP,Dl_utf32_fromString("Layer"),NULL,NULL,0,1);
    struct xmlTreeElement* reqDevExtensionXmlElmntP=getFirstSubelementWith_freeArg234(eng_setupxmlP,Dl_utf32_fromString("RequiredDeviceExtensions"),NULL,NULL,0,1);
    //printXMLsubelements(reqDevExtensionXmlElmntP);
    struct DynamicList* reqDevExtensionDynlistP=getAllSubelementsWith_freeArg234(reqDevExtensionXmlElmntP,Dl_utf32_fromString("Extension"),NULL,NULL,0,1);

    //get all vulkan devices
    CHK_VK(vkEnumeratePhysicalDevices(vkRuntimeInfoP->instance,&vkRuntimeInfoP->physDeviceCount,NULL));
    vkRuntimeInfoP->physAvailDevicesP=(VkPhysicalDevice*)malloc(vkRuntimeInfoP->physDeviceCount*sizeof(VkPhysicalDevice));
    CHK_VK(vkEnumeratePhysicalDevices(vkRuntimeInfoP->instance,&vkRuntimeInfoP->physDeviceCount,vkRuntimeInfoP->physAvailDevicesP));

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
        CHK_VK(vkEnumerateDeviceLayerProperties(vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],&layerCount,NULL));
        VkLayerProperties* LayerProptertiesP=(VkLayerProperties*)malloc(layerCount*sizeof(VkLayerProperties));
        CHK_VK(vkEnumerateDeviceLayerProperties(vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],&layerCount,LayerProptertiesP));

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
        CHK_VK(vkEnumerateDeviceExtensionProperties(vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],NULL,&extensionCount,NULL));
        VkExtensionProperties* ExtensionProptertiesP=(VkExtensionProperties*)malloc(extensionCount*sizeof(VkExtensionProperties));
        CHK_VK(vkEnumerateDeviceExtensionProperties(vkRuntimeInfoP->physAvailDevicesP[physicalDevicesIdx],NULL,&extensionCount,ExtensionProptertiesP));

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
        //printXMLsubelements(currentExtensionXmlElmntP);
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
    CHK_VK(vkCreateDevice(vkRuntimeInfoP->physSelectedDevice,&DevCreateInfo,NULL,&(vkRuntimeInfoP->device)));

    //Get handle for graphics queue
    vkGetDeviceQueue(vkRuntimeInfoP->device,vkRuntimeInfoP->graphics_queue_family_idx,0,&vkRuntimeInfoP->graphics_queue);
}

void eng_createCommandPool(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    VkCommandPoolCreateInfo CommandPoolInfo={0};
    CommandPoolInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolInfo.queueFamilyIndex=vkRuntimeInfoP->graphics_queue_family_idx;
    CHK_VK(vkCreateCommandPool(vkRuntimeInfoP->device,&CommandPoolInfo,NULL,&(vkRuntimeInfoP->commandPool)));
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
    //printXMLsubelements(engSetupDebOrRelP);
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
    struct DynamicList* minversionUTF32DynlistP=getValueFromKeyNameASCII(currentReqLayerAttribP,"minversion");
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
    CHK_VK(glfwCreateWindowSurface(vkRuntimeInfoP->instance,glfwWindowP,NULL,&vkRuntimeInfoP->surface));
}

void eng_createSwapChain(struct VulkanRuntimeInfo* vkRuntimeInfoP,GLFWwindow* glfwWindowP){
    VkBool32 surfaceSupport=VK_TRUE;
    CHK_VK(vkGetPhysicalDeviceSurfaceSupportKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->graphics_queue_family_idx,vkRuntimeInfoP->surface,&surfaceSupport));
    CHK_VK(surfaceSupport!=VK_TRUE);

    //get basic surface capabilitiers
    struct VkSurfaceCapabilitiesKHR surfaceCapabilities;
    CHK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->surface,&surfaceCapabilities));
    //get supported formats
    uint32_t formatCount=0;
    CHK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->surface,&formatCount,NULL));
    VkSurfaceFormatKHR* SurfaceFormatsP=(VkSurfaceFormatKHR*)malloc(formatCount*sizeof(VkSurfaceFormatKHR));
    CHK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->surface,&formatCount,SurfaceFormatsP));
    //get supported present modes
    uint32_t presentModeCount=0;
    CHK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->surface,&presentModeCount,NULL));
    VkPresentModeKHR* PresentModeP=(VkPresentModeKHR*)malloc(presentModeCount*sizeof(VkPresentModeKHR));
    CHK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(vkRuntimeInfoP->physSelectedDevice,vkRuntimeInfoP->surface,&presentModeCount,PresentModeP));

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

    CHK_VK(vkCreateSwapchainKHR(vkRuntimeInfoP->device,&swapchainCreateInfo,NULL,&vkRuntimeInfoP->swapChain));
    CHK_VK(vkGetSwapchainImagesKHR(vkRuntimeInfoP->device,vkRuntimeInfoP->swapChain,&vkRuntimeInfoP->imagesInFlightCount,NULL));
    vkRuntimeInfoP->swapChainImagesP=(VkImage*)malloc(vkRuntimeInfoP->imagesInFlightCount*sizeof(VkImage));
    CHK_VK(vkGetSwapchainImagesKHR(vkRuntimeInfoP->device,vkRuntimeInfoP->swapChain,&vkRuntimeInfoP->imagesInFlightCount,vkRuntimeInfoP->swapChainImagesP));
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

        CHK_VK(vkCreateImageView(vkRuntimeInfoP->device,&imageViewCreateInfo,NULL,&(vkRuntimeInfoP->swapChainImageViewsP[imageIndex])));
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

    CHK_VK(vkCreateRenderPass(vkRuntimeInfoP->device,&RenderPassInfo,NULL,&vkRuntimeInfoP->renderPass));

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
    CHK_VK(vkCreateDescriptorPool(vkRuntimeInfoP->device,&DescriptorPoolInfo,NULL,&(vkRuntimeInfoP->descriptorPool)));

    VkDescriptorSetLayoutBinding LayoutBinding={0};
    LayoutBinding.stageFlags=VK_SHADER_STAGE_VERTEX_BIT;
    LayoutBinding.binding=0;    //for mvp in binding 0, for all frames this should keep that way
    LayoutBinding.descriptorCount=1;
    LayoutBinding.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    //Create descriptor layouts and fill sets
    VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutInfo={0};
    DescriptorSetLayoutInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    DescriptorSetLayoutInfo.bindingCount=1;
    DescriptorSetLayoutInfo.pBindings=&LayoutBinding;
    vkRuntimeInfoP->descriptorSetLayoutP=(VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout));
    CHK_VK(vkCreateDescriptorSetLayout(vkRuntimeInfoP->device,&DescriptorSetLayoutInfo,NULL,vkRuntimeInfoP->descriptorSetLayoutP));

    //Allocate descriptor set
    VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo={0};
    DescriptorSetAllocateInfo.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DescriptorSetAllocateInfo.descriptorPool=vkRuntimeInfoP->descriptorPool;
    DescriptorSetAllocateInfo.descriptorSetCount=1; //one mvp matrix and hence one descriptor set per frame in flight
    DescriptorSetAllocateInfo.pSetLayouts=vkRuntimeInfoP->descriptorSetLayoutP;
    vkRuntimeInfoP->descriptorSetsP=(VkDescriptorSet*)malloc(vkRuntimeInfoP->imagesInFlightCount*sizeof(VkDescriptorSet));
    for(uint32_t image=0;image<vkRuntimeInfoP->imagesInFlightCount;image++){  //if the all generated sets need to have the same layout one needs to loop over the allocateDescriptorSets Function
        CHK_VK(vkAllocateDescriptorSets(vkRuntimeInfoP->device,&DescriptorSetAllocateInfo,&(vkRuntimeInfoP->descriptorSetsP[image])));
    }

}

void eng_createGraphicsPipeline(struct VulkanRuntimeInfo* vkRuntimeInfoP){

    VkVertexInputAttributeDescription InputAttributeDescriptionArray[3];
    //Positions
    InputAttributeDescriptionArray[0].location=0;   //will be used for positions vec3 (location=0) in shader
    InputAttributeDescriptionArray[0].binding=0;    //binding used to cross reference to the InputBindingDescription
    InputAttributeDescriptionArray[0].format=VK_FORMAT_R32G32B32_SFLOAT;    //is equivalent to vec3
    InputAttributeDescriptionArray[0].offset=0;     //positions are at the start of our static buffer
    //Normals
    InputAttributeDescriptionArray[1].location=1;
    InputAttributeDescriptionArray[1].binding=0;
    InputAttributeDescriptionArray[1].format=VK_FORMAT_R32G32B32_SFLOAT;    //is equivalent to vec3
    InputAttributeDescriptionArray[1].offset=4*sizeof(float);
    //UVs
    InputAttributeDescriptionArray[2].location=2;
    InputAttributeDescriptionArray[2].binding=0;
    InputAttributeDescriptionArray[2].format=VK_FORMAT_R32G32_SFLOAT;    //is equivalent to vec2
    InputAttributeDescriptionArray[2].offset=8*sizeof(float);


    VkVertexInputBindingDescription InputBindingDescriptionArray[1];
    InputBindingDescriptionArray[0].binding=0;
    InputBindingDescriptionArray[0].inputRate=VK_VERTEX_INPUT_RATE_VERTEX;    //jump to next vertex for every new triangle in the index buffer, not every vertex
    InputBindingDescriptionArray[0].stride=sizeof(float)*10;                     //stride is sizeof(vec4)*/


    //VertexInput
    VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateInfo={0};
    PipelineVertexInputStateInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    PipelineVertexInputStateInfo.pNext=NULL;
    PipelineVertexInputStateInfo.vertexAttributeDescriptionCount=sizeof(InputAttributeDescriptionArray)/sizeof(InputAttributeDescriptionArray[0]);
    PipelineVertexInputStateInfo.vertexBindingDescriptionCount=sizeof(InputBindingDescriptionArray)/sizeof(InputBindingDescriptionArray[0]);
    //needs to be set if we supply vertex buffers to our shader
    PipelineVertexInputStateInfo.pVertexAttributeDescriptions=InputAttributeDescriptionArray;
    PipelineVertexInputStateInfo.pVertexBindingDescriptions=InputBindingDescriptionArray;

    VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyInfo={0};
    PipelineInputAssemblyInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    PipelineInputAssemblyInfo.primitiveRestartEnable=VK_FALSE;
    PipelineInputAssemblyInfo.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    //Vertex Shader
    VkPipelineShaderStageCreateInfo VertexShaderStageCreateInfo={0};
    VertexShaderStageCreateInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    VertexShaderStageCreateInfo.module=vkRuntimeInfoP->VertexShaderModule;
    VertexShaderStageCreateInfo.stage=VK_SHADER_STAGE_VERTEX_BIT;
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
    VkPipelineViewportStateCreateInfo PipelineViewportInfo={0};
    PipelineViewportInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    PipelineViewportInfo.viewportCount=1;
    PipelineViewportInfo.scissorCount=1;
    PipelineViewportInfo.pScissors=&scissor;
    PipelineViewportInfo.pViewports=&viewport;
    //RasterizerInfo
    VkPipelineRasterizationStateCreateInfo RasterizationInfo={0};
    RasterizationInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
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
    VkPipelineMultisampleStateCreateInfo MultisampleInfo={0};
    MultisampleInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultisampleInfo.sampleShadingEnable=VK_FALSE;
    MultisampleInfo.alphaToOneEnable=VK_FALSE;
    MultisampleInfo.minSampleShading=1.0f;
    MultisampleInfo.pSampleMask=NULL;
    MultisampleInfo.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    MultisampleInfo.sampleShadingEnable=VK_FALSE;
    MultisampleInfo.alphaToCoverageEnable=VK_FALSE;

    //Fragment
    VkPipelineShaderStageCreateInfo FragmentShaderStageCreateInfo={0};
    FragmentShaderStageCreateInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    FragmentShaderStageCreateInfo.module=vkRuntimeInfoP->FragmentShaderModule;
    FragmentShaderStageCreateInfo.stage=VK_SHADER_STAGE_FRAGMENT_BIT;
    FragmentShaderStageCreateInfo.pSpecializationInfo=NULL;
    FragmentShaderStageCreateInfo.pName="main";

    //Blending
    VkPipelineColorBlendAttachmentState ColorBlendAttachment={0};
    ColorBlendAttachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    ColorBlendAttachment.blendEnable=VK_FALSE;
    ColorBlendAttachment.colorBlendOp=VK_BLEND_OP_ADD;
    ColorBlendAttachment.srcColorBlendFactor=VK_BLEND_FACTOR_ONE;
    ColorBlendAttachment.dstColorBlendFactor=VK_BLEND_FACTOR_ZERO;
    ColorBlendAttachment.alphaBlendOp=VK_BLEND_OP_ADD;
    ColorBlendAttachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
    ColorBlendAttachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;

    VkPipelineColorBlendStateCreateInfo ColorBlendInfo={0};
    ColorBlendInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendInfo.flags=0;
    ColorBlendInfo.logicOp=VK_FALSE;
    ColorBlendInfo.attachmentCount=1;
    ColorBlendInfo.pAttachments=&ColorBlendAttachment;
    ColorBlendInfo.blendConstants[0]=0.0f;
    ColorBlendInfo.blendConstants[1]=0.0f;
    ColorBlendInfo.blendConstants[2]=0.0f;
    ColorBlendInfo.blendConstants[3]=0.0f;

    //Pipeline Layout for Uniforms
    VkPipelineLayoutCreateInfo PipelineLayoutInfo={0};
    PipelineLayoutInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    //TODO reenable
    PipelineLayoutInfo.setLayoutCount=1;
    PipelineLayoutInfo.pSetLayouts=vkRuntimeInfoP->descriptorSetLayoutP;
    //PipelineLayoutInfo.setLayoutCount=0;
    //PipelineLayoutInfo.pSetLayouts=NULL;
    PipelineLayoutInfo.pushConstantRangeCount=0;
    PipelineLayoutInfo.pPushConstantRanges=NULL;

    CHK_VK(vkCreatePipelineLayout(vkRuntimeInfoP->device,&PipelineLayoutInfo,NULL,&(vkRuntimeInfoP->graphicsPipelineLayout)));

    //Assemble everything
    VkPipelineShaderStageCreateInfo Stages[2]={
        VertexShaderStageCreateInfo,
        FragmentShaderStageCreateInfo
    };

    VkGraphicsPipelineCreateInfo PipelineInfo={0};
    PipelineInfo.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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
    CHK_VK(vkCreateGraphicsPipelines(vkRuntimeInfoP->device,VK_NULL_HANDLE,1,&PipelineInfo,NULL,&(vkRuntimeInfoP->graphicsPipeline)));
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
        CHK_VK(vkCreateFramebuffer(vkRuntimeInfoP->device,&FramebufferInfo,NULL,&(vkRuntimeInfoP->FramebufferP[framebufferIdx])));
    }
}

void eng_createRenderCommandBuffers(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    vkRuntimeInfoP->CommandbufferP=(VkCommandBuffer*)malloc(sizeof(VkCommandBuffer)*vkRuntimeInfoP->imagesInFlightCount);

    VkCommandBufferAllocateInfo CommandBufferAllocateInfo={0};
    CommandBufferAllocateInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocateInfo.commandPool=vkRuntimeInfoP->commandPool;
    CommandBufferAllocateInfo.commandBufferCount=vkRuntimeInfoP->imagesInFlightCount;
    CommandBufferAllocateInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    CHK_VK(vkAllocateCommandBuffers(vkRuntimeInfoP->device,&CommandBufferAllocateInfo,vkRuntimeInfoP->CommandbufferP));
    for(uint32_t CommandBufferIdx=0;CommandBufferIdx<vkRuntimeInfoP->imagesInFlightCount;CommandBufferIdx++){
        VkCommandBufferBeginInfo CommandBufferBeginInfo={0};
        CommandBufferBeginInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        CHK_VK(vkBeginCommandBuffer(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],&CommandBufferBeginInfo));

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

        VkDeviceSize PNUBufferOffset=ObjectToBeLoaded.PosNormUvInBufOffset;
        vkCmdBindVertexBuffers(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],0,1,&(ObjectToBeLoaded.readVertexBufferP->BufferHandle),&PNUBufferOffset);
        vkCmdBindIndexBuffer(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],ObjectToBeLoaded.readVertexBufferP->BufferHandle,ObjectToBeLoaded.IdxInBufOffset,VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],VK_PIPELINE_BIND_POINT_GRAPHICS,vkRuntimeInfoP->graphicsPipelineLayout,0,1,&(vkRuntimeInfoP->descriptorSetsP[CommandBufferIdx]),0,NULL);
        //vkCmdBindDescriptorSets(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],VK_PIPELINE_BIND_POINT_GRAPHICS,vkRuntimeInfoP->graphicsPipelineLayout,0,1,vkRuntimeInfoP->descriptorSetsP,0,NULL);
        vkCmdBindPipeline(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],VK_PIPELINE_BIND_POINT_GRAPHICS,vkRuntimeInfoP->graphicsPipeline);
        vkCmdDrawIndexed(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],ObjectToBeLoaded.vertexCount,1,0,0,0);
        vkCmdEndRenderPass(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx]);
        CHK_VK(vkEndCommandBuffer(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx]));
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
        CHK_VK(vkCreateFence(vkRuntimeInfoP->device,&FenceInfo,NULL,(vkRuntimeInfoP->ImageAlreadyProcessingFenceP)+FrameIdx));
        CHK_VK(vkCreateSemaphore(vkRuntimeInfoP->device,&SemaphoreInfo,NULL,(vkRuntimeInfoP->imageAvailableSemaphoreP)+FrameIdx));
        CHK_VK(vkCreateSemaphore(vkRuntimeInfoP->device,&SemaphoreInfo,NULL,(vkRuntimeInfoP->renderFinishedSemaphoreP)+FrameIdx));
    }

}

void error_callback(int code,const char* description){
    dprintf(DBGT_ERROR,"Error in glfw code: %d, \n String %s",code,description);
}

void eng_draw(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    static float angle=0;
    static uint32_t nextImageIdx=0;
    //Work
    if(vkWaitForFences(vkRuntimeInfoP->device,1,(vkRuntimeInfoP->ImageAlreadyProcessingFenceP)+nextImageIdx,VK_TRUE,UINT64_MAX)){
        dprintf(DBGT_ERROR,"Waiting for fence timeout");
        exit(1);
    }
    CHK_VK(vkResetFences(vkRuntimeInfoP->device,1,(vkRuntimeInfoP->ImageAlreadyProcessingFenceP)+nextImageIdx));

    //create new MVP
    //Model
    mat4x4 MMatrix;
    mat4x4 MVPMatrix;
    mat4x4_identity(MMatrix);
    mat4x4_rotate_X(MVPMatrix,MMatrix,angle);
    angle+=0.01f;

    //View
    mat4x4 VMatrix;
    vec3 eye={2.0f,0.0f,2.0f};
    vec3 center={0.0f,0.0f,0.0f};
    vec3 up={0.0f,0.0f,1.0f};
    mat4x4_look_at(VMatrix,eye,center,up);
    /*
    for(int row=0;row<4;row++){
        for(int col=0;col<4;col++){
            printf("%f \t",VMatrix[row][col]);
        }
        printf("\n");
    }
    printf("\n");*/
    mat4x4_mul(MVPMatrix,VMatrix,MVPMatrix);

    //projection
    mat4x4 PMatrix;
    float aspRatio=((float)vkRuntimeInfoP->swapChainImageExtent.width)/vkRuntimeInfoP->swapChainImageExtent.height;
    mat4x4_perspective(PMatrix,1.6f,aspRatio,0.1f,10.0f);
    mat4x4_mul(MVPMatrix,PMatrix,MVPMatrix);

    //copy in buffer
    void* mappedUniformSliceP;
    CHK_VK(vkMapMemory(vkRuntimeInfoP->device,vkRuntimeInfoP->FastUpdatingUniformBuffer.Memory,sizeof(mat4x4)*nextImageIdx,sizeof(mat4x4),0,&mappedUniformSliceP));
    memcpy(mappedUniformSliceP,&(MVPMatrix[0][0]),sizeof(mat4x4));
    //TODO flush memory
    vkUnmapMemory(vkRuntimeInfoP->device,vkRuntimeInfoP->FastUpdatingUniformBuffer.Memory);

    //Get next image from swapChain
    CHK_VK(vkAcquireNextImageKHR(vkRuntimeInfoP->device,vkRuntimeInfoP->swapChain,UINT64_MAX,(vkRuntimeInfoP->imageAvailableSemaphoreP)[nextImageIdx],VK_NULL_HANDLE,&nextImageIdx));

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

    CHK_VK(vkQueueSubmit(vkRuntimeInfoP->graphics_queue,1,&SubmitInfo,vkRuntimeInfoP->ImageAlreadyProcessingFenceP[nextImageIdx]));
    VkPresentInfoKHR PresentInfo={0};
    PresentInfo.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount=1;
    PresentInfo.pWaitSemaphores=(vkRuntimeInfoP->renderFinishedSemaphoreP)+nextImageIdx;
    PresentInfo.swapchainCount=1;
    PresentInfo.pSwapchains=&(vkRuntimeInfoP->swapChain);
    PresentInfo.pImageIndices=&nextImageIdx;
    CHK_VK(vkQueuePresentKHR(vkRuntimeInfoP->graphics_queue,&PresentInfo));

    nextImageIdx+=1;
    nextImageIdx%=vkRuntimeInfoP->imagesInFlightCount;
}

void eng_writeDescriptorSets(struct VulkanRuntimeInfo* vkRuntimeInfoP){
    for(uint32_t image=0;image<vkRuntimeInfoP->imagesInFlightCount;image++){
        //Write descriptor set
        VkDescriptorBufferInfo BufferInfo;
        BufferInfo.buffer=vkRuntimeInfoP->FastUpdatingUniformBuffer.BufferHandle;
        BufferInfo.offset=sizeof(mat4x4)*image;
        BufferInfo.range=sizeof(mat4x4);

        VkWriteDescriptorSet WriteDescriptorSet={0};
        WriteDescriptorSet.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        WriteDescriptorSet.descriptorCount=1;
        WriteDescriptorSet.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        WriteDescriptorSet.dstBinding=0;
        WriteDescriptorSet.dstSet=vkRuntimeInfoP->descriptorSetsP[image];
        WriteDescriptorSet.pBufferInfo=&BufferInfo;
        vkUpdateDescriptorSets(vkRuntimeInfoP->device,1,&WriteDescriptorSet,0,NULL);
    }
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
    struct xmlTreeElement* applicationNameXmlElmntP=getFirstSubelementWithASCII(eng_setupxmlP,"ApplicationName",NULL,NULL,0,1);
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
    eng_createDescriptorPoolAndSets(&engVkRuntimeInfo);                     //depends on eng_createSwapChain
    eng_createGraphicsPipeline(&engVkRuntimeInfo);                          //depends on eng_createShaderModule and eng_createImageViews and eng_createDescriptorPoolAndSets
    eng_createFramebuffers(&engVkRuntimeInfo);                              //depends on eng_createRenderPass   and eng_createImageViews
    eng_createCommandPool(&engVkRuntimeInfo);                               //depends on eng_createDevice
    eng_load_static_models(&engVkRuntimeInfo);                              //depends on eng_createCommandPool and creates vertex buffer
    eng_writeDescriptorSets(&engVkRuntimeInfo);                             //depends on eng_load_static_models
    eng_createRenderCommandBuffers(&engVkRuntimeInfo);                      //depends on eng_createCommandPool and eng_createFramebuffers and eng_load_static_models and createPipeline
    eng_createSynchronizationPrimitives(&engVkRuntimeInfo);
    dprintf(DBGT_INFO,"Got here");
    while (!glfwWindowShouldClose(mainWindowP)) {
        eng_draw(&engVkRuntimeInfo);
        glfwPollEvents();
    }
}
