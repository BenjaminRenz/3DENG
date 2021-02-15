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
#include "xmlReader/xmlHelper.h"
#include "daeLoader/daeLoader.h"
#include "dynList/dynList.h"

#include "vkEngine/core.h"



uint32_t eng_get_version_number_from_xmlemnt(xmlTreeElement* currentReqXmlP);
uint32_t eng_get_version_number_from_UTF32DynlistP(Dl_utf32Char* inputStringP);
struct xmlTreeElement* eng_get_eng_setupxml(char* FilePath, int debug_enabled);


struct modelPathAndName {
    Dl_utf32Char* pathString;
    Dl_utf32Char* modelName;
};

DlTypedef_plain(modelPathAndName, struct modelPathAndName);




Dl_eng3dObj* scene1ObjectsDlP;

VkCommandBuffer _eng_cmdBuf_startSingleUse(struct VulkanRuntimeInfo* vkRuntimeInfoP);
void _eng_cmdBuf_endAndSubmitSingleUse(struct VulkanRuntimeInfo* vkRuntimeInfoP, VkCommandBuffer SingleUseBufferP);

void eng_createShaderModule(struct VulkanRuntimeInfo* vkRuntimeInfoP, char* ShaderFileLocation) {
    FILE* ShaderXmlFileP = fopen(ShaderFileLocation, "rb");
    if(ShaderXmlFileP == NULL) {
        dprintf(DBGT_ERROR, "Could not open file %s for compilation", ShaderFileLocation);
        exit(1);
    }
    xmlTreeElement* xmlRootElementP;
    readXML(ShaderXmlFileP, &xmlRootElementP);

    xmlTreeElement* VertexShaderXmlElmntP = getFirstSubelementWithASCII(xmlRootElementP, "vertex", NULL, NULL, xmltype_tag, 0);
    xmlTreeElement* VertexShaderContentXmlElmntP = getFirstSubelementWith(VertexShaderXmlElmntP, NULL, NULL, NULL, xmltype_chardata, 0);
    char* VertexShaderAsciiSourceP = (char*)malloc((VertexShaderContentXmlElmntP->charData->itemcnt + 1) * sizeof(char));
    uint32_t VertexSourceLength = utf32CutASCII(VertexShaderContentXmlElmntP->charData->items,
                                  VertexShaderContentXmlElmntP->charData->itemcnt,
                                  VertexShaderAsciiSourceP);

    xmlTreeElement* FragmentShaderXmlElmntP = getFirstSubelementWithASCII(xmlRootElementP, "fragment", NULL, NULL, xmltype_tag, 0);
    xmlTreeElement* FragmentShaderContentXmlElmntP = getFirstSubelementWithASCII(FragmentShaderXmlElmntP, NULL, NULL, NULL, xmltype_chardata, 0);
    char* FragmentShaderAsciiSourceP = (char*)malloc((FragmentShaderContentXmlElmntP->charData->itemcnt + 1) * sizeof(char));
    uint32_t FragmentSourceLength = utf32CutASCII(FragmentShaderContentXmlElmntP->charData->items,
                                    FragmentShaderContentXmlElmntP->charData->itemcnt,
                                    FragmentShaderAsciiSourceP);

    shaderc_compiler_t shaderCompilerObj = shaderc_compiler_initialize();
    shaderc_compilation_result_t compResultVertex = shaderc_compile_into_spv(shaderCompilerObj, VertexShaderAsciiSourceP, VertexSourceLength, shaderc_glsl_vertex_shader, ShaderFileLocation, "main", NULL);
    shaderc_compilation_result_t compResultFragment = shaderc_compile_into_spv(shaderCompilerObj, FragmentShaderAsciiSourceP, FragmentSourceLength, shaderc_glsl_fragment_shader, ShaderFileLocation, "main", NULL);
    free(VertexShaderAsciiSourceP);
    free(FragmentShaderAsciiSourceP);
    //Print result for vertex
    if(shaderc_result_get_compilation_status(compResultVertex)) {
        dprintf(DBGT_ERROR, "Vertex shader compilation failed");
        if(shaderc_result_get_num_errors(compResultVertex)) {
            dprintf(DBGT_ERROR, "Error was:\n%s", shaderc_result_get_error_message(compResultVertex));
            exit(1);
        }
    }
    dprintf(DBGT_INFO, "While compiling vertex_%s there were %lld warnings.", ShaderFileLocation, shaderc_result_get_num_warnings(compResultVertex));
    //Print result for fragment
    if(shaderc_result_get_compilation_status(compResultFragment)) {
        dprintf(DBGT_ERROR, "Fragment shader compilation failed");
        if(shaderc_result_get_num_errors(compResultFragment)) {
            dprintf(DBGT_ERROR, "Error was:\n%s", shaderc_result_get_error_message(compResultFragment));
            exit(1);
        }
    }
    dprintf(DBGT_INFO, "While compiling fragment_%s there were %lld warnings.", ShaderFileLocation, shaderc_result_get_num_warnings(compResultFragment));

    shaderc_result_get_bytes(compResultFragment);
    //cleanup

    shaderc_compiler_release(shaderCompilerObj);

    VkShaderModuleCreateInfo ShaderModuleCreateInfo;
    ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.pNext = NULL;
    ShaderModuleCreateInfo.flags = 0;
    ShaderModuleCreateInfo.pCode = (uint32_t*)shaderc_result_get_bytes(compResultVertex);
    ShaderModuleCreateInfo.codeSize = shaderc_result_get_length(compResultVertex);
    CHK_VK(vkCreateShaderModule(vkRuntimeInfoP->device, &ShaderModuleCreateInfo, NULL, &(vkRuntimeInfoP->VertexShaderModule)));
    //Recycle ShaderModuleCreateInfo

    ShaderModuleCreateInfo.pCode = (uint32_t*)shaderc_result_get_bytes(compResultFragment);
    ShaderModuleCreateInfo.codeSize = shaderc_result_get_length(compResultFragment);
    CHK_VK(vkCreateShaderModule(vkRuntimeInfoP->device, &ShaderModuleCreateInfo, NULL, &(vkRuntimeInfoP->FragmentShaderModule)));

    //shaderc_result_release(compResultVertex);
    //shaderc_result_release(compResultFragment);
}
/*
void _eng_DynamicUnifBuf_allocateAndBind(struct VulkanRuntimeInfo* vkRuntimeInfoP, struct engBufferHandle* BufferWithSpecifiedSizeP){

    eng_AllocBlock_createHandlesAndGetMemReq()

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
                                UniformBufferMemoryRequirements.memoryTypeBits,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, //Memory needs to be Host visible
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                                &(BufferWithSpecifiedSizeP->MemoryFlags),
                                UniformBufferMemorySizeWithPadding);

    //Allocate Memory for unifrom buffer
    _eng_Memory_allocate(vkRuntimeInfoP,&(BufferWithSpecifiedSizeP->Memory),UniformBufferMemorySizeWithPadding,bestMemoryTypeIdx);

    //Bind the buffer handle to memory
    CHK_VK(vkBindBufferMemory(vkRuntimeInfoP->device,BufferWithSpecifiedSizeP->BufferHandle,BufferWithSpecifiedSizeP->Memory,0));
}
*/


struct formatAndReaderArgument {
    VkFormat imageFormat;
    char* readerOutputFormat;
    char pack32Toggle;
};

struct formatAndReaderArgument _eng_getSupported2DImageFormats(struct VulkanRuntimeInfo* vkRuntimeInfoP, VkImageFormatProperties* optionalMinImageFormatPropertiesP, VkImageFormatProperties* returnImageFormatPropertiesP) {
    struct formatAndReaderArgument preferenceList[] = {
        {VK_FORMAT_A8B8G8R8_SRGB_PACK32, "ABGR", 1},
        {VK_FORMAT_B8G8R8A8_SRGB, "BGRA", 0},
        {VK_FORMAT_R8G8B8A8_SRGB, "RGBA", 0}
    };
    for(size_t prefIdx = 0; prefIdx < (sizeof(preferenceList) / sizeof(preferenceList[0])); prefIdx++) {
        VkImageFormatProperties FormatProperties;
        if(vkGetPhysicalDeviceImageFormatProperties(vkRuntimeInfoP->physSelectedDevice, preferenceList[prefIdx].imageFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0, &FormatProperties)) {
            continue;
        }
        if(optionalMinImageFormatPropertiesP) {
            if(optionalMinImageFormatPropertiesP->maxMipLevels > FormatProperties.maxMipLevels) {
                continue;  //the required mip level was not supported
            }
            if(optionalMinImageFormatPropertiesP->maxExtent.height > FormatProperties.maxExtent.height) {
                continue;  //supported resolution was not high enough
            }
            if(optionalMinImageFormatPropertiesP->maxExtent.width > FormatProperties.maxExtent.width) {
                continue; //supported resolution was not high enough
            }
            if(optionalMinImageFormatPropertiesP->maxExtent.depth > FormatProperties.maxExtent.depth) {
                continue; //supported resolution was not high enough
            }
            if(optionalMinImageFormatPropertiesP->maxArrayLayers > FormatProperties.maxArrayLayers) {
                continue; //supports to few array layers
            }
            if(optionalMinImageFormatPropertiesP->maxResourceSize > FormatProperties.maxResourceSize) {
                continue;
            }
            if((optionalMinImageFormatPropertiesP->sampleCounts & FormatProperties.sampleCounts) != optionalMinImageFormatPropertiesP->sampleCounts) {
                continue;
            }
        }
        if(returnImageFormatPropertiesP) {
            memcpy(returnImageFormatPropertiesP, &FormatProperties, sizeof(VkImageFormatProperties));
        }
        return preferenceList[prefIdx];
    }
    dprintf(DBGT_ERROR, "No supported Image Format found");
    exit(1);
}


