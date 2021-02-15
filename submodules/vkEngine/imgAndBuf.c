#include "vkEngine/imgAndBuf.h"
#include "mathHelper/mathHelper.h"

//ToDo do actual memory usage calculations for each heap
int32_t _eng_Memory_findBestType(struct VulkanRuntimeInfo* vkRuntimeInfoP,uint32_t supportedMemTypes,VkMemoryPropertyFlags reqProp, VkMemoryPropertyFlags uprankProp, VkMemoryPropertyFlags downrankProp,VkMemoryPropertyFlags* ReturnPropP, VkDeviceSize minsize){
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
        uint32_t MemoryTypeBit=(1<<MemoryTypeIdx);
        if(!(MemoryTypeBit&supportedMemTypes)){
            continue;
        }
        //Check if memory has all requiriedProperty bits set
        if((DeviceMemProperties.memoryTypes[MemoryTypeIdx].propertyFlags&reqProp)!=reqProp){
            continue;
        }

        currentRank+=countBitsInUint32(DeviceMemProperties.memoryTypes[MemoryTypeIdx].propertyFlags&uprankProp);      //Uprank memory that has VkMemoryPropertyFlags uprankProp set
        currentRank-=countBitsInUint32(DeviceMemProperties.memoryTypes[MemoryTypeIdx].propertyFlags&downrankProp);
        if(currentRank>bestRanking){
            bestRanking=currentRank;
            bestRankingMemoryTypeIdx=(int32_t)MemoryTypeIdx;//we don't expect memory types over 2^31, so use the extra bit for error handling
            if(ReturnPropP){
                *ReturnPropP=DeviceMemProperties.memoryTypes[MemoryTypeIdx].propertyFlags;
            }
        }
    }
    if(!bestRanking){
        dprintf(DBGT_ERROR,"No suitable memory type found");
        exit(1);
    }
    return bestRankingMemoryTypeIdx;
}

