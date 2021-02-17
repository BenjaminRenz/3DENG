#include "bmpLoader/bmpLoader.h"
#include "debugPrint/debugPrint.h"
#include <stdlib.h> //for abs and fopen
#include <string.h> //for memcpy


//since bitmap is little endian we might need to convert it to our system's endianess
uint32_t _bmpLoader_uint32_t_from_le_file(FILE* file, int bigEndianFlag) {
    uint32_t data = 0;
    if(fread(&data, 4, 1, file) != 1) {
        dprintf(DBGT_ERROR, "unexpected end of bmp file while reading uint32_t");
        exit(1);
    }
    if(bigEndianFlag) {
        data = ((data >> 24) & 0x000000ff) |
               ((data >> 8)  & 0x0000ff00) |
               ((data << 8)  & 0x00ff0000) |
               ((data << 24) & 0xff000000); //big endian
    }
    return data;
}

uint16_t _bmpLoader_uint16_t_from_le_file(FILE* FileP, int bigEndianFlag) {
    uint16_t data = 0;
    if(fread(&data, 2, 1, FileP) != 1) {
        dprintf(DBGT_ERROR, "unexpected end of bmp file while reading uint16_t");
        exit(1);
    }
    if(bigEndianFlag) {
        data = ((data << 8) & 0xff00) | ((data >> 8) & 0x00ff);
    }
    return data;
}

//output format can be specified as "ARGB" or "BGGR" or "RGB0" for padding
struct ImageData bmpLoader_load(char* filepathString, char* outputFormatString, char pack32toogle) {
    //argument error handling
    if(!filepathString) {
        dprintf(DBGT_ERROR, "no filepath specified");
        exit(1);
    }
    if(!outputFormatString) {
        dprintf(DBGT_ERROR, "no format string specified");
        exit(1);
    }
    unsigned char length = 0;
    while(outputFormatString[length]) {
        length++;
    }
    if(length != 4) {
        dprintf(DBGT_ERROR, "too short or too long format string specified");
        exit(1);
    }
    char* editableFormatString = (char*)malloc(4 * sizeof(char));
    struct ImageData OutputData;
    FILE* FileP = fopen(filepathString, "rb");
    if(FileP == NULL) {
        dprintf(DBGT_ERROR, "No file under path %s found", filepathString);
        exit(1);
    }

    fseek(FileP, 0, SEEK_SET);   //Jump to beginning of file
    int bigEndianFlag = 0;
    uint16_t bfType = _bmpLoader_uint16_t_from_le_file(FileP, bigEndianFlag);
    if(bfType == 0x4D42) {
        dprintf(DBGT_INFO, "Host is little endian");
    } else if(bfType == 0x424D) {
        dprintf(DBGT_INFO, "Host is big endian");
        bigEndianFlag = 1;
    } else {
        dprintf(DBGT_ERROR, "File %s is not an BMP", filepathString);
        exit(1);
    }
    //handle the case there pack32toodle is not set and we need to output independent of system endianness
    if(!pack32toogle && bigEndianFlag) {
        //invert string order
        editableFormatString[3] = outputFormatString[0];
        editableFormatString[2] = outputFormatString[1];
        editableFormatString[1] = outputFormatString[2];
        editableFormatString[0] = outputFormatString[3];
    } else {
        memcpy(editableFormatString, outputFormatString, 4 * sizeof(char));
    }