void _eng_setAccMasksAndTransImgLayout(struct VulkanRuntimeInfo* vkRuntimeInfoP, VkCommandBuffer CommandBuffer, VkImageMemoryBarrier* imgMemBarP) {
    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;
    if(      imgMemBarP->oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             imgMemBarP->newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        //For getting image ready to copy staging buffer to image
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        imgMemBarP->srcAccessMask = 0;
        imgMemBarP->dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if(imgMemBarP->oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
              imgMemBarP->newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        //For getting image after it has been filled ready to be used inside the fragment shader
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        imgMemBarP->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgMemBarP->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    } else {
        dprintf(DBGT_ERROR, "image layout transition not handled");
        exit(1);
    }
    vkCmdPipelineBarrier(CommandBuffer, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, imgMemBarP);
}

void eng_load_static_models(struct VulkanRuntimeInfo* vkRuntimeInfoP, Dl_modelPathAndName* modelPathAndNameDlP) {
    //Make model data accessible for outside functions
    Dl_eng3dObj* all3dObjectsDlP = Dl_eng3dObj_alloc(modelPathAndNameDlP->itemcnt, 0);
    scene1ObjectsDlP = all3dObjectsDlP;

    //check the supported image formats
    VkImageFormatProperties maximumImageProperties;
    struct formatAndReaderArgument readerArg = _eng_getSupported2DImageFormats(vkRuntimeInfoP, NULL, &maximumImageProperties);

    for(size_t ObjectNum = 0; ObjectNum < modelPathAndNameDlP->itemcnt; ObjectNum++) {
        Dl_utf32Char* FilePathString  = modelPathAndNameDlP->items[ObjectNum].pathString;
        Dl_utf32Char* ModelNameString = modelPathAndNameDlP->items[ObjectNum].modelName;
        dprintf(DBGT_INFO, "Itemcount of model Path %d, address of 3dobj %x", modelPathAndNameDlP->itemcnt, &(all3dObjectsDlP->items[ObjectNum].daeData));
        daeLoader_load(FilePathString, ModelNameString, &(all3dObjectsDlP->items[ObjectNum].daeData), readerArg.readerOutputFormat, readerArg.pack32Toggle);
        all3dObjectsDlP->items[ObjectNum].vertexCount = all3dObjectsDlP->items[ObjectNum].daeData.IndexingDlP->itemcnt;
    }

    //
    //  Create Textures
    //
    struct eng_AllocBlock TexAllocBlock = {0};
    TexAllocBlock.TexAllocInfoDlP = Dl_PerTexAllocInfo_alloc(all3dObjectsDlP->itemcnt, NULL);
    struct eng_AllocBlock TexBufStagingAllocBlock = {0};
    TexBufStagingAllocBlock.BufAllocInfoDlP = Dl_PerBufAllocInfo_alloc(all3dObjectsDlP->itemcnt, NULL);

    //fill in fields
    for(size_t ObjectNum = 0; ObjectNum < all3dObjectsDlP->itemcnt; ObjectNum++) {
        struct eng3dObject* current3dObjectP = &(all3dObjectsDlP->items[ObjectNum]);
        if(current3dObjectP->daeData.DiffuseTexture.dataP) {
            //fill in fields to generate buffer handles
            struct eng_PerBufAllocInfo* CurrentBufAllocInfoP   = &(TexBufStagingAllocBlock.BufAllocInfoDlP->items[ObjectNum]);
            VkDeviceSize imageContentSize                      = 4 * current3dObjectP->daeData.DiffuseTexture.width * current3dObjectP->daeData.DiffuseTexture.height;
            CurrentBufAllocInfoP->initContentSizeInBytes       = imageContentSize;
            CurrentBufAllocInfoP->initUsage                    = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            //fill in fields to generate texture handles
            struct eng_PerTexAllocInfo* CurrentTexAllocInfoP   = &(TexAllocBlock.TexAllocInfoDlP->items[ObjectNum]);
            CurrentTexAllocInfoP->initFormat                   = readerArg.imageFormat;
            CurrentTexAllocInfoP->initTiling                   = VK_IMAGE_TILING_OPTIMAL;
            CurrentTexAllocInfoP->initContentExtentInPx.depth  = 1;
            CurrentTexAllocInfoP->initContentExtentInPx.width  = current3dObjectP->daeData.DiffuseTexture.width;
            CurrentTexAllocInfoP->initContentExtentInPx.height = current3dObjectP->daeData.DiffuseTexture.height;
            CurrentTexAllocInfoP->initUsage                    = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        } else {

        }
    }

    //create images for textures
    eng_AllocBlock_createHandlesAndGetMemReq (vkRuntimeInfoP, &TexAllocBlock);
    eng_AllocBlock_alignAndCalcSizeAndOffsets(vkRuntimeInfoP, &TexAllocBlock);
    uint32_t resTexMemProps;
    eng_AllocBlock_setFastDevLocalAlloc      (vkRuntimeInfoP, &TexAllocBlock, &resTexMemProps);
    eng_AllocBlock_allocAndBindMem           (vkRuntimeInfoP, &TexAllocBlock);
    if(resTexMemProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) { //unified memory
        dprintf(DBGT_INFO, "Image loader has detected unified memory");
    }

    //create buffers for staging upload
    eng_AllocBlock_createHandlesAndGetMemReq (vkRuntimeInfoP, &TexBufStagingAllocBlock);
    eng_AllocBlock_alignAndCalcSizeAndOffsets(vkRuntimeInfoP, &TexBufStagingAllocBlock);
    eng_AllocBlock_setStagingAlloc           (vkRuntimeInfoP, &TexBufStagingAllocBlock, NULL);
    eng_AllocBlock_allocAndBindMem           (vkRuntimeInfoP, &TexBufStagingAllocBlock);

    //fill staging buffer with data
    for(size_t ObjectNum = 0; ObjectNum < all3dObjectsDlP->itemcnt; ObjectNum++) {
        void* mappedMemoryP;
        CHK_VK(vkMapMemory(vkRuntimeInfoP->device,
                           TexBufStagingAllocBlock.Memory,
                           TexBufStagingAllocBlock.BufAllocInfoDlP->items[ObjectNum].OffsetInMemoryInBytes,
                           TexBufStagingAllocBlock.BufAllocInfoDlP->items[ObjectNum].initContentSizeInBytes,
                           0, &mappedMemoryP));
        memcpy(mappedMemoryP,
               all3dObjectsDlP->items[ObjectNum].daeData.DiffuseTexture.dataP,
               TexBufStagingAllocBlock.BufAllocInfoDlP->items[ObjectNum].initContentSizeInBytes);
        vkUnmapMemory(vkRuntimeInfoP->device, TexBufStagingAllocBlock.Memory);
        free(all3dObjectsDlP->items[ObjectNum].daeData.DiffuseTexture.dataP);
    }

    //Schedule upload and layout transition
    VkCommandBuffer UploadTextureCommandBuffer = _eng_cmdBuf_startSingleUse(vkRuntimeInfoP);
    for(size_t ObjectNum = 0; ObjectNum < all3dObjectsDlP->itemcnt; ObjectNum++) {
        //transform image to optimally writable layout
        struct VkImageMemoryBarrier imgMakeWriteDstBarrier = {0};
        imgMakeWriteDstBarrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgMakeWriteDstBarrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        imgMakeWriteDstBarrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgMakeWriteDstBarrier.image               = TexAllocBlock.TexAllocInfoDlP->items[ObjectNum].ImageHandle;
        imgMakeWriteDstBarrier.dstQueueFamilyIndex = vkRuntimeInfoP->graphics_queue_family_idx;
        imgMakeWriteDstBarrier.srcQueueFamilyIndex = vkRuntimeInfoP->graphics_queue_family_idx;
        imgMakeWriteDstBarrier.subresourceRange.layerCount = 1;
        imgMakeWriteDstBarrier.subresourceRange.levelCount = 1;
        imgMakeWriteDstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        _eng_setAccMasksAndTransImgLayout(vkRuntimeInfoP, UploadTextureCommandBuffer, &imgMakeWriteDstBarrier);

        //copy buffer to image
        VkBufferImageCopy copyRegion = {0};
        copyRegion.imageExtent                 = TexAllocBlock.TexAllocInfoDlP->items[ObjectNum].initContentExtentInPx;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
        vkCmdCopyBufferToImage(UploadTextureCommandBuffer,
                               TexBufStagingAllocBlock.BufAllocInfoDlP->items[ObjectNum].BufferHandle,
                               TexAllocBlock.TexAllocInfoDlP->items[ObjectNum].ImageHandle,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &copyRegion);

        //transform image to an optimal read layout for the shaders
        struct VkImageMemoryBarrier imgMakeShaderReadBarrier = {0};
        imgMakeShaderReadBarrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgMakeShaderReadBarrier.oldLayout                   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgMakeShaderReadBarrier.newLayout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgMakeShaderReadBarrier.image                       = TexAllocBlock.TexAllocInfoDlP->items[ObjectNum].ImageHandle;
        imgMakeShaderReadBarrier.dstQueueFamilyIndex         = vkRuntimeInfoP->graphics_queue_family_idx;
        imgMakeShaderReadBarrier.srcQueueFamilyIndex         = vkRuntimeInfoP->graphics_queue_family_idx;
        imgMakeShaderReadBarrier.subresourceRange.layerCount = 1;
        imgMakeShaderReadBarrier.subresourceRange.levelCount = 1;
        imgMakeShaderReadBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        _eng_setAccMasksAndTransImgLayout(vkRuntimeInfoP, UploadTextureCommandBuffer, &imgMakeShaderReadBarrier);
    }
    _eng_cmdBuf_endAndSubmitSingleUse(vkRuntimeInfoP, UploadTextureCommandBuffer);

    //
    //  Create Vertex buffers
    //
    struct eng_AllocBlock VtxBuffAllocBlock = {0};
    VtxBuffAllocBlock.BufAllocInfoDlP = Dl_PerBufAllocInfo_alloc(all3dObjectsDlP->itemcnt, NULL);
    for(size_t ObjectNum = 0; ObjectNum < all3dObjectsDlP->itemcnt; ObjectNum++) {
        struct eng3dObject* current3dObjectP = &(all3dObjectsDlP->items[ObjectNum]);
        current3dObjectP->PosNormUvInBufOffset  = 0;
        current3dObjectP->IdxInBufOffset        = current3dObjectP->daeData.CombinedPsNrUvDlP->itemcnt * sizeof(float);

        struct eng_PerBufAllocInfo* CurrentBufAllocInfoP = &(VtxBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum]);
        CurrentBufAllocInfoP->initContentSizeInBytes = (current3dObjectP->daeData.CombinedPsNrUvDlP->itemcnt * sizeof(float))
                + (current3dObjectP->daeData.IndexingDlP->itemcnt       * sizeof(uint32_t));
        CurrentBufAllocInfoP->initUsage =  VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                           | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                           | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                                           | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    eng_AllocBlock_createHandlesAndGetMemReq  (vkRuntimeInfoP, &VtxBuffAllocBlock);
    for(size_t ObjectNum = 0; ObjectNum < all3dObjectsDlP->itemcnt; ObjectNum++) {
        struct eng3dObject* current3dObjectP = &(all3dObjectsDlP->items[ObjectNum]);
        current3dObjectP->vtxBufferHandle = VtxBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum].BufferHandle;
    }
    eng_AllocBlock_alignAndCalcSizeAndOffsets (vkRuntimeInfoP, &VtxBuffAllocBlock);
    uint32_t resVtxBufMemProps;
    eng_AllocBlock_setFastDevLocalAlloc       (vkRuntimeInfoP, &VtxBuffAllocBlock, &resVtxBufMemProps);
    eng_AllocBlock_allocAndBindMem            (vkRuntimeInfoP, &VtxBuffAllocBlock);

    int unifiedMemoryFlag = resVtxBufMemProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    //handle the case where we need to create a staging buffer
    struct eng_AllocBlock optVtxStagingBuffAllocBlock = {0};
    if(!unifiedMemoryFlag) { //non unified memory detected, we need to create a separate staging buffer on the cpu side memory
        optVtxStagingBuffAllocBlock.BufAllocInfoDlP = Dl_PerBufAllocInfo_alloc(all3dObjectsDlP->itemcnt, NULL);
        //copy contentSize information from GPU side buffer
        for(size_t ObjectNum = 0; ObjectNum < all3dObjectsDlP->itemcnt; ObjectNum++) {
            struct eng_PerBufAllocInfo* CurrentVtxBufAllocInfoP        =           &(VtxBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum]);
            struct eng_PerBufAllocInfo* CurrentStagingVtxBufAllocInfoP = &(optVtxStagingBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum]);
            CurrentStagingVtxBufAllocInfoP->initContentSizeInBytes = CurrentVtxBufAllocInfoP->initContentSizeInBytes;
            CurrentStagingVtxBufAllocInfoP->initUsage              =  VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                    | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                    | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                    | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }
        eng_AllocBlock_createHandlesAndGetMemReq  (vkRuntimeInfoP, &optVtxStagingBuffAllocBlock);
        eng_AllocBlock_alignAndCalcSizeAndOffsets (vkRuntimeInfoP, &optVtxStagingBuffAllocBlock);
        eng_AllocBlock_setStagingAlloc            (vkRuntimeInfoP, &optVtxStagingBuffAllocBlock, NULL);
        eng_AllocBlock_allocAndBindMem            (vkRuntimeInfoP, &optVtxStagingBuffAllocBlock);
    }

    //Move data into Staging buffer or if unified architecture into GPU buffer
    for(size_t ObjectNum = 0; ObjectNum < all3dObjectsDlP->itemcnt; ObjectNum++) {
        //map memory
        void* mappedMemoryP;
        if(!unifiedMemoryFlag) {
            CHK_VK(vkMapMemory(vkRuntimeInfoP->device,
                               optVtxStagingBuffAllocBlock.Memory,
                               optVtxStagingBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum].OffsetInMemoryInBytes,
                               optVtxStagingBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum].initContentSizeInBytes,
                               0, &mappedMemoryP));
        } else {
            CHK_VK(vkMapMemory(vkRuntimeInfoP->device,
                               VtxBuffAllocBlock.Memory,
                               VtxBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum].OffsetInMemoryInBytes,
                               VtxBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum].initContentSizeInBytes,
                               0, &mappedMemoryP));
        }

        //copy position,normal,uv data
        memcpy(((char*)mappedMemoryP) + all3dObjectsDlP->items[ObjectNum].PosNormUvInBufOffset,
               all3dObjectsDlP->items[ObjectNum].daeData.CombinedPsNrUvDlP->items,
               all3dObjectsDlP->items[ObjectNum].daeData.CombinedPsNrUvDlP->itemcnt * sizeof(float));
        //copy combined index
        memcpy(((char*)mappedMemoryP) + all3dObjectsDlP->items[ObjectNum].IdxInBufOffset,
               all3dObjectsDlP->items[ObjectNum].daeData.IndexingDlP->items,
               all3dObjectsDlP->items[ObjectNum].daeData.IndexingDlP->itemcnt * sizeof(uint32_t));
        if(!unifiedMemoryFlag) {
            vkUnmapMemory(vkRuntimeInfoP->device, optVtxStagingBuffAllocBlock.Memory);
        } else {
            vkUnmapMemory(vkRuntimeInfoP->device, VtxBuffAllocBlock.Memory);
        }

        Dl_float_delete(all3dObjectsDlP->items[ObjectNum].daeData.CombinedPsNrUvDlP);
        Dl_uint32_delete(all3dObjectsDlP->items[ObjectNum].daeData.IndexingDlP);
    }

    if(!unifiedMemoryFlag) {
        //schedule upload from Cpu to Gpu side
        VkCommandBuffer UploadCommandBuffer = _eng_cmdBuf_startSingleUse(vkRuntimeInfoP);
        //copy command for every object
        VkBufferCopy copyRegion = {0};
        for(size_t ObjectNum = 0; ObjectNum < optVtxStagingBuffAllocBlock.BufAllocInfoDlP->itemcnt; ObjectNum++) {
            copyRegion.size = optVtxStagingBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum].initContentSizeInBytes;
            vkCmdCopyBuffer(UploadCommandBuffer,
                            optVtxStagingBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum].BufferHandle,
                            VtxBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum].BufferHandle,
                            1, &copyRegion);
        }
        //end recording and submit
        _eng_cmdBuf_endAndSubmitSingleUse(vkRuntimeInfoP, UploadCommandBuffer);
    }

    //
    //  Create Uniform buffers
    //

    //Setup Uniform Buffer, no upload needed
    //ToDo support per object mvp matrices
    vkRuntimeInfoP->FastUpdateUniformAllocP = (struct eng_AllocBlock*)malloc(sizeof(struct eng_AllocBlock));
    memset(vkRuntimeInfoP->FastUpdateUniformAllocP, 0, sizeof(struct eng_AllocBlock));
    vkRuntimeInfoP->FastUpdateUniformAllocP->BufAllocInfoDlP = Dl_PerBufAllocInfo_alloc(1, NULL);
    vkRuntimeInfoP->FastUpdateUniformAllocP->BufAllocInfoDlP->items[0].initContentSizeInBytes = sizeof(mat4x4) * vkRuntimeInfoP->imagesInFlightCount;
    vkRuntimeInfoP->FastUpdateUniformAllocP->BufAllocInfoDlP->items[0].initUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    eng_AllocBlock_createHandlesAndGetMemReq  (vkRuntimeInfoP, vkRuntimeInfoP->FastUpdateUniformAllocP);
    eng_AllocBlock_alignAndCalcSizeAndOffsets (vkRuntimeInfoP, vkRuntimeInfoP->FastUpdateUniformAllocP);
    eng_AllocBlock_setUniformAlloc            (vkRuntimeInfoP, vkRuntimeInfoP->FastUpdateUniformAllocP, NULL);
    eng_AllocBlock_allocAndBindMem            (vkRuntimeInfoP, vkRuntimeInfoP->FastUpdateUniformAllocP);

    //Cleanup
    if(!unifiedMemoryFlag) {
        for(size_t ObjectNum = 0; ObjectNum < optVtxStagingBuffAllocBlock.BufAllocInfoDlP->itemcnt; ObjectNum++) {
            vkDestroyBuffer(vkRuntimeInfoP->device, optVtxStagingBuffAllocBlock.BufAllocInfoDlP->items[ObjectNum].BufferHandle, NULL);
            vkDestroyBuffer(vkRuntimeInfoP->device, TexBufStagingAllocBlock.BufAllocInfoDlP->items[ObjectNum].BufferHandle, NULL);
        }
        vkFreeMemory(vkRuntimeInfoP->device, optVtxStagingBuffAllocBlock.Memory, NULL);
        vkFreeMemory(vkRuntimeInfoP->device, TexBufStagingAllocBlock.Memory, NULL);
        Dl_PerBufAllocInfo_delete(optVtxStagingBuffAllocBlock.BufAllocInfoDlP);
    }
}

