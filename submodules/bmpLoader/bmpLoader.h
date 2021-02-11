#ifndef BMPLOADER_H_INCLUDED
#define BMPLOADER_H_INCLUDED
#include <stdint.h>


struct TextureData{
    uint32_t width;
    uint32_t height;
    uint32_t* dataP;
};

/** \brief reads .bmp image into memory with no compression or bitfields for RGB or RGBA images
 *         When outputFormartString requires an alpha channel while the read image which does not provide one,
 *         The channel is assumed to be 0xff, so fully visible.
 *
 * \param filepathString A string to the file location of a bmp file which will be opened with fread(filepath,"rb")
 * \param outputFormatString
 *        Specifies the order of colors in the imageDataReturnPP. Must be 4 characters + 0 null termination character long.
 *        Supported characters are A,R,G,B,0. They can also be used multiple times to duplicate a channel like "RRR0". The first character corresponds
 *        to the lowest order byte in imageDataReturnPP. Please note that when interpreting the imageDataReturnPP as a char** the position of the colors will
 *        change due to system endianes
 * \return TextureData.dataP
 *        The function will heap allocate the necessary memory and will write this memory address into TextureData.dataP.
 *        The image array will always be top down, so the upper left pixel in the image will be the first one in this uint32_t array.
 *        The next pixel will be the one which is one pixel to the right in the first row.
 */
struct TextureData bmpLoader_load(char* filepathString,char* outputFormartString);
#endif //BMPLOADER_H_INCLUDED
