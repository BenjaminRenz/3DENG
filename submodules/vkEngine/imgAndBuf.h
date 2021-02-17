#ifndef IMGANDBUF_H_INCLUDED
#define IMGANDBUF_H_INCLUDED
#include "vulkan/vulkan.h"
#include "vkEngine/core.h"
#include "dynList/dynList.h"

struct VulkanRuntimeInfo;

struct eng_PerTexData{
    //this field must be filled before image handle creation
    VkExtent3D              initContentExtentInPx;
    VkFormat                initFormat;
    VkImageTiling           initTiling;
    VkImageUsageFlags       initUsage;
    //there fields must be filled before view and sampler creation

    //the fields below are filled while creating the image handle
    VkImage                 ImageHandle;
    VkDeviceSize            reqAllocSize;
    //the fields below are only filled while allocating and binding the image to memory
    VkDeviceSize            OffsetInMemoryInBytes;
};
DlTypedef_plain(PerTexData,struct eng_PerTexData);

struct eng_PerBufData{
    //this field must be filled before buffer handle creation
    VkDeviceSize            initContentSizeInBytes;
    VkBufferUsageFlags      initUsage;
    //the fields below are while creating the buffer handle
    VkBuffer                BufferHandle;
    VkDeviceSize            reqAllocSize;
    //the fields below are only filled while allocating and binding the buffer to memory
    VkDeviceSize            OffsetInMemoryInBytes;
};
DlTypedef_plain(PerBufData,struct eng_PerBufData);

struct eng_AllocBlock{
    VkDeviceSize            alignment;
    uint32_t                memoryAllowedTypeIdxBitmask;
    uint32_t                memorySelectedTypeIdx;
    VkDeviceMemory          Memory;
    VkDeviceSize            totalAllocSize;
    VkMemoryPropertyFlags   MemoryFlags;
    Dl_PerBufData*     BufAllocInfoDlP;        //either one of BufAllocInfoDlP or TexAllocInfoDlP is a nullptr
    Dl_PerTexData*     TexAllocInfoDlP;
};

void eng_AllocBlock_createHandlesAndGetMemReq(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP);
void eng_AllocBlock_alignAndCalcSizeAndOffsets(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP);
void eng_AllocBlock_allocAndBindMem(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP);
void eng_AllocBlock_setFastDevLocalAlloc(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP,uint32_t* optResMemPropBitsP);
void eng_AllocBlock_setStagingAlloc(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP,uint32_t* optResMemPropBitsP);
void eng_AllocBlock_setUniformAlloc(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP,uint32_t* optResMemPropBitsP);




#endif // IMGANDBUF_H_INCLUDED