void eng_AllocBlock_createHandlesAndGetMemReq(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP){
    AllocBlockP->memoryAllowedTypeIdxBitmask=0xffffffff;
    if(AllocBlockP->BufAllocInfoDlP){
        if(AllocBlockP->TexAllocInfoDlP){
            dprintf(DBGT_ERROR,"mixed allocations of buffers and images are not supported");
            exit(1);
        }
        for(size_t bufferIdx = 0; bufferIdx < AllocBlockP->BufAllocInfoDlP->itemcnt; bufferIdx++){
            VkBufferCreateInfo BufferCreateInfo={0};
            BufferCreateInfo.sType                  = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            BufferCreateInfo.queueFamilyIndexCount  = 1;
            BufferCreateInfo.pQueueFamilyIndices    = &(vkRuntimeInfoP->graphics_queue_family_idx);
            BufferCreateInfo.sharingMode            = VK_SHARING_MODE_EXCLUSIVE;
            BufferCreateInfo.usage                  = AllocBlockP->BufAllocInfoDlP->items[bufferIdx].initUsage;
            BufferCreateInfo.size                   = AllocBlockP->BufAllocInfoDlP->items[bufferIdx].initContentSizeInBytes;
            CHK_VK(vkCreateBuffer(vkRuntimeInfoP->device,&BufferCreateInfo,NULL,&(AllocBlockP->BufAllocInfoDlP->items[bufferIdx].BufferHandle)));
            VkMemoryRequirements VkMemReq;
            vkGetBufferMemoryRequirements(vkRuntimeInfoP->device,
                                          AllocBlockP->BufAllocInfoDlP->items[bufferIdx].BufferHandle,
                                          &VkMemReq);
            AllocBlockP->BufAllocInfoDlP->items[bufferIdx].reqAllocSize=VkMemReq.size;
            AllocBlockP->alignment=max_uint32(AllocBlockP->alignment,VkMemReq.alignment);
            AllocBlockP->memoryAllowedTypeIdxBitmask&=VkMemReq.memoryTypeBits;
        }
    }else if(AllocBlockP->TexAllocInfoDlP){
        for(size_t texIdx = 0; texIdx < AllocBlockP->TexAllocInfoDlP->itemcnt; texIdx++){
            VkImageCreateInfo ImageInfo={0};
            ImageInfo.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            ImageInfo.arrayLayers=1;
            ImageInfo.extent=AllocBlockP->TexAllocInfoDlP->items[texIdx].initContentExtentInPx;
            ImageInfo.format=AllocBlockP->TexAllocInfoDlP->items[texIdx].initFormat;
            ImageInfo.imageType=VK_IMAGE_TYPE_2D;
            ImageInfo.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            ImageInfo.mipLevels=1;
            ImageInfo.queueFamilyIndexCount=1;
            ImageInfo.pQueueFamilyIndices=&(vkRuntimeInfoP->graphics_queue_family_idx);
            ImageInfo.samples=VK_SAMPLE_COUNT_1_BIT;
            ImageInfo.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
            ImageInfo.tiling=AllocBlockP->TexAllocInfoDlP->items[texIdx].initTiling;
            ImageInfo.usage= AllocBlockP->TexAllocInfoDlP->items[texIdx].initUsage;
            VkMemoryRequirements VkMemReq;
            vkCreateImage(vkRuntimeInfoP->device,&ImageInfo,NULL,&(AllocBlockP->TexAllocInfoDlP->items[texIdx].ImageHandle));
            vkGetImageMemoryRequirements(vkRuntimeInfoP->device,
                                         AllocBlockP->TexAllocInfoDlP->items[texIdx].ImageHandle,
                                         &VkMemReq);
            AllocBlockP->TexAllocInfoDlP->items[texIdx].reqAllocSize=VkMemReq.size;
            AllocBlockP->alignment=max_uint32(AllocBlockP->alignment,VkMemReq.alignment);
            AllocBlockP->memoryAllowedTypeIdxBitmask&=VkMemReq.memoryTypeBits;
        }
    }
}

void eng_AllocBlock_alignAndCalcSizeAndOffsets(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP){
    if(AllocBlockP->BufAllocInfoDlP){
        for(size_t bufferIdx=0;bufferIdx<AllocBlockP->BufAllocInfoDlP->itemcnt;bufferIdx++){
            //align the start of the buffer
            VkDeviceSize LastBufferAlignmentOvershoot=(AllocBlockP->totalAllocSize)%(AllocBlockP->alignment);
            if(LastBufferAlignmentOvershoot){
                (AllocBlockP->totalAllocSize)+=(AllocBlockP->alignment)-LastBufferAlignmentOvershoot;
            }
            //store the memory offset of the buffer inside the potentially allocated memory block
            AllocBlockP->BufAllocInfoDlP->items[bufferIdx].OffsetInMemoryInBytes=(AllocBlockP->totalAllocSize);
            //account for the size of the object itself
            (AllocBlockP->totalAllocSize)+=AllocBlockP->BufAllocInfoDlP->items[bufferIdx].reqAllocSize;
        }
    }else if(AllocBlockP->TexAllocInfoDlP){
        //TODO account for imageBufferGranularity if image resources are changing from linear to optimal and at the
        //border between buffer an images
        //vkRuntimeInfoP->PhysDevPropP->limits->bufferImageGranularity
        for(size_t texIdx=0; texIdx<AllocBlockP->TexAllocInfoDlP->itemcnt; texIdx++){
            //align the start of the image
            VkDeviceSize LastBufferAlignmentOvershoot=(AllocBlockP->totalAllocSize)%(AllocBlockP->alignment);
            if(LastBufferAlignmentOvershoot){
                (AllocBlockP->totalAllocSize)+=(AllocBlockP->alignment)-LastBufferAlignmentOvershoot;
            }
            //store the memory offset of the image inside the potentially allocated memory block
            AllocBlockP->TexAllocInfoDlP->items[texIdx].OffsetInMemoryInBytes=(AllocBlockP->totalAllocSize);
            //account for the size of the object itself
            (AllocBlockP->totalAllocSize)+=AllocBlockP->TexAllocInfoDlP->items[texIdx].reqAllocSize;
        }
    }
}