VkCommandBuffer _eng_cmdBuf_startSingleUse(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    //create command buffer
    VkCommandBuffer UploadCommandBuffer;
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {0};
    CommandBufferAllocateInfo.sType             = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocateInfo.commandBufferCount = 1;
    CommandBufferAllocateInfo.level             = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferAllocateInfo.commandPool       = vkRuntimeInfoP->commandPool;
    CHK_VK(vkAllocateCommandBuffers(vkRuntimeInfoP->device, &CommandBufferAllocateInfo, &UploadCommandBuffer));
    //start recording
    VkCommandBufferBeginInfo CommandBufferBeginInfo = {0};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CHK_VK(vkBeginCommandBuffer(UploadCommandBuffer, &CommandBufferBeginInfo));
    return UploadCommandBuffer;
}

void _eng_cmdBuf_endAndSubmitSingleUse(struct VulkanRuntimeInfo* vkRuntimeInfoP, VkCommandBuffer SingleUseBufferP) {
    //end recording
    vkEndCommandBuffer(SingleUseBufferP);
    //submit to gpu with no synchronisation
    VkSubmitInfo SubmitInfo = {0};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &SingleUseBufferP;
    CHK_VK(vkQueueSubmit(vkRuntimeInfoP->graphics_queue, 1, &SubmitInfo, VK_NULL_HANDLE));
    CHK_VK(vkQueueWaitIdle(vkRuntimeInfoP->graphics_queue));
    vkFreeCommandBuffers(vkRuntimeInfoP->device, vkRuntimeInfoP->commandPool, 1, &SingleUseBufferP);
}

