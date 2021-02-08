#ifndef BMPLOADER_H_INCLUDED
#define BMPLOADER_H_INCLUDED
#include <stdint.h>
void bmpLoader_load(char* filepathString,char* outputFormartString,uint32_t** imageDataReturnPP,int32_t* widthReturnP,int32_t* heightReturnP);
#endif //BMPLOADER_H_INCLUDED
