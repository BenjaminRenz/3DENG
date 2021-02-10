#ifndef DAELOADER_H_INCLUDED
#define DAELOADER_H_INCLUDED
#include <stdint.h>
#include "dynList/dynList.h"
#include "xmlReader/stringutils.h"
#include "bmpLoader/bmpLoader.h"

struct DataFromDae{
    Dl_float* CombinedPsNrUvDlP;      //DlP of floats, in order vec3 position, padding, vec3 normal, padding, vec2 uv
    Dl_int32* IndexingDlP;            //DlP of uint32_t
    struct TextureData  DiffuseTexture;
    struct TextureData  RoughnessTexture;
    struct TextureData  NormalTexture;
};


void daeLoader_load(Dl_utf32Char* filePathString,Dl_utf32Char* meshIdString,struct DataFromDae* outputDataP);
#endif //DAELOADER_H_INCLUDED