void eng_createInstance(struct VulkanRuntimeInfo* vkRuntimeInfoP, xmlTreeElement* eng_setupxmlP) {
    //get required information from xml object in memory
    //engine and app name
    xmlTreeElement* engNameXmlElmntP = getFirstSubelementWithASCII(eng_setupxmlP, "EngineName", NULL, NULL, xmltype_tag, 0);
    xmlTreeElement* engNameContentXmlElmntP = getNthChildWithType(engNameXmlElmntP, 0, xmltype_chardata);
    Dl_utf32Char* engNameStrippedString = Dl_utf32Char_stripOuterSpaces(engNameContentXmlElmntP->charData);
    char* engNameCharP = Dl_utf32Char_toStringAlloc_freeArg1(engNameStrippedString);

    xmlTreeElement* appNameXmlElmntP = getFirstSubelementWithASCII(eng_setupxmlP, "ApplicationName", NULL, NULL, xmltype_tag, 0);
    xmlTreeElement* appNameContentXmlElmntP = getNthChildWithType(appNameXmlElmntP, 0, xmltype_chardata);
    Dl_utf32Char* appNameStrippedString = Dl_utf32Char_stripOuterSpaces(appNameContentXmlElmntP->charData);
    char* appNameCharP = Dl_utf32Char_toStringAlloc_freeArg1(appNameStrippedString);

    //engine and app version
    xmlTreeElement* engVersionXmlElmntP = getFirstSubelementWithASCII(eng_setupxmlP, "EngineVersion", NULL, NULL, xmltype_tag, 0);
    xmlTreeElement* engVersionContentXmlElmntP = getNthChildWithType(engVersionXmlElmntP, 0, xmltype_chardata);
    Dl_utf32Char* engVersionStrippedString = Dl_utf32Char_stripOuterSpaces(engVersionContentXmlElmntP->charData);
    uint32_t engVersion = eng_get_version_number_from_UTF32DynlistP(engVersionStrippedString);

    xmlTreeElement* appVersionXmlElmntP = getFirstSubelementWithASCII(eng_setupxmlP, "ApplicationVersion", NULL, NULL, xmltype_tag, 0);
    xmlTreeElement* appVersionContentXmlElmntP = getFirstSubelementWith(appVersionXmlElmntP, NULL, NULL, NULL, xmltype_chardata, 0);
    Dl_utf32Char* appVersionStrippedString = Dl_utf32Char_stripOuterSpaces(appVersionContentXmlElmntP->charData);
    uint32_t appVersion = eng_get_version_number_from_UTF32DynlistP(appVersionStrippedString);

    //Create Application Info structure
    VkApplicationInfo AppInfo;
    AppInfo.sType =              VK_STRUCTURE_TYPE_APPLICATION_INFO;
    AppInfo.pNext =              NULL;
    AppInfo.apiVersion =         VK_API_VERSION_1_1;
    AppInfo.pApplicationName =   appNameCharP;
    AppInfo.applicationVersion = appVersion;
    AppInfo.pEngineName =        engNameCharP;
    AppInfo.engineVersion =      engVersion;

    //retrieve required layers and extensions for instance
    xmlTreeElement* reqInstLayerXmlElmntP = getFirstSubelementWithASCII(eng_setupxmlP, "RequiredInstanceLayers", NULL, NULL, xmltype_tag, 0);
    Dl_xmlP* reqInstLayerDynlistP = getAllSubelementsWithASCII(reqInstLayerXmlElmntP, "Layer", NULL, NULL, xmltype_tag, 0);
    xmlTreeElement* reqInstExtensionXmlElmntP = getFirstSubelementWithASCII(eng_setupxmlP, "RequiredInstanceExtensions", NULL, NULL, xmltype_tag, 0);
    Dl_xmlP* reqInstExtensionDynlistP = getAllSubelementsWithASCII(reqInstExtensionXmlElmntP, "Extension", NULL, NULL, xmltype_tag, 0);

    //Check layer support
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    VkLayerProperties* LayerProptertiesP = (VkLayerProperties*)malloc(layerCount * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layerCount, LayerProptertiesP);


    for(unsigned int required_layer_idx = 0; required_layer_idx < reqInstLayerDynlistP->itemcnt; required_layer_idx++) {
        unsigned int available_layer_idx;
        xmlTreeElement* currentLayerXmlElmntP = reqInstLayerDynlistP->items[required_layer_idx];
        Dl_utf32Char* reqLayerNameString = getValueFromKeyNameASCII(currentLayerXmlElmntP->attributes, "name");
        char* reqLayerNameCharP = Dl_utf32Char_toStringAlloc(reqLayerNameString);
        uint32_t minVersion = eng_get_version_number_from_xmlemnt(currentLayerXmlElmntP);
        for(available_layer_idx = 0; available_layer_idx < layerCount; available_layer_idx++) {
            if(!strcmp(LayerProptertiesP[available_layer_idx].layerName, reqLayerNameCharP)) {
                uint32_t availableVersion = LayerProptertiesP[available_layer_idx].implementationVersion;
                if(availableVersion >= minVersion) {
                    break;//requested layer was found with a version that is supported
                } else {
                    dprintf(DBGT_INFO, "Layer %s was found but version %d while %d required!\n", LayerProptertiesP[available_layer_idx].layerName, availableVersion, minVersion);
                }
            }
        }
        if(available_layer_idx == layerCount) { //layer was not found
            dprintf(DBGT_ERROR, "Vulkan instance does not support required layer: %s", reqLayerNameCharP);
            exit(1);
        }
        free(reqLayerNameCharP);
    }
    free(LayerProptertiesP);

    //Check extension support
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    VkExtensionProperties* ExtensionProptertiesP = (VkExtensionProperties*)malloc(extensionCount * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, ExtensionProptertiesP);
    for(uint32_t required_extension_idx = 0; required_extension_idx < reqInstExtensionDynlistP->itemcnt; required_extension_idx++) {
        uint32_t available_extension_idx;
        xmlTreeElement* currentExtensionXmlElmntP = reqInstExtensionDynlistP->items[required_extension_idx];
        char* reqExtensionNameCharP = Dl_utf32Char_toStringAlloc(getValueFromKeyNameASCII(currentExtensionXmlElmntP->attributes, "name"));
        uint32_t minVersion = eng_get_version_number_from_xmlemnt(currentExtensionXmlElmntP);
        for(available_extension_idx = 0; available_extension_idx < extensionCount; available_extension_idx++) {
            if(!strcmp(ExtensionProptertiesP[available_extension_idx].extensionName, reqExtensionNameCharP)) {
                uint32_t availableVersion = ExtensionProptertiesP[available_extension_idx].specVersion;
                if(availableVersion >= minVersion) {
                    break;//requested extension was found with a version that is supported
                } else {
                    dprintf(DBGT_INFO, "Extension %s was found but version %d while %d required!\n", ExtensionProptertiesP[available_extension_idx].extensionName, availableVersion, minVersion);
                }
            }
        }
        if(available_extension_idx == extensionCount) { //extension was not found
            dprintf(DBGT_ERROR, "Vulkan instance does not support required extension %s", reqExtensionNameCharP);
            exit(1);
        }
        free(reqExtensionNameCharP);

    }
    free(ExtensionProptertiesP);

    //generate InstExtensionNames and Count

    //vkRuntimeInfoP->InstExtensionCount=reqInstExtensionDynlistP->itemcnt;
    vkRuntimeInfoP->_engExtAndLayers.InstExtensionCount = reqInstExtensionDynlistP->itemcnt;
    dprintf(DBGT_INFO, "Number of required instance extensions %d", vkRuntimeInfoP->_engExtAndLayers.InstExtensionCount);
    vkRuntimeInfoP->_engExtAndLayers.InstExtensionNamesPP = (char**)malloc(vkRuntimeInfoP->_engExtAndLayers.InstExtensionCount * sizeof(char*));
    for(uint32_t InstExtensionIdx = 0; InstExtensionIdx < vkRuntimeInfoP->_engExtAndLayers.InstExtensionCount; InstExtensionIdx++) {
        xmlTreeElement* currentExtensionXmlElmntP = reqInstExtensionDynlistP->items[InstExtensionIdx];
        vkRuntimeInfoP->_engExtAndLayers.InstExtensionNamesPP[InstExtensionIdx] = Dl_utf32Char_toStringAlloc(getValueFromKeyNameASCII(currentExtensionXmlElmntP->attributes, "name"));
        dprintf(DBGT_INFO, "Requesting inst extension %s", vkRuntimeInfoP->_engExtAndLayers.InstExtensionNamesPP[InstExtensionIdx]);
    }

    //generate InstLayerNames and Count
    vkRuntimeInfoP->_engExtAndLayers.InstLayerCount = reqInstLayerDynlistP->itemcnt;
    vkRuntimeInfoP->_engExtAndLayers.InstLayerNamesPP = (char**)malloc(vkRuntimeInfoP->_engExtAndLayers.InstLayerCount * sizeof(char*));
    for(uint32_t InstLayerIdx = 0; InstLayerIdx < vkRuntimeInfoP->_engExtAndLayers.InstLayerCount; InstLayerIdx++) {
        xmlTreeElement* currentExtensionXmlElmntP = reqInstLayerDynlistP->items[InstLayerIdx];
        vkRuntimeInfoP->_engExtAndLayers.InstLayerNamesPP[InstLayerIdx] = Dl_utf32Char_toStringAlloc(getValueFromKeyNameASCII(currentExtensionXmlElmntP->attributes, "name"));
        dprintf(DBGT_INFO, "Requesting inst layer %s", vkRuntimeInfoP->_engExtAndLayers.InstLayerNamesPP[InstLayerIdx]);
    }

    uint32_t count;
    const char** extensionsInstancePP = glfwGetRequiredInstanceExtensions(&count);

    //Create Vulkan instance
    VkInstanceCreateInfo CreateInfo;
    CreateInfo.sType =                   VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    CreateInfo.pApplicationInfo =        &AppInfo;
    CreateInfo.pNext =                   NULL;
    CreateInfo.flags =                   0;
    CreateInfo.enabledExtensionCount =   count; //vkRuntimeInfoP->InstExtensionCount;
    CreateInfo.ppEnabledExtensionNames = extensionsInstancePP; //vkRuntimeInfoP->InstExtensionNames;
    CreateInfo.enabledLayerCount =       vkRuntimeInfoP->_engExtAndLayers.InstLayerCount;
    CreateInfo.ppEnabledLayerNames =     (const char* const*)vkRuntimeInfoP->_engExtAndLayers.InstLayerNamesPP;

    CHK_VK(vkCreateInstance(&CreateInfo, NULL, &(vkRuntimeInfoP->instance)));
}