    fseek(FileP, 10, SEEK_SET); //Jump to 10bytes after the start of the file
    uint32_t BitmapOffset = _bmpLoader_uint32_t_from_le_file(FileP, bigEndianFlag);
    uint32_t biWidth = _bmpLoader_uint32_t_from_le_file(FileP, bigEndianFlag);
    if(biWidth != 124 && biWidth != 108 && biWidth != 40) {
        dprintf(DBGT_ERROR, "BitmapHeader is not V5, V4 or V3, size was: %d", biWidth);
        exit(1);
    }
    uint32_t width = _bmpLoader_uint32_t_from_le_file(FileP, bigEndianFlag);
    OutputData.width = width;
    uint32_t tempHeight = _bmpLoader_uint32_t_from_le_file(FileP, bigEndianFlag);
    int32_t height = *((int32_t*)(&tempHeight)); //Important leave height as int32_t, because negative height means top down BMP
    uint32_t absHeight = abs(height);
    OutputData.height = absHeight;
    dprintf(DBGT_INFO, "BMP resolution %d x %d", width, abs(height));
    uint16_t biPlanes = _bmpLoader_uint16_t_from_le_file(FileP, bigEndianFlag);
    if(biPlanes != 1) {
        dprintf(DBGT_ERROR, "Unsupported plane count");
        exit(1);
    }
    uint16_t BitmapColorDepth = _bmpLoader_uint16_t_from_le_file(FileP, bigEndianFlag);
    unsigned char ColorsPerPixel = BitmapColorDepth / 8;
    switch(ColorsPerPixel) {
    case 3:
        dprintf(DBGT_INFO, "RGB data");
        break;
    case 4:
        dprintf(DBGT_INFO, "RGBA data");
        break;
    default:
        dprintf(DBGT_ERROR, "Unsupported ColorDepth");
        exit(1);
        break;
    }

    uint32_t sizeOfUnalignedRowInBytes = ColorsPerPixel * width;
    uint32_t sizeOfAlignedRowInBytes = sizeOfUnalignedRowInBytes;
    //Bitmap data rows are padded to align with 4bytes boundary
    if(sizeOfAlignedRowInBytes % 4) {
        sizeOfAlignedRowInBytes += (4 - (sizeOfAlignedRowInBytes % 4));
    }
    uint32_t BitmapSizeCalculated = sizeOfAlignedRowInBytes * absHeight;

    uint32_t BitmapCompression = _bmpLoader_uint32_t_from_le_file(FileP, bigEndianFlag);
    switch(BitmapCompression) {
    case 0:
        dprintf(DBGT_ERROR, "Compression type: none/BI_RGB");
        break;
    case 3:
        dprintf(DBGT_INFO, "Compression type: Bitfields/BI_BITFIELDS");
        break;
    case 6:
        dprintf(DBGT_INFO, "Compression type: Bitfields/BI_ALPHABITFIELDS");
        break;
    default:
        dprintf(DBGT_ERROR, "Unsupported compression");
        exit(1);
    }
    uint32_t BitmapImageSize = _bmpLoader_uint32_t_from_le_file(FileP, bigEndianFlag);
    if(BitmapImageSize && (BitmapImageSize != BitmapSizeCalculated)) {
        dprintf(DBGT_ERROR, "image size %d not matching with predicted size %d", BitmapImageSize, BitmapSizeCalculated);
        exit(1);
    }
    fseek(FileP, 8, SEEK_CUR);       //skip over PixelsPerMeter
    uint32_t BitmapColorsInPalette = _bmpLoader_uint32_t_from_le_file(FileP, bigEndianFlag);
    if(BitmapColorsInPalette) {
        dprintf(DBGT_ERROR, "Color tables are not supported");
        exit(1);
    }
    fseek(FileP, 4, SEEK_CUR);   //skip over important color count
    char channelBitmaskOrder[4] = {'R', 'G', 'B', 'A'}; //in which order the channel masks occur in the bmp file
    char colorChannelSrcMask[4] = {'B', 'G', 'R', 0}; //initialize for compression==0 (RGB)
    char colorChannelDstMask[4] = {0};
    char dstWantsAlphaChannelFlag = 0;
    for(int OutputFormatCharIdx = 0; OutputFormatCharIdx < 4; OutputFormatCharIdx++) {
        char ChannelNameChar = editableFormatString[OutputFormatCharIdx];
        switch(ChannelNameChar) {
        case 'A':
            dstWantsAlphaChannelFlag = 1;
        //fallthrough
        case 'R':
        case 'G':
        case 'B':
            colorChannelDstMask[OutputFormatCharIdx] = ChannelNameChar;
            break;
        case '0':
            break;
        default:
            dprintf(DBGT_ERROR, "invalid channel name in outputFormartString");
            exit(1);
        }
    }