void eng_AllocBlock_setFastDevLocalAlloc(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP,uint32_t* optResMemPropBitsP){
    AllocBlockP->memorySelectedTypeIdx=_eng_Memory_findBestType(vkRuntimeInfoP,
                                                                AllocBlockP->memoryAllowedTypeIdxBitmask,
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,     //force device local
                                                                0,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,     //downrank host visible memory
                                                                optResMemPropBitsP,                         //to check which memory was actually assigned
                                                                AllocBlockP->totalAllocSize);
}

void eng_AllocBlock_setStagingAlloc(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP,uint32_t* optResMemPropBitsP){
    AllocBlockP->memorySelectedTypeIdx=_eng_Memory_findBestType(vkRuntimeInfoP,
                                                                AllocBlockP->memoryAllowedTypeIdxBitmask,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,     //force device local
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                0,
                                                                optResMemPropBitsP,                         //to check which memory was actually assigned
                                                                AllocBlockP->totalAllocSize);
}

void eng_AllocBlock_setUniformAlloc(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP,uint32_t* optResMemPropBitsP){
    AllocBlockP->memorySelectedTypeIdx=_eng_Memory_findBestType(vkRuntimeInfoP,
                                                                AllocBlockP->memoryAllowedTypeIdxBitmask,
                                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, //Memory needs to be Host visible
                                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                                                                optResMemPropBitsP,                         //to check which memory was actually assigned
                                                                AllocBlockP->totalAllocSize);
}

void eng_AllocBlock_allocAndBindMem(struct VulkanRuntimeInfo* vkRuntimeInfoP,struct eng_AllocBlock* AllocBlockP){
    if(!AllocBlockP->totalAllocSize){
        dprintf(DBGT_ERROR,"Zero allocation size");
        exit(1);
    }
    VkMemoryAllocateInfo MemAllocInfo={0};
    MemAllocInfo.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemAllocInfo.allocationSize=AllocBlockP->totalAllocSize;
    MemAllocInfo.memoryTypeIndex=AllocBlockP->memorySelectedTypeIdx;
    CHK_VK(vkAllocateMemory(vkRuntimeInfoP->device,&MemAllocInfo,NULL,&(AllocBlockP->Memory)));
    if(AllocBlockP->BufAllocInfoDlP){
        for(size_t bufIdx=0;bufIdx<AllocBlockP->BufAllocInfoDlP->itemcnt;bufIdx++){
            CHK_VK(vkBindBufferMemory(vkRuntimeInfoP->device,
                                      AllocBlockP->BufAllocInfoDlP->items[bufIdx].BufferHandle,
                                      AllocBlockP->Memory,
                                      AllocBlockP->BufAllocInfoDlP->items[bufIdx].OffsetInMemoryInBytes
                                      ));
        }
    }else if(AllocBlockP->TexAllocInfoDlP){
        //TODO account for imageBufferGranularity if image resources are changing from linear to optimal and at the
        //border between buffer an images
        //vkRuntimeInfoP->PhysDevPropP->limits->bufferImageGranularity
        for(size_t texIdx = 0; texIdx < AllocBlockP->TexAllocInfoDlP->itemcnt; texIdx++){
            CHK_VK(vkBindImageMemory(vkRuntimeInfoP->device,
                                     AllocBlockP->TexAllocInfoDlP->items[texIdx].ImageHandle,
                                     AllocBlockP->Memory,
                                     AllocBlockP->TexAllocInfoDlP->items[texIdx].OffsetInMemoryInBytes
                                     ));
        }
    }
}