uint8_t* eng_vulkan_generate_device_ranking(struct VulkanRuntimeInfo* vkRuntimeInfoP, struct xmlTreeElement* eng_setupxmlP) {

    xmlTreeElement* reqDevLayerXmlElmntP    = getFirstSubelementWithASCII(eng_setupxmlP, "RequiredDeviceLayers", NULL, NULL, xmltype_tag, 1);
    Dl_xmlP* reqDevLayerDynlistP            = getAllSubelementsWithASCII(reqDevLayerXmlElmntP, "Layer", NULL, NULL, xmltype_tag, 1);
    xmlTreeElement* reqDevExtensionXmlElmntP = getFirstSubelementWithASCII(eng_setupxmlP, "RequiredDeviceExtensions", NULL, NULL, xmltype_tag, 1);
    Dl_xmlP* reqDevExtensionDynlistP        = getAllSubelementsWithASCII(reqDevExtensionXmlElmntP, "Extension", NULL, NULL, xmltype_tag, 1);

    //get all vulkan devices
    CHK_VK(vkEnumeratePhysicalDevices(vkRuntimeInfoP->instance, &vkRuntimeInfoP->physDeviceCount, NULL));
    vkRuntimeInfoP->physAvailDevicesP = (VkPhysicalDevice*)malloc(vkRuntimeInfoP->physDeviceCount * sizeof(VkPhysicalDevice));
    CHK_VK(vkEnumeratePhysicalDevices(vkRuntimeInfoP->instance, &vkRuntimeInfoP->physDeviceCount, vkRuntimeInfoP->physAvailDevicesP));

    //check if our device supports the required layers,extensions and queues
    uint8_t* deviceRankingP = malloc(vkRuntimeInfoP->physDeviceCount * sizeof(uint8_t));
    memset(deviceRankingP, 1, sizeof(uint8_t)*vkRuntimeInfoP->physDeviceCount);
    dprintf(DBGT_INFO, "Number of available devices %d", vkRuntimeInfoP->physDeviceCount);
    for(uint32_t physDevIdx = 0; physDevIdx < vkRuntimeInfoP->physDeviceCount; physDevIdx++) {
        //Check device properties
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(vkRuntimeInfoP->physAvailDevicesP[physDevIdx], &deviceProperties);
        if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            deviceRankingP[physDevIdx] += 1; //discrete GPU's are prefered
        }

        //Check layer support
        uint32_t layerCount = 0;
        CHK_VK(vkEnumerateDeviceLayerProperties(vkRuntimeInfoP->physAvailDevicesP[physDevIdx], &layerCount, NULL));
        VkLayerProperties* LayerProptertiesP = (VkLayerProperties*)malloc(layerCount * sizeof(VkLayerProperties));
        CHK_VK(vkEnumerateDeviceLayerProperties(vkRuntimeInfoP->physAvailDevicesP[physDevIdx], &layerCount, LayerProptertiesP));

        for(unsigned int required_layer_idx = 0; required_layer_idx < reqDevLayerDynlistP->itemcnt; required_layer_idx++) {
            unsigned int available_layer_idx;
            xmlTreeElement* currentLayerXmlElmntP = ((struct xmlTreeElement**)(reqDevLayerDynlistP->items))[required_layer_idx];
            char* reqLayerNameCharP = Dl_utf32Char_toStringAlloc(getValueFromKeyNameASCII(currentLayerXmlElmntP->attributes, "name"));
            uint32_t minVersion = eng_get_version_number_from_xmlemnt(currentLayerXmlElmntP);
            for(available_layer_idx = 0; available_layer_idx < layerCount; available_layer_idx++) {
                if(!strcmp(LayerProptertiesP[available_layer_idx].layerName, reqLayerNameCharP)) {
                    uint32_t availableVersion = LayerProptertiesP[available_layer_idx].implementationVersion;
                    if(availableVersion >= minVersion) {
                        break;//requested layer was found with a version that is supported
                    } else {
                        dprintf(DBGT_INFO, "Layer %s was found but version %d while %d required!\n", LayerProptertiesP[available_layer_idx].layerName, availableVersion, minVersion);
                    }
                }
            }
            free(reqLayerNameCharP);
            if(available_layer_idx == layerCount) { //layer was not found
                deviceRankingP[physDevIdx] = 0;
                break;
            }
        }
        free(LayerProptertiesP);
        if(!deviceRankingP[physDevIdx]) {
            dprintf(DBGT_INFO, "Device missing support for at least one layer");
            continue;
        }

        //Check extension support
        uint32_t extensionCount = 0;
        CHK_VK(vkEnumerateDeviceExtensionProperties(vkRuntimeInfoP->physAvailDevicesP[physDevIdx], NULL, &extensionCount, NULL));
        VkExtensionProperties* ExtensionProptertiesP = (VkExtensionProperties*)malloc(extensionCount * sizeof(VkExtensionProperties));
        CHK_VK(vkEnumerateDeviceExtensionProperties(vkRuntimeInfoP->physAvailDevicesP[physDevIdx], NULL, &extensionCount, ExtensionProptertiesP));

        for(unsigned int required_extension_idx = 0; required_extension_idx < reqDevExtensionDynlistP->itemcnt; required_extension_idx++) {
            unsigned int available_extension_idx;
            xmlTreeElement* currentExtensionXmlElmntP = reqDevExtensionDynlistP->items[required_extension_idx];
            char* reqExtensionNameCharP = Dl_utf32Char_toStringAlloc(getValueFromKeyNameASCII(currentExtensionXmlElmntP->attributes, "name"));
            uint32_t minVersion = eng_get_version_number_from_xmlemnt(currentExtensionXmlElmntP);
            for(available_extension_idx = 0; available_extension_idx < extensionCount; available_extension_idx++) {
                if(!strcmp(ExtensionProptertiesP[available_extension_idx].extensionName, reqExtensionNameCharP)) {
                    uint32_t availableVersion = ExtensionProptertiesP[available_extension_idx].specVersion;
                    if(availableVersion >= minVersion) {
                        break;//requested extension was found with a version that is supported
                    } else {
                        dprintf(DBGT_INFO, "Extension %s was found but version %d while %d required!\n", ExtensionProptertiesP[available_extension_idx].extensionName, availableVersion, minVersion);
                    }
                }
            }

            if(available_extension_idx == extensionCount) { //extension was not found
                deviceRankingP[physDevIdx] = 0;
                dprintf(DBGT_INFO, "Device missing support for extension %s", reqExtensionNameCharP);
                free(reqExtensionNameCharP);
                break;
            }
            free(reqExtensionNameCharP);
        }
        free(ExtensionProptertiesP);
        if(!deviceRankingP[physDevIdx]) {
            dprintf(DBGT_INFO, "Device missing support for at least one extension");
            continue;
        }

        //Check supported Queues
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(vkRuntimeInfoP->physAvailDevicesP[physDevIdx], &queueFamilyCount, NULL);
        dprintf(DBGT_INFO, "Found %d queueFamilys", queueFamilyCount);
        VkQueueFamilyProperties* queueFamiliyPropP = (VkQueueFamilyProperties*)malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(vkRuntimeInfoP->physAvailDevicesP[physDevIdx], &queueFamilyCount, queueFamiliyPropP);
        uint32_t queueFamilyIdx;
        for(queueFamilyIdx = 0; queueFamilyIdx < queueFamilyCount; queueFamilyIdx++) {
            dprintf(DBGT_INFO, "Found Queue with Count %d\n Properties:\nGRAP\t COMP\t TRANS\t SPARSE\t PROT\n%d \t %d \t %d\t %d\t %d",
                    queueFamiliyPropP[queueFamilyIdx].queueCount,
                    (queueFamiliyPropP[queueFamilyIdx].queueFlags & VK_QUEUE_GRAPHICS_BIT         ) / VK_QUEUE_GRAPHICS_BIT,
                    (queueFamiliyPropP[queueFamilyIdx].queueFlags & VK_QUEUE_COMPUTE_BIT          ) / VK_QUEUE_COMPUTE_BIT,
                    (queueFamiliyPropP[queueFamilyIdx].queueFlags & VK_QUEUE_TRANSFER_BIT         ) / VK_QUEUE_TRANSFER_BIT,
                    (queueFamiliyPropP[queueFamilyIdx].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT   ) / VK_QUEUE_SPARSE_BINDING_BIT,
                    (queueFamiliyPropP[queueFamilyIdx].queueFlags & VK_QUEUE_PROTECTED_BIT        ) / VK_QUEUE_PROTECTED_BIT
                   );
            if(queueFamiliyPropP[queueFamilyIdx].queueFlags & (VK_QUEUE_GRAPHICS_BIT) && glfwGetPhysicalDevicePresentationSupport(vkRuntimeInfoP->instance, vkRuntimeInfoP->physAvailDevicesP[physDevIdx], queueFamilyIdx)) {
                break;
            }
        }
        free(queueFamiliyPropP);
        if(queueFamilyIdx == queueFamilyCount) {
            deviceRankingP[physDevIdx] = 0;
            dprintf(DBGT_ERROR, "This GPU does not support a Graphics Queue or is missing presentation support.");
            break;
        }
    }

    //generate DevExtensionNames and Count
    vkRuntimeInfoP->_engExtAndLayers.DevExtensionCount = reqDevExtensionDynlistP->itemcnt;
    dprintf(DBGT_INFO, "count: %d", vkRuntimeInfoP->_engExtAndLayers.DevExtensionCount);
    vkRuntimeInfoP->_engExtAndLayers.DevExtensionNamesPP = (char**)malloc(vkRuntimeInfoP->_engExtAndLayers.DevExtensionCount * sizeof(char*));
    for(uint32_t DevExtensionIdx = 0; DevExtensionIdx < vkRuntimeInfoP->_engExtAndLayers.DevExtensionCount; DevExtensionIdx++) {
        xmlTreeElement* currentExtensionXmlElmntP = reqDevExtensionDynlistP->items[DevExtensionIdx];
        //printXMLsubelements(currentExtensionXmlElmntP);
        Dl_utf32Char* extensionNameString = getValueFromKeyNameASCII(currentExtensionXmlElmntP->attributes, "name");
        vkRuntimeInfoP->_engExtAndLayers.DevExtensionNamesPP[DevExtensionIdx] = Dl_utf32Char_toStringAlloc(extensionNameString);
    }

    //generate DevLayerNames and Count
    vkRuntimeInfoP->_engExtAndLayers.DevLayerCount = reqDevLayerDynlistP->itemcnt;
    vkRuntimeInfoP->_engExtAndLayers.DevLayerNamesPP = (char**)malloc(vkRuntimeInfoP->_engExtAndLayers.DevLayerCount * sizeof(char*));
    for(uint32_t DevLayerIdx = 0; DevLayerIdx < vkRuntimeInfoP->_engExtAndLayers.DevLayerCount; DevLayerIdx++) {
        xmlTreeElement* currentExtensionXmlElmntP = reqDevLayerDynlistP->items[DevLayerIdx];
        Dl_utf32Char* layerNameString = getValueFromKeyNameASCII(currentExtensionXmlElmntP->attributes, "name");
        vkRuntimeInfoP->_engExtAndLayers.DevLayerNamesPP[DevLayerIdx] = Dl_utf32Char_toStringAlloc(layerNameString);
    }

    return deviceRankingP;
}

void eng_createDevice(struct VulkanRuntimeInfo* vkRuntimeInfoP, uint8_t* deviceRanking) {
    //find highest ranking device in list
    uint8_t bestRank = 0;
    for(uint32_t deviceNum = 0; deviceNum < vkRuntimeInfoP->physDeviceCount; deviceNum++) {
        if(deviceRanking[deviceNum] > bestRank) {
            bestRank = deviceRanking[deviceNum];
        }
    }
    if(bestRank == 0) {
        dprintf(DBGT_ERROR, "None of your devices supports the required extensions, layers and features required for running this app");
        exit(1);
    }
    uint32_t deviceNum = 0;
    for(; deviceNum < vkRuntimeInfoP->physDeviceCount; deviceNum++) {
        if(deviceRanking[deviceNum] == bestRank) {
            break;
        }
    }
    vkRuntimeInfoP->physSelectedDevice = vkRuntimeInfoP->physAvailDevicesP[deviceNum];

    //select first available graphics queue on device
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vkRuntimeInfoP->physSelectedDevice, &queueFamilyCount, NULL);
    VkQueueFamilyProperties* queueFamiliyPropP = (VkQueueFamilyProperties*)malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(vkRuntimeInfoP->physSelectedDevice, &queueFamilyCount, queueFamiliyPropP);
    uint32_t queueFamilyIdx;

    for(queueFamilyIdx = 0; queueFamilyIdx < queueFamilyCount; queueFamilyIdx++) {
        if(queueFamiliyPropP[queueFamilyIdx].queueFlags & (VK_QUEUE_GRAPHICS_BIT)) {
            break;
        }
        //vkGetPhysicalDeviceSurfaceSupportKHR()
    }
    if(queueFamilyIdx == queueFamilyCount) {
        dprintf(DBGT_ERROR, "No graphics queue which support presentation was found");
        exit(1);
    }
    vkRuntimeInfoP->graphics_queue_family_idx = queueFamilyIdx;
    free(queueFamiliyPropP);


    //Create logical device
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo QueueCreateInfo;
    QueueCreateInfo.sType =              VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    QueueCreateInfo.queueCount =         1;
    QueueCreateInfo.queueFamilyIndex =   vkRuntimeInfoP->graphics_queue_family_idx;
    QueueCreateInfo.pQueuePriorities =   &queuePriority;
    QueueCreateInfo.pNext =              NULL;
    QueueCreateInfo.flags =              0;

    VkDeviceCreateInfo DevCreateInfo;
    DevCreateInfo.sType =                    VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DevCreateInfo.pQueueCreateInfos =        &QueueCreateInfo;
    DevCreateInfo.queueCreateInfoCount =     1;
    DevCreateInfo.pNext =                    NULL;
    DevCreateInfo.enabledExtensionCount =    vkRuntimeInfoP->_engExtAndLayers.DevExtensionCount;
    DevCreateInfo.ppEnabledExtensionNames =  (const char* const*)vkRuntimeInfoP->_engExtAndLayers.DevExtensionNamesPP;
    DevCreateInfo.enabledLayerCount =        vkRuntimeInfoP->_engExtAndLayers.DevLayerCount;
    DevCreateInfo.ppEnabledLayerNames =      (const char* const*)vkRuntimeInfoP->_engExtAndLayers.DevLayerNamesPP;
    DevCreateInfo.pEnabledFeatures =         NULL;
    DevCreateInfo.flags =                    0;
    CHK_VK(vkCreateDevice(vkRuntimeInfoP->physSelectedDevice, &DevCreateInfo, NULL, &(vkRuntimeInfoP->device)));

    //Get handle for graphics queue
    vkGetDeviceQueue(vkRuntimeInfoP->device, vkRuntimeInfoP->graphics_queue_family_idx, 0, &vkRuntimeInfoP->graphics_queue);

    //Get physicalDeviceLimits
    vkRuntimeInfoP->PhysDevPropP = (VkPhysicalDeviceProperties*)malloc(sizeof(VkPhysicalDeviceProperties));
    vkGetPhysicalDeviceProperties(vkRuntimeInfoP->physSelectedDevice, vkRuntimeInfoP->PhysDevPropP);
}

void eng_createCommandPool(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    VkCommandPoolCreateInfo CommandPoolInfo = {0};
    CommandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolInfo.queueFamilyIndex = vkRuntimeInfoP->graphics_queue_family_idx;
    CHK_VK(vkCreateCommandPool(vkRuntimeInfoP->device, &CommandPoolInfo, NULL, &(vkRuntimeInfoP->commandPool)));
}