    //Parse Bitfields if applicable
    char numberOfBitfields=0;
    if(BitmapCompression==3){
        if(biWidth==40){
            numberOfBitfields=3;    //bitmap v3 only supports RGB bitmasks
        }else{
            numberOfBitfields=4;
        }
    }
    if(BitmapCompression==6){
        numberOfBitfields=4;
    }
    for(int channelIdx = 0; channelIdx < numberOfBitfields; channelIdx++) {
        uint32_t color_channel_mask = _bmpLoader_uint32_t_from_le_file(FileP, bigEndianFlag);
        switch(color_channel_mask) {   //read shift value for channelIdx
        case 0xFF000000:
            colorChannelSrcMask[3] = channelBitmaskOrder[channelIdx];
            break;
        case 0x00FF0000:
            colorChannelSrcMask[2] = channelBitmaskOrder[channelIdx];
            break;
        case 0x0000FF00:
            colorChannelSrcMask[1] = channelBitmaskOrder[channelIdx];
            break;
        case 0x000000FF:
            colorChannelSrcMask[0] = channelBitmaskOrder[channelIdx];
            break;
        case 0x00:      //for xRGB, where the alpha channel is ignored
            break;
        default:
            dprintf(DBGT_ERROR, "Error: Invalid bitmask,%x", color_channel_mask);
            exit(1);
            break;
        }
    }

    uint8_t* imageDataTempP = malloc(BitmapSizeCalculated);
    uint32_t* imageOutputP = malloc(sizeof(uint32_t) * width * absHeight);
    dprintf(DBGT_INFO, "Info: BMP data offset %d", BitmapOffset);
    fseek(FileP, BitmapOffset, SEEK_SET);   //jump to pixel data
    if(fread(imageDataTempP, BitmapSizeCalculated, 1, FileP) == 0) {
        dprintf(DBGT_ERROR, "Reading image data failed");
        exit(1);
    }

    uint32_t readBufferOffset = 0;
    for(uint32_t outputColumnPos = 0; outputColumnPos < absHeight; outputColumnPos++) {
        for(uint32_t outputRowPos = 0; outputRowPos < width; outputRowPos++) {
            uint32_t resultPixelData = 0;
            char alphaChannelNeedsFillInDstFlag = dstWantsAlphaChannelFlag;
            for(unsigned char srcColorIdx = 0; srcColorIdx < ColorsPerPixel; srcColorIdx++) {
                uint32_t currentColorData = imageDataTempP[readBufferOffset++];
                unsigned char srcColorName = colorChannelSrcMask[srcColorIdx];
                if(srcColorName == 'A') {
                    alphaChannelNeedsFillInDstFlag = 0;
                }
                for(unsigned char colorDstIdx = 0; colorDstIdx < (unsigned char)(sizeof(colorChannelDstMask) / sizeof(colorChannelDstMask[0])); colorDstIdx++) {
                    if(srcColorName == colorChannelDstMask[colorDstIdx]) {
                        resultPixelData |= (currentColorData << (colorDstIdx * 8));
                    }
                }
            }
            //alpha needs to be filled with 0xff, so that images which do not provide transparency information still show up when dst has an alpha channel
            if(alphaChannelNeedsFillInDstFlag) {
                for(unsigned char colorDstIdx = 0; colorDstIdx < (unsigned char)(sizeof(colorChannelDstMask) / sizeof(colorChannelDstMask[0])); colorDstIdx++) {
                    if('A' == colorChannelDstMask[colorDstIdx]) {
                        resultPixelData |= (0xFF << (colorDstIdx * 8));
                    }
                }
            }
            //top down or bottom up image
            if(height < 0) {
                imageOutputP[((absHeight - 1) - outputColumnPos) * width + outputRowPos] = resultPixelData;
            } else {
                imageOutputP[outputColumnPos * width + outputRowPos] = resultPixelData;
            }
            //realign at end of row
            if((readBufferOffset % sizeOfAlignedRowInBytes) >= sizeOfUnalignedRowInBytes) {
                readBufferOffset += sizeOfAlignedRowInBytes - sizeOfUnalignedRowInBytes;
            }
        }
    }
    free(imageDataTempP);
    fclose(FileP);
    OutputData.dataP = imageOutputP;
    return OutputData;
}
