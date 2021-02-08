#ifndef DAELOADER_H_INCLUDED
#define DAELOADER_H_INCLUDED
struct DataFromDae{
    struct DynamicList* CombinedPsNrUvDlP;      //DlP of floats, in order vec3 position, padding, vec3 normal, padding, vec2 uv
    struct DynamicList* IndexingDlP;            //DlP of uint32_t
};
void daeLoader_load(char* filePath,char* meshIdStringP,struct DataFromDae* outputDataP);
#endif //DAELOADER_H_INCLUDED