struct xmlTreeElement* eng_get_eng_setupxml(char* FilePath, int debug_enabled) {
    FILE* engSetupFileP = fopen(FilePath, "rb");
    xmlTreeElement* engSetupRootP = 0;
    readXML(engSetupFileP, &engSetupRootP);
    fclose(engSetupFileP);
    //select release or debug
    xmlTreeElement* engSetupDebOrRelP;
    Dl_xmlP* tempXmlDlP;
    if(debug_enabled) {
        tempXmlDlP = getAllSubelementsWithASCII(engSetupRootP, "Debug", NULL, NULL, xmltype_tag, 1);
    } else {
        tempXmlDlP = getAllSubelementsWithASCII(engSetupRootP, "Release", NULL, NULL, xmltype_tag, 1);
    }
    if(tempXmlDlP->itemcnt != 1) {
        dprintf(DBGT_ERROR, "Invalid EngSetupFile format");
        exit(1);
    }
    engSetupDebOrRelP = tempXmlDlP->items[0];
    //printXMLsubelements(engSetupDebOrRelP);
    Dl_xmlP_delete(tempXmlDlP);
    return engSetupDebOrRelP;
};

uint32_t eng_get_version_number_from_UTF32DynlistP(Dl_utf32Char* inputStringP) {
    Dl_int64* versionNumDlP = Dl_utf32Char_to_int64(Dl_CM_initFromList('.', '.'), inputStringP);
    if(versionNumDlP->itemcnt != 3) {
        dprintf(DBGT_ERROR, "Invalid Version format");
        exit(1);
    }
    uint32_t version = VK_MAKE_VERSION(versionNumDlP->items[0], versionNumDlP->items[1], versionNumDlP->items[2]);
    Dl_int64_delete(versionNumDlP);
    return version;
}

uint32_t eng_get_version_number_from_xmlemnt(xmlTreeElement* currentReqXmlP) {
    Dl_utf32Char* minversionString = getValueFromKeyNameASCII(currentReqXmlP->attributes, "minversion");
    if(!minversionString) {
        return 0;
    }
    uint32_t versionNum = eng_get_version_number_from_UTF32DynlistP(minversionString);
    return versionNum;
}

void cleanup(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    //vkDestroySwapchainKHR(vkRuntimeInfoP->device, vkRuntimeInfoP->swapChain, NULL);
    /*vkDestroyDevice();
    vkDestroySurfaceKHR();
    vkDestroyInstance();*/
    glfwDestroyWindow(vkRuntimeInfoP->mainWindowP);
    glfwTerminate();
}

void eng_createSurface(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    CHK_VK(glfwCreateWindowSurface(vkRuntimeInfoP->instance, vkRuntimeInfoP->mainWindowP, NULL, &vkRuntimeInfoP->surface));
}

void eng_createSwapChain(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    VkBool32 surfaceSupport = VK_TRUE;
    CHK_VK(vkGetPhysicalDeviceSurfaceSupportKHR(vkRuntimeInfoP->physSelectedDevice, vkRuntimeInfoP->graphics_queue_family_idx, vkRuntimeInfoP->surface, &surfaceSupport));
    CHK_VK(surfaceSupport != VK_TRUE);

    //get basic surface capabilitiers
    struct VkSurfaceCapabilitiesKHR surfaceCapabilities;
    CHK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkRuntimeInfoP->physSelectedDevice, vkRuntimeInfoP->surface, &surfaceCapabilities));
    //get supported formats
    uint32_t formatCount = 0;
    CHK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkRuntimeInfoP->physSelectedDevice, vkRuntimeInfoP->surface, &formatCount, NULL));
    VkSurfaceFormatKHR* SurfaceFormatsP = (VkSurfaceFormatKHR*)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    CHK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkRuntimeInfoP->physSelectedDevice, vkRuntimeInfoP->surface, &formatCount, SurfaceFormatsP));
    //get supported present modes
    uint32_t presentModeCount = 0;
    CHK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(vkRuntimeInfoP->physSelectedDevice, vkRuntimeInfoP->surface, &presentModeCount, NULL));
    VkPresentModeKHR* PresentModeP = (VkPresentModeKHR*)malloc(presentModeCount * sizeof(VkPresentModeKHR));
    CHK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(vkRuntimeInfoP->physSelectedDevice, vkRuntimeInfoP->surface, &presentModeCount, PresentModeP));

    //choose the format/mode we want
    uint32_t availableFormatIdx;
    for(availableFormatIdx = 0; availableFormatIdx < formatCount; availableFormatIdx++) {
        if(SurfaceFormatsP[availableFormatIdx].format == VK_FORMAT_B8G8R8A8_SRGB && SurfaceFormatsP[availableFormatIdx].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
            break;
        }
    }
    if(formatCount == availableFormatIdx) {
        dprintf(DBGT_ERROR, "No suitable VK format available");
        exit(1);
    }
    vkRuntimeInfoP->swapChainFormat = SurfaceFormatsP[availableFormatIdx];

    //choose presentation mode
    uint32_t availableModeIdx;
    for(availableModeIdx = 0; availableModeIdx < presentModeCount; availableModeIdx++) {
        if(PresentModeP[availableModeIdx] == VK_PRESENT_MODE_FIFO_KHR ) {
            break;
        }
    }
    if(presentModeCount == availableModeIdx) {
        dprintf(DBGT_ERROR, "No suitable VK present mode available");
        exit(1);
    }

    int32_t glfw_height;
    int32_t glfw_width;
    glfwGetFramebufferSize(vkRuntimeInfoP->mainWindowP, &glfw_height, &glfw_width);
    (vkRuntimeInfoP->swapChainImageExtent).height = clamp_uint32(surfaceCapabilities.minImageExtent.height, (uint32_t)glfw_height, surfaceCapabilities.maxImageExtent.height);
    (vkRuntimeInfoP->swapChainImageExtent).width = clamp_uint32(surfaceCapabilities.minImageExtent.width, (uint32_t)glfw_width, surfaceCapabilities.maxImageExtent.width);

    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = NULL;
    swapchainCreateInfo.flags = 0;
    swapchainCreateInfo.surface = vkRuntimeInfoP->surface;
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
    swapchainCreateInfo.imageFormat = (vkRuntimeInfoP->swapChainFormat).format;
    swapchainCreateInfo.imageColorSpace = (vkRuntimeInfoP->swapChainFormat).colorSpace;
    swapchainCreateInfo.imageExtent = (vkRuntimeInfoP->swapChainImageExtent);
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;      //don't share swapchain image between multiple queues
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform; //not image transform
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; //don't blend with other application windows
    swapchainCreateInfo.presentMode = PresentModeP[availableModeIdx];
    swapchainCreateInfo.clipped = VK_TRUE;  //Don't render pixels that are obstructed by other windows
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    CHK_VK(vkCreateSwapchainKHR(vkRuntimeInfoP->device, &swapchainCreateInfo, NULL, &vkRuntimeInfoP->swapChain));
    CHK_VK(vkGetSwapchainImagesKHR(vkRuntimeInfoP->device, vkRuntimeInfoP->swapChain, &vkRuntimeInfoP->imagesInFlightCount, NULL));
    vkRuntimeInfoP->swapChainImagesP = (VkImage*)malloc(vkRuntimeInfoP->imagesInFlightCount * sizeof(VkImage));
    CHK_VK(vkGetSwapchainImagesKHR(vkRuntimeInfoP->device, vkRuntimeInfoP->swapChain, &vkRuntimeInfoP->imagesInFlightCount, vkRuntimeInfoP->swapChainImagesP));
}

void eng_createImageViews(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    vkRuntimeInfoP->swapChainImageViewsP = (VkImageView*)malloc(vkRuntimeInfoP->imagesInFlightCount * sizeof(VkImageView));
    for(uint32_t imageIndex = 0; imageIndex < vkRuntimeInfoP->imagesInFlightCount; imageIndex++) {
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext                           = NULL;
        imageViewCreateInfo.flags                           = 0;
        imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.format                          = (vkRuntimeInfoP->swapChainFormat).format;
        imageViewCreateInfo.subresourceRange.layerCount     = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
        imageViewCreateInfo.subresourceRange.levelCount     = 1;
        imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.image                           = (vkRuntimeInfoP->swapChainImagesP)[imageIndex];

        CHK_VK(vkCreateImageView(vkRuntimeInfoP->device, &imageViewCreateInfo, NULL, &(vkRuntimeInfoP->swapChainImageViewsP[imageIndex])));
    }
}

void eng_createRenderPass(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    VkAttachmentDescription ColorAttachments = {0};
    ColorAttachments.flags = 0;
    //ColorAttachments.format=vkRuntimeInfoP->swapChainFormat;
    ColorAttachments.format = (vkRuntimeInfoP->swapChainFormat).format;
    ColorAttachments.samples = VK_SAMPLE_COUNT_1_BIT;
    ColorAttachments.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ColorAttachments.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachments.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachments.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachments.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ColorAttachments.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ColorAttachmentRef = {0};
    ColorAttachmentRef.attachment = 0;
    ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subPasses = {0};
    subPasses.flags = 0;
    subPasses.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPasses.colorAttachmentCount = 1;
    subPasses.pColorAttachments = &ColorAttachmentRef;

    VkRenderPassCreateInfo RenderPassInfo = {0};
    RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassInfo.flags = 0;
    RenderPassInfo.pNext = NULL;
    RenderPassInfo.dependencyCount = 0;
    RenderPassInfo.pDependencies = NULL;
    RenderPassInfo.attachmentCount = 1;
    RenderPassInfo.pAttachments = &ColorAttachments;
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &subPasses;

    CHK_VK(vkCreateRenderPass(vkRuntimeInfoP->device, &RenderPassInfo, NULL, &vkRuntimeInfoP->renderPass));

}

void eng_createDescriptorPoolAndSets(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    //Create Descriptor Pool
    //It will hold one descriptor set for each frame in flight
    //to calculate the total pool size pPoolSizes will point to an array specifying the number for each descriptor type for the whole pool
    VkDescriptorPoolSize DescriptorPoolSize = {0};
    DescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    DescriptorPoolSize.descriptorCount = vkRuntimeInfoP->imagesInFlightCount; //all descriptors of !ALL! sets in this pool can only total up to two

    VkDescriptorPoolCreateInfo DescriptorPoolInfo = {0};
    DescriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    DescriptorPoolInfo.poolSizeCount = 1;
    DescriptorPoolInfo.pPoolSizes = &DescriptorPoolSize;
    DescriptorPoolInfo.maxSets = vkRuntimeInfoP->imagesInFlightCount; //the descriptor pool can store two sets
    CHK_VK(vkCreateDescriptorPool(vkRuntimeInfoP->device, &DescriptorPoolInfo, NULL, &(vkRuntimeInfoP->descriptorPool)));

    VkDescriptorSetLayoutBinding LayoutBinding = {0};
    LayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    LayoutBinding.binding = 0;  //for mvp in binding 0, for all frames this should keep that way
    LayoutBinding.descriptorCount = 1;
    LayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    //Create descriptor layouts and fill sets
    VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutInfo = {0};
    DescriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    DescriptorSetLayoutInfo.bindingCount = 1;
    DescriptorSetLayoutInfo.pBindings = &LayoutBinding;
    vkRuntimeInfoP->descriptorSetLayoutP = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout));
    CHK_VK(vkCreateDescriptorSetLayout(vkRuntimeInfoP->device, &DescriptorSetLayoutInfo, NULL, vkRuntimeInfoP->descriptorSetLayoutP));

    //Allocate descriptor set
    VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = {0};
    DescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DescriptorSetAllocateInfo.descriptorPool = vkRuntimeInfoP->descriptorPool;
    DescriptorSetAllocateInfo.descriptorSetCount = 1; //one mvp matrix and hence one descriptor set per frame in flight
    DescriptorSetAllocateInfo.pSetLayouts = vkRuntimeInfoP->descriptorSetLayoutP;
    vkRuntimeInfoP->descriptorSetsP = (VkDescriptorSet*)malloc(vkRuntimeInfoP->imagesInFlightCount * sizeof(VkDescriptorSet));
    for(uint32_t image = 0; image < vkRuntimeInfoP->imagesInFlightCount; image++) { //if the all generated sets need to have the same layout one needs to loop over the allocateDescriptorSets Function
        CHK_VK(vkAllocateDescriptorSets(vkRuntimeInfoP->device, &DescriptorSetAllocateInfo, &(vkRuntimeInfoP->descriptorSetsP[image])));
    }

}

void eng_createGraphicsPipeline(struct VulkanRuntimeInfo* vkRuntimeInfoP) {

    VkVertexInputAttributeDescription InputAttributeDescriptionArray[3];
    //Positions
    InputAttributeDescriptionArray[0].location = 0; //will be used for positions vec3 (location=0) in shader
    InputAttributeDescriptionArray[0].binding = 0;  //binding used to cross reference to the InputBindingDescription
    InputAttributeDescriptionArray[0].format = VK_FORMAT_R32G32B32_SFLOAT;  //is equivalent to vec3
    InputAttributeDescriptionArray[0].offset = 0;   //positions are at the start of our static buffer
    //Normals
    InputAttributeDescriptionArray[1].location = 1;
    InputAttributeDescriptionArray[1].binding = 0;
    InputAttributeDescriptionArray[1].format = VK_FORMAT_R32G32B32_SFLOAT;  //is equivalent to vec3
    InputAttributeDescriptionArray[1].offset = 4 * sizeof(float);
    //UVs
    InputAttributeDescriptionArray[2].location = 2;
    InputAttributeDescriptionArray[2].binding = 0;
    InputAttributeDescriptionArray[2].format = VK_FORMAT_R32G32_SFLOAT;  //is equivalent to vec2
    InputAttributeDescriptionArray[2].offset = 8 * sizeof(float);


    VkVertexInputBindingDescription InputBindingDescriptionArray[1];
    InputBindingDescriptionArray[0].binding = 0;
    InputBindingDescriptionArray[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;  //jump to next vertex for every new triangle in the index buffer, not every vertex
    InputBindingDescriptionArray[0].stride = sizeof(float) * 10;                 //stride is sizeof(vec4)*/


    //VertexInput
    VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateInfo = {0};
    PipelineVertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    PipelineVertexInputStateInfo.pNext = NULL;
    PipelineVertexInputStateInfo.vertexAttributeDescriptionCount = sizeof(InputAttributeDescriptionArray) / sizeof(InputAttributeDescriptionArray[0]);
    PipelineVertexInputStateInfo.vertexBindingDescriptionCount = sizeof(InputBindingDescriptionArray) / sizeof(InputBindingDescriptionArray[0]);
    //needs to be set if we supply vertex buffers to our shader
    PipelineVertexInputStateInfo.pVertexAttributeDescriptions = InputAttributeDescriptionArray;
    PipelineVertexInputStateInfo.pVertexBindingDescriptions = InputBindingDescriptionArray;

    VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyInfo = {0};
    PipelineInputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    PipelineInputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
    PipelineInputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    //Vertex Shader
    VkPipelineShaderStageCreateInfo VertexShaderStageCreateInfo = {0};
    VertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    VertexShaderStageCreateInfo.module = vkRuntimeInfoP->VertexShaderModule;
    VertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    VertexShaderStageCreateInfo.pName = "main";

    //Rasterizer
    //Scissor
    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = vkRuntimeInfoP->swapChainImageExtent;
    //Viewport
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    viewport.height = vkRuntimeInfoP->swapChainImageExtent.height;
    viewport.width = vkRuntimeInfoP->swapChainImageExtent.width;
    //ViewportInfo
    VkPipelineViewportStateCreateInfo PipelineViewportInfo = {0};
    PipelineViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    PipelineViewportInfo.viewportCount = 1;
    PipelineViewportInfo.scissorCount = 1;
    PipelineViewportInfo.pScissors = &scissor;
    PipelineViewportInfo.pViewports = &viewport;
    //RasterizerInfo
    VkPipelineRasterizationStateCreateInfo RasterizationInfo = {0};
    RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterizationInfo.depthClampEnable = VK_FALSE;
    RasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    RasterizationInfo.lineWidth = 1.0f;
    RasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    RasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    RasterizationInfo.depthBiasEnable = VK_FALSE;
    RasterizationInfo.depthBiasConstantFactor = 0.0f;
    RasterizationInfo.depthBiasClamp = 0.0f;
    RasterizationInfo.depthBiasSlopeFactor = 0.0f;
    //Multisampling
    VkPipelineMultisampleStateCreateInfo MultisampleInfo = {0};
    MultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultisampleInfo.sampleShadingEnable = VK_FALSE;
    MultisampleInfo.alphaToOneEnable = VK_FALSE;
    MultisampleInfo.minSampleShading = 1.0f;
    MultisampleInfo.pSampleMask = NULL;
    MultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    MultisampleInfo.sampleShadingEnable = VK_FALSE;
    MultisampleInfo.alphaToCoverageEnable = VK_FALSE;

    //Fragment
    VkPipelineShaderStageCreateInfo FragmentShaderStageCreateInfo = {0};
    FragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    FragmentShaderStageCreateInfo.module = vkRuntimeInfoP->FragmentShaderModule;
    FragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    FragmentShaderStageCreateInfo.pSpecializationInfo = NULL;
    FragmentShaderStageCreateInfo.pName = "main";

    //Blending
    VkPipelineColorBlendAttachmentState ColorBlendAttachment = {0};
    ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ColorBlendAttachment.blendEnable = VK_FALSE;
    ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    VkPipelineColorBlendStateCreateInfo ColorBlendInfo = {0};
    ColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendInfo.flags = 0;
    ColorBlendInfo.logicOp = VK_FALSE;
    ColorBlendInfo.attachmentCount = 1;
    ColorBlendInfo.pAttachments = &ColorBlendAttachment;
    ColorBlendInfo.blendConstants[0] = 0.0f;
    ColorBlendInfo.blendConstants[1] = 0.0f;
    ColorBlendInfo.blendConstants[2] = 0.0f;
    ColorBlendInfo.blendConstants[3] = 0.0f;

    //Pipeline Layout for Uniforms
    VkPipelineLayoutCreateInfo PipelineLayoutInfo = {0};
    PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutInfo.setLayoutCount = 1;
    PipelineLayoutInfo.pSetLayouts = vkRuntimeInfoP->descriptorSetLayoutP;
    PipelineLayoutInfo.pushConstantRangeCount = 0;
    PipelineLayoutInfo.pPushConstantRanges = NULL;

    CHK_VK(vkCreatePipelineLayout(vkRuntimeInfoP->device, &PipelineLayoutInfo, NULL, &(vkRuntimeInfoP->graphicsPipelineLayout)));

    //Assemble everything
    VkPipelineShaderStageCreateInfo Stages[2] = {
        VertexShaderStageCreateInfo,
        FragmentShaderStageCreateInfo
    };

    VkGraphicsPipelineCreateInfo PipelineInfo = {0};
    PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineInfo.stageCount = 2;
    PipelineInfo.pStages = Stages;
    PipelineInfo.pVertexInputState = &PipelineVertexInputStateInfo;
    PipelineInfo.pInputAssemblyState = &PipelineInputAssemblyInfo;
    PipelineInfo.pColorBlendState = &ColorBlendInfo;
    PipelineInfo.pDepthStencilState = NULL;
    PipelineInfo.pDynamicState = NULL;
    PipelineInfo.pMultisampleState = &MultisampleInfo;
    PipelineInfo.pRasterizationState = &RasterizationInfo;
    PipelineInfo.pTessellationState = NULL;
    PipelineInfo.pViewportState = &PipelineViewportInfo;

    PipelineInfo.layout = vkRuntimeInfoP->graphicsPipelineLayout;
    PipelineInfo.renderPass = vkRuntimeInfoP->renderPass;
    PipelineInfo.subpass = 0;
    CHK_VK(vkCreateGraphicsPipelines(vkRuntimeInfoP->device, VK_NULL_HANDLE, 1, &PipelineInfo, NULL, &(vkRuntimeInfoP->graphicsPipeline)));
}

void eng_createFramebuffers(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    vkRuntimeInfoP->FramebufferP = malloc(sizeof(VkFramebuffer) * vkRuntimeInfoP->imagesInFlightCount);
    for(uint32_t framebufferIdx = 0; framebufferIdx < vkRuntimeInfoP->imagesInFlightCount; framebufferIdx++) {
        VkFramebufferCreateInfo FramebufferInfo = {0};
        FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FramebufferInfo.renderPass = vkRuntimeInfoP->renderPass;
        FramebufferInfo.height = vkRuntimeInfoP->swapChainImageExtent.height;
        FramebufferInfo.width = vkRuntimeInfoP->swapChainImageExtent.width;
        FramebufferInfo.layers = 1;
        FramebufferInfo.attachmentCount = 1;
        FramebufferInfo.pAttachments = &(vkRuntimeInfoP->swapChainImageViewsP[framebufferIdx]);
        CHK_VK(vkCreateFramebuffer(vkRuntimeInfoP->device, &FramebufferInfo, NULL, &(vkRuntimeInfoP->FramebufferP[framebufferIdx])));
    }
}

void eng_createRenderCommandBuffers(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    vkRuntimeInfoP->CommandbufferP = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * vkRuntimeInfoP->imagesInFlightCount);

    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {0};
    CommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocateInfo.commandPool = vkRuntimeInfoP->commandPool;
    CommandBufferAllocateInfo.commandBufferCount = vkRuntimeInfoP->imagesInFlightCount;
    CommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    CHK_VK(vkAllocateCommandBuffers(vkRuntimeInfoP->device, &CommandBufferAllocateInfo, vkRuntimeInfoP->CommandbufferP));
    for(uint32_t CommandBufferIdx = 0; CommandBufferIdx < vkRuntimeInfoP->imagesInFlightCount; CommandBufferIdx++) {
        VkCommandBufferBeginInfo CommandBufferBeginInfo = {0};
        CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        CHK_VK(vkBeginCommandBuffer(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx], &CommandBufferBeginInfo));

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderPassBeginInfo RenderPassInfo = {0};
        RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassInfo.framebuffer = vkRuntimeInfoP->FramebufferP[CommandBufferIdx];
        RenderPassInfo.clearValueCount = 1;
        RenderPassInfo.pClearValues = &clearColor;
        RenderPassInfo.renderArea.extent = vkRuntimeInfoP->swapChainImageExtent;
        RenderPassInfo.renderArea.offset.x = 0;
        RenderPassInfo.renderArea.offset.y = 0;
        RenderPassInfo.renderPass = vkRuntimeInfoP->renderPass;
        vkCmdBeginRenderPass(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx], &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        for(size_t ObjectNum = 0; ObjectNum < scene1ObjectsDlP->itemcnt; ObjectNum++) {
            struct eng3dObject*  currentObjectP = &(scene1ObjectsDlP->items[ObjectNum]);
            VkDeviceSize* PNUBufferOffsetP = &(currentObjectP->PosNormUvInBufOffset);
            vkCmdBindVertexBuffers(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx], 0, 1, &(currentObjectP->vtxBufferHandle), PNUBufferOffsetP);
            vkCmdBindIndexBuffer(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx], currentObjectP->vtxBufferHandle, currentObjectP->IdxInBufOffset, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, vkRuntimeInfoP->graphicsPipelineLayout, 0, 1, &(vkRuntimeInfoP->descriptorSetsP[CommandBufferIdx]), 0, NULL);
            //vkCmdBindDescriptorSets(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx],VK_PIPELINE_BIND_POINT_GRAPHICS,vkRuntimeInfoP->graphicsPipelineLayout,0,1,vkRuntimeInfoP->descriptorSetsP,0,NULL);
            vkCmdBindPipeline(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, vkRuntimeInfoP->graphicsPipeline);
            vkCmdDrawIndexed(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx], currentObjectP->vertexCount, 1, 0, 0, 0);
        }
        vkCmdEndRenderPass(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx]);
        CHK_VK(vkEndCommandBuffer(vkRuntimeInfoP->CommandbufferP[CommandBufferIdx]));
    }
}

void eng_createSynchronizationPrimitives(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    //Create Semaphores to sync vkAcquireNextImageKHR with start of buffer execution and
    //sync end of buffer executing with vkQueuePresentKHR
    VkSemaphoreCreateInfo SemaphoreInfo = {0};
    SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    //Create Fences to sync cpu gpu
    VkFenceCreateInfo FenceInfo = {0};
    FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkRuntimeInfoP->imageAvailableSemaphoreP = (VkSemaphore*)malloc(sizeof(VkSemaphore) * vkRuntimeInfoP->imagesInFlightCount);
    vkRuntimeInfoP->renderFinishedSemaphoreP = (VkSemaphore*)malloc(sizeof(VkSemaphore) * vkRuntimeInfoP->imagesInFlightCount);
    vkRuntimeInfoP->ImageAlreadyProcessingFenceP = (VkFence*)malloc(sizeof(VkFence) * vkRuntimeInfoP->imagesInFlightCount);

    for(uint32_t FrameIdx = 0; FrameIdx < vkRuntimeInfoP->imagesInFlightCount; FrameIdx++) {
        CHK_VK(vkCreateFence(vkRuntimeInfoP->device, &FenceInfo, NULL, (vkRuntimeInfoP->ImageAlreadyProcessingFenceP) + FrameIdx));
        CHK_VK(vkCreateSemaphore(vkRuntimeInfoP->device, &SemaphoreInfo, NULL, (vkRuntimeInfoP->imageAvailableSemaphoreP) + FrameIdx));
        CHK_VK(vkCreateSemaphore(vkRuntimeInfoP->device, &SemaphoreInfo, NULL, (vkRuntimeInfoP->renderFinishedSemaphoreP) + FrameIdx));
    }

}

void error_callback(int code, const char* description) {
    dprintf(DBGT_ERROR, "Error in glfw code: %d, \n String %s", code, description);
}

void eng_draw(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    static float angle = 0;
    static uint32_t nextImageIdx = 0;
    //Work
    if(vkWaitForFences(vkRuntimeInfoP->device, 1, (vkRuntimeInfoP->ImageAlreadyProcessingFenceP) + nextImageIdx, VK_TRUE, UINT64_MAX)) {
        dprintf(DBGT_ERROR, "Waiting for fence timeout");
        exit(1);
    }
    CHK_VK(vkResetFences(vkRuntimeInfoP->device, 1, (vkRuntimeInfoP->ImageAlreadyProcessingFenceP) + nextImageIdx));

    //create new MVP
    //Model
    mat4x4 MMatrix;
    mat4x4 MVPMatrix;
    mat4x4_identity(MMatrix);
    mat4x4_rotate_X(MVPMatrix, MMatrix, angle);
    angle += 0.01f;

    //View
    mat4x4 VMatrix;
    vec3 eye = {2.0f, 0.0f, 2.0f};
    vec3 center = {0.0f, 0.0f, 0.0f};
    vec3 up = {0.0f, 0.0f, 1.0f};
    mat4x4_look_at(VMatrix, eye, center, up);
    /*
    for(int row=0;row<4;row++){
        for(int col=0;col<4;col++){
            printf("%f \t",VMatrix[row][col]);
        }
        printf("\n");
    }
    printf("\n");*/
    mat4x4_mul(MVPMatrix, VMatrix, MVPMatrix);

    //projection
    mat4x4 PMatrix;
    float aspRatio = ((float)vkRuntimeInfoP->swapChainImageExtent.width) / vkRuntimeInfoP->swapChainImageExtent.height;
    mat4x4_perspective(PMatrix, 1.6f, aspRatio, 0.1f, 10.0f);
    mat4x4_mul(MVPMatrix, PMatrix, MVPMatrix);

    //copy in buffer
    void* mappedUniformSliceP;
    CHK_VK(vkMapMemory(vkRuntimeInfoP->device, vkRuntimeInfoP->FastUpdateUniformAllocP->Memory, sizeof(mat4x4)*nextImageIdx, sizeof(mat4x4), 0, &mappedUniformSliceP));
    memcpy(mappedUniformSliceP, &(MVPMatrix[0][0]), sizeof(mat4x4));
    //TODO flush memory
    vkUnmapMemory(vkRuntimeInfoP->device, vkRuntimeInfoP->FastUpdateUniformAllocP->Memory);

    //Get next image from swapChain
    CHK_VK(vkAcquireNextImageKHR(vkRuntimeInfoP->device, vkRuntimeInfoP->swapChain, UINT64_MAX, (vkRuntimeInfoP->imageAvailableSemaphoreP)[nextImageIdx], VK_NULL_HANDLE, &nextImageIdx));

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo SubmitInfo = {0};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitDstStageMask = waitStages;
    SubmitInfo.pWaitSemaphores = (vkRuntimeInfoP->imageAvailableSemaphoreP) + nextImageIdx;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = (vkRuntimeInfoP->CommandbufferP) + nextImageIdx;
    SubmitInfo.signalSemaphoreCount = 1;
    SubmitInfo.pSignalSemaphores = (vkRuntimeInfoP->renderFinishedSemaphoreP) + nextImageIdx;

    CHK_VK(vkQueueSubmit(vkRuntimeInfoP->graphics_queue, 1, &SubmitInfo, vkRuntimeInfoP->ImageAlreadyProcessingFenceP[nextImageIdx]));
    VkPresentInfoKHR PresentInfo = {0};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = (vkRuntimeInfoP->renderFinishedSemaphoreP) + nextImageIdx;
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &(vkRuntimeInfoP->swapChain);
    PresentInfo.pImageIndices = &nextImageIdx;
    CHK_VK(vkQueuePresentKHR(vkRuntimeInfoP->graphics_queue, &PresentInfo));

    nextImageIdx += 1;
    nextImageIdx %= vkRuntimeInfoP->imagesInFlightCount;
}

void eng_writeDescriptorSets(struct VulkanRuntimeInfo* vkRuntimeInfoP) {
    for(uint32_t image = 0; image < vkRuntimeInfoP->imagesInFlightCount; image++) {
        //Write descriptor set
        VkDescriptorBufferInfo BufferInfo;
        BufferInfo.buffer = vkRuntimeInfoP->FastUpdateUniformAllocP->BufAllocInfoDlP->items[0].BufferHandle;
        BufferInfo.offset = sizeof(mat4x4) * image;
        BufferInfo.range = sizeof(mat4x4);

        VkWriteDescriptorSet WriteDescriptorSet = {0};
        WriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        WriteDescriptorSet.descriptorCount = 1;
        WriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        WriteDescriptorSet.dstBinding = 0;
        WriteDescriptorSet.dstSet = vkRuntimeInfoP->descriptorSetsP[image];
        WriteDescriptorSet.pBufferInfo = &BufferInfo;
        vkUpdateDescriptorSets(vkRuntimeInfoP->device, 1, &WriteDescriptorSet, 0, NULL);
    }
}

int main(int argc, char** argv) {
    (void)argc; //avoid unused arguments warning
    (void)argv;

    glfwInit();
    glfwSetErrorCallback(error_callback);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
#ifdef DEBUG
    struct xmlTreeElement* eng_setupxmlP = eng_get_eng_setupxml("./res/vk_setup.xml", 1);
#else
    struct xmlTreeElement* eng_setupxmlP = eng_get_eng_setupxml("./res/vk_setup.xml", 0);
#endif
    printXMLsubelements(eng_setupxmlP);
    xmlTreeElement* applicationNameXmlElmntP = getFirstSubelementWithASCII(eng_setupxmlP, "ApplicationName", NULL, NULL, xmltype_tag, 1);
    char* applicationNameCharP = Dl_utf32Char_toStringAlloc(getNthChildWithType(applicationNameXmlElmntP, 0, xmltype_chardata)->charData);
    struct VulkanRuntimeInfo engVkRuntimeInfo = {0};
    engVkRuntimeInfo.mainWindowP = glfwCreateWindow(1920, 1080, applicationNameCharP, NULL, NULL);
    free(applicationNameCharP);

    if(GLFW_FALSE == glfwVulkanSupported()) {
        dprintf(DBGT_ERROR, "Vulkan not supported on this device");
        exit(1);
    }


    eng_createInstance(&engVkRuntimeInfo, eng_setupxmlP);
    uint8_t* deviceRankingP = eng_vulkan_generate_device_ranking(&engVkRuntimeInfo, eng_setupxmlP);
    eng_createSurface(&engVkRuntimeInfo);
    eng_createDevice(&engVkRuntimeInfo, deviceRankingP);
    eng_createSwapChain(&engVkRuntimeInfo);
    eng_createImageViews(&engVkRuntimeInfo);                                //depends on eng_createSwapChain
    eng_createShaderModule(&engVkRuntimeInfo, "./res/shader1.xml");         //depends on eng_createDevice
    eng_createRenderPass(&engVkRuntimeInfo);                                //depends on eng_createSwapChain
    eng_createDescriptorPoolAndSets(&engVkRuntimeInfo);                     //depends on eng_createSwapChain
    eng_createGraphicsPipeline(&engVkRuntimeInfo);                          //depends on eng_createShaderModule and eng_createImageViews and eng_createDescriptorPoolAndSets
    eng_createFramebuffers(&engVkRuntimeInfo);                              //depends on eng_createRenderPass   and eng_createImageViews
    eng_createCommandPool(&engVkRuntimeInfo);                               //depends on eng_createDevice
    Dl_modelPathAndName* modelPathAndNameDlP = Dl_modelPathAndName_alloc(1, NULL);
    modelPathAndNameDlP->items[0].pathString = Dl_utf32Char_fromString("./res/cube.dae");
    modelPathAndNameDlP->items[0].modelName = Dl_utf32Char_fromString("Cube-mesh");
    //modelPathAndNameDlP->items[1].pathString=Dl_utf32Char_fromString("./res/cube_jank.dae");
    //modelPathAndNameDlP->items[1].modelName =Dl_utf32Char_fromString("Cube-mesh");

    eng_load_static_models(&engVkRuntimeInfo, modelPathAndNameDlP);         //depends on eng_createCommandPool and creates vertex buffer
    eng_writeDescriptorSets(&engVkRuntimeInfo);                             //depends on eng_load_static_models
    eng_createRenderCommandBuffers(&engVkRuntimeInfo);                      //depends on eng_createCommandPool and eng_createFramebuffers and eng_load_static_models and createPipeline
    eng_createSynchronizationPrimitives(&engVkRuntimeInfo);
    dprintf(DBGT_INFO, "Got here");
    while (!glfwWindowShouldClose(engVkRuntimeInfo.mainWindowP)) {
        eng_draw(&engVkRuntimeInfo);
        glfwPollEvents();
    }
}
