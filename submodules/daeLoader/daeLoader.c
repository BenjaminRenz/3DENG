#include "daeLoader/daeLoader.h"
#include "bmpLoader/bmpLoader.h"
#include "xmlReader/xmlReader.h"
#include "xmlReader/xmlHelper.h"
#include "debugPrint/debugPrint.h"
#include "stdlib.h"
void _daeLoader_getTextureData(struct DataFromDae* outputDataP,struct xmlTreeElement* xmlColladaElementP);
void _daeLoader_getVertexData(struct DataFromDae* outputDataP,struct xmlTreeElement* xmlColladaElementP,Dl_utf32Char* meshIdString);


void daeLoader_load(Dl_utf32Char* filePathString,Dl_utf32Char* meshIdString,struct DataFromDae* outputDataP){
    char* asciiFilepath=Dl_utf32Char_toStringAlloc(filePathString);
    FILE* cylinderDaeFileP=fopen(asciiFilepath,"rb");
    free(asciiFilepath);
    xmlTreeElement* xmlDaeRootP=0;
    readXML(cylinderDaeFileP,&xmlDaeRootP);
    fclose(cylinderDaeFileP);
    //printXMLsubelements(xmlDaeRootP);
    xmlTreeElement* xmlColladaElementP=getNthChildWithType(xmlDaeRootP,0,xmltype_tag);
    _daeLoader_getVertexData(outputDataP,xmlColladaElementP,meshIdString);
    _daeLoader_getTextureData(outputDataP,xmlColladaElementP);
}

void _daeLoader_getTextureData(struct DataFromDae* outputDataP,xmlTreeElement* xmlColladaElementP){
    xmlTreeElement* xmlLibaryImagesP=getFirstSubelementWithASCII(xmlColladaElementP,"library_images",NULL,NULL,xmltype_tag,0);
    Dl_utf32Char* PrePathString=Dl_utf32Char_fromString("./res/");
    Dl_xmlP* allXmlImagesDlP=getAllSubelementsWithASCII(xmlLibaryImagesP,"image",NULL,NULL,xmltype_tag,0);
    for(uint32_t XmlImageElementIdx=0;XmlImageElementIdx<allXmlImagesDlP->itemcnt;XmlImageElementIdx++){
        struct xmlTreeElement* xmlImageP=((struct xmlTreeElement**)allXmlImagesDlP->items)[XmlImageElementIdx];
        struct xmlTreeElement* xmlImageInitP=getFirstSubelementWithASCII(xmlImageP,"init_from",NULL,NULL,xmltype_tag,0);
        struct xmlTreeElement* xmlImageInitContentP=getFirstSubelementWithASCII(xmlImageInitP,NULL,NULL,NULL,xmltype_chardata,0);
        Dl_utf32Char* LoadImageFilenameString=xmlImageInitContentP->nameOrContent;
        Dl_utf32Char* LoadImagePathString=Dl_utf32Char_mergeDulplicate(PrePathString,LoadImageFilenameString);
        char* imageLoadPathString=Dl_utf32Char_toStringAlloc_freeArg1(LoadImagePathString);
        dprintf(DBGT_INFO,"Load Image from %s",imageLoadPathString);

        outputDataP->DiffuseTexture=bmpLoader_load(imageLoadPathString,"BGRA");
    }
    Dl_utf32Char_delete(PrePathString);
}

void _daeLoader_getVertexData(struct DataFromDae* outputDataP,xmlTreeElement* xmlColladaElementP,Dl_utf32Char* meshIdString){
    xmlTreeElement* xmlLibGeoElementP=getFirstSubelementWithASCII(xmlColladaElementP,"library_geometries",NULL,NULL,xmltype_tag,0);
    xmlTreeElement* xmlGeoElementP=getFirstSubelementWithASCII(xmlLibGeoElementP,"geometry","id",meshIdString,xmltype_tag,0);
    if(!xmlGeoElementP){
        dprintf(DBGT_ERROR,"The collada file does not contain a mesh with the name %s",Dl_utf32Char_toStringAlloc(meshIdString));
        exit(1);
    }
    xmlTreeElement* xmlMeshElementP=getFirstSubelementWithASCII(xmlGeoElementP,"mesh",NULL,NULL,0,0);

    //Get triangles xml element
    xmlTreeElement* xmlTrianglesP=getFirstSubelementWithASCII(xmlMeshElementP,"triangles",NULL,NULL,0,0);
    Dl_utf32Char* TrianglesCountString=getValueFromKeyNameASCII(xmlTrianglesP->attributes,"count");
    Dl_int64* TrianglesCount=Dl_utf32Char_to_int64_freeArg1(Dl_CM_initFromList(' ',' '),TrianglesCountString);
    dprintf(DBGT_INFO,"Model has %lld triangles",TrianglesCount->items[0]);

    //get triangle position, normal, uv index list
    xmlTreeElement* xmlTrianglesOrderP=getFirstSubelementWithASCII(xmlTrianglesP,"p",NULL,NULL,0,0);
    if(!xmlTrianglesOrderP){
        dprintf(DBGT_ERROR,"No Triangles Order list found");exit(1);
    }
    xmlTreeElement* xmlTrianglesOrderContentP=getNthChildWithType(xmlTrianglesOrderP,0,xmltype_chardata);
    if(!xmlTrianglesOrderContentP->nameOrContent->itemcnt){
        dprintf(DBGT_ERROR,"Triangles Order List empty");exit(1);
    }
    Dl_int64* TempTriangleIndexDlP=Dl_utf32Char_to_int64_freeArg1(Dl_CM_initFromList(' ',' '),xmlTrianglesOrderContentP->nameOrContent);

    //Define structures to copy read data to
    Dl_float* PosDlP;
    Dl_int32* PosIndexDlP;
    Dl_float* NormDlP;
    Dl_int32* NormIndexDlP;
    Dl_float* UvDlP;
    Dl_int32* UvIndexDlP;

    //Get input accessor and position,vertex,normal source
    Dl_xmlP* InputXmlTrianglesP=getAllSubelementsWithASCII(xmlTrianglesP,"input",NULL,NULL,0,0);
    for(uint32_t inputsemantic=0;inputsemantic<InputXmlTrianglesP->itemcnt;inputsemantic++){
        Dl_utf32Char* semanticStringDlP=getValueFromKeyNameASCII(InputXmlTrianglesP->items[inputsemantic]->attributes,"semantic");
        Dl_utf32Char* offsetStringDlP  =getValueFromKeyNameASCII(InputXmlTrianglesP->items[inputsemantic]->attributes,"offset");
        struct DynamicList* offsetNumbersDlP =Dl_utf32_to_Dl_int64(Dl_CMatch_create(2,' ',' '),offsetStringDlP);
        struct DynamicList* sourceStringWithHashtagDlP=getValueFromKeyNameASCII(((struct xmlTreeElement**)InputXmlTrianglesP->items)[inputsemantic]->attributes,"source");
        struct DynamicList* sourceStringDlP=Dl_utf32_Substring(sourceStringWithHashtagDlP,1,-1);
        //get corresponding source data
        struct xmlTreeElement* refXmlElmntP=getFirstSubelementWithASCII(xmlMeshElementP,NULL,"id",sourceStringDlP,0,0);
        //Handle VERTEX special case, which has to jump to POSITIONS again
        struct xmlTreeElement* sourceXmlElmntP;
        if(Dl_utf32_compareEqual_freeArg2(refXmlElmntP->name,Dl_utf32_fromString("vertices"))){
            refXmlElmntP=getFirstSubelementWithASCII(refXmlElmntP,"input",NULL,NULL,0,0);
            sourceStringWithHashtagDlP=getValueFromKeyNameASCII(refXmlElmntP->attributes,"source");
            sourceStringDlP=Dl_utf32_Substring(sourceStringWithHashtagDlP,1,-1);
            sourceXmlElmntP=getFirstSubelementWithASCII(xmlMeshElementP,NULL,"id",sourceStringDlP,0,0);
        }else{
            sourceXmlElmntP=refXmlElmntP;
        }
        struct xmlTreeElement* floatArrayXmlElementP=getFirstSubelementWithASCII(sourceXmlElmntP,"float_array",NULL,NULL,0,1);
        struct xmlTreeElement* floatArrayCharDataP=getFirstSubelementWith(floatArrayXmlElementP,NULL,NULL,NULL,xmltype_chardata,1);
        struct DynamicList* sourceFloatArrayDlP=Dl_utf32_to_Dl_float_freeArg123(Dl_CMatch_create(2,' ',' '),Dl_CMatch_create(4,'e','e','E','E'),Dl_CMatch_create(4,'.','.',',',','),floatArrayCharDataP->content);
        //check if FloatArray length corresponds with the expected count specified in the xml file
        struct DynamicList* countStringDlP=getValueFromKeyNameASCII(floatArrayXmlElementP->attributes,"count");
        struct DynamicList* countIntArrayDlP=Dl_utf32_to_Dl_int64_freeArg1(Dl_CMatch_create(2,' ',' '),countStringDlP);
        if(((int64_t*)countIntArrayDlP->items)[0]!=sourceFloatArrayDlP->itemcnt){
            dprintf(DBGT_ERROR,"Missmatch in supplied source float count, count says %lld, found were %d",((int64_t*)countIntArrayDlP)[0],sourceFloatArrayDlP->itemcnt);
        }

        uint32_t offsetInIdxArray=((int64_t*)(offsetNumbersDlP->items))[0];
        DlDelete(offsetNumbersDlP);
        uint32_t stridePerVertex=InputXmlTrianglesP->itemcnt;
        uint32_t numberOfAllIndices=TempTriangleIndexDlP->itemcnt;
        uint32_t numberOfVertexIndices=numberOfAllIndices/stridePerVertex;
        struct DynamicList* CurrentOutputArrayDlP=0;
        if(Dl_utf32_compareEqual_freeArg2(semanticStringDlP,Dl_utf32_fromString("VERTEX"))){
            //Create and fill PosIndexDlP
            PosIndexDlP=DlAlloc(sizeof(uint32_t),DlType_int32,numberOfVertexIndices,NULL);
            CurrentOutputArrayDlP=PosIndexDlP;
            PosDlP=sourceFloatArrayDlP;
        }else if(Dl_utf32_compareEqual_freeArg2(semanticStringDlP,Dl_utf32_fromString("NORMAL"))){
            //Create and fill NormIndexDlP
            NormIndexDlP=DlAlloc(sizeof(uint32_t),DlType_int32,numberOfVertexIndices,NULL);
            CurrentOutputArrayDlP=NormIndexDlP;
            NormDlP=sourceFloatArrayDlP;
        }else if(Dl_utf32_compareEqual_freeArg2(semanticStringDlP,Dl_utf32_fromString("TEXCOORD"))){
            //Create and fill UvIndexDlP
            UvIndexDlP=DlAlloc(sizeof(uint32_t),DlType_int32,numberOfVertexIndices,NULL);
            CurrentOutputArrayDlP=UvIndexDlP;
            UvDlP=sourceFloatArrayDlP;
        }else{
            dprintf(DBGT_ERROR,"Unknown semantic type");
            exit(1);
        }
        for(uint32_t IdxInIndexArray=0;IdxInIndexArray<numberOfVertexIndices;IdxInIndexArray++){
            ((int32_t*)(CurrentOutputArrayDlP->items))[IdxInIndexArray]=((int64_t*)(TempTriangleIndexDlP->items))[stridePerVertex*IdxInIndexArray+offsetInIdxArray];
        }
    }

    //Since vulkan does not support individual index buffers we need to list vertices and combine only the one with the same position& normal& uv
    //DataBuffer will be layed out like vec4 pos; vec4 norm; vec2 UV; ...
    struct DynamicList* CombinedPsNrUvDlP=DlAlloc(sizeof(float),DlType_float,(4+4+2)*PosIndexDlP->itemcnt,NULL);
    struct DynamicList* NewIndexBuffer=DlAlloc(sizeof(uint32_t),DlType_uint32,PosIndexDlP->itemcnt,NULL);
    uint32_t uniqueVertices=0;
    for(uint32_t vertex=0;vertex<PosIndexDlP->itemcnt;vertex++){
        uint32_t posIdx= ((uint32_t*)PosIndexDlP->items) [vertex];
        uint32_t normIdx=((uint32_t*)NormIndexDlP->items)[vertex];
        uint32_t uvIdx=  ((uint32_t*)UvIndexDlP->items)  [vertex];
        uint32_t testUniqueVertex;
        for(testUniqueVertex=0;testUniqueVertex<uniqueVertices;testUniqueVertex++){
            //Compare new vertex read from PosDlP, NormDlP and UvDlP with already read vertices in CombinedPsNrUvDlP
            if(   ((float*)PosDlP->items) [3*posIdx  ] ==((float*)CombinedPsNrUvDlP->items)[10*testUniqueVertex  ]
                &&((float*)PosDlP->items) [3*posIdx+1] ==((float*)CombinedPsNrUvDlP->items)[10*testUniqueVertex+1]
                &&((float*)PosDlP->items) [3*posIdx+2] ==((float*)CombinedPsNrUvDlP->items)[10*testUniqueVertex+2]
                &&((float*)NormDlP->items)[3*normIdx  ]==((float*)CombinedPsNrUvDlP->items)[10*testUniqueVertex+4]
                &&((float*)NormDlP->items)[3*normIdx+1]==((float*)CombinedPsNrUvDlP->items)[10*testUniqueVertex+5]
                &&((float*)NormDlP->items)[3*normIdx+2]==((float*)CombinedPsNrUvDlP->items)[10*testUniqueVertex+6]
                &&((float*)UvDlP->items)  [2*uvIdx]    ==((float*)CombinedPsNrUvDlP->items)[10*testUniqueVertex+8]
                &&((float*)UvDlP->items)  [2*uvIdx+1]  ==((float*)CombinedPsNrUvDlP->items)[10*testUniqueVertex+9]){
                break;
            }
        }
        //
        ((uint32_t*)NewIndexBuffer->items)[vertex]=testUniqueVertex;
        //for loop terminated prematurely and we have found a duplicate vertex
        if(testUniqueVertex==uniqueVertices){
            //Add vertex into combined buffer with padding
            ((float*)CombinedPsNrUvDlP->items)[10*uniqueVertices  ]=((float*)PosDlP->items) [3*posIdx  ];
            ((float*)CombinedPsNrUvDlP->items)[10*uniqueVertices+1]=((float*)PosDlP->items) [3*posIdx+1];
            ((float*)CombinedPsNrUvDlP->items)[10*uniqueVertices+2]=((float*)PosDlP->items) [3*posIdx+2];
            ((float*)CombinedPsNrUvDlP->items)[10*uniqueVertices+3]=-42.0f;
            ((float*)CombinedPsNrUvDlP->items)[10*uniqueVertices+4]=((float*)NormDlP->items)[3*normIdx  ];
            ((float*)CombinedPsNrUvDlP->items)[10*uniqueVertices+5]=((float*)NormDlP->items)[3*normIdx+1];
            ((float*)CombinedPsNrUvDlP->items)[10*uniqueVertices+6]=((float*)NormDlP->items)[3*normIdx+2];
            ((float*)CombinedPsNrUvDlP->items)[10*uniqueVertices+7]=-42.0f;
            ((float*)CombinedPsNrUvDlP->items)[10*uniqueVertices+8]=((float*)UvDlP->items)  [2*uvIdx  ];
            ((float*)CombinedPsNrUvDlP->items)[10*uniqueVertices+9]=((float*)UvDlP->items)  [2*uvIdx+1];
            uniqueVertices++;
        }
    }
    DlResize(&CombinedPsNrUvDlP,10*uniqueVertices);  //Discard preallocated data which has been left empty because of dulplicat vertices
    for(uint32_t idx=0;idx<NewIndexBuffer->itemcnt;idx++){
        printf("I: %d\n",((int32_t*)NewIndexBuffer->items)[idx]);
    }
    for(uint32_t item=0;item<CombinedPsNrUvDlP->itemcnt;item+=10){
        printf("px: %f\t",((float*)CombinedPsNrUvDlP->items)[item+0]);
        printf("py: %f\t",((float*)CombinedPsNrUvDlP->items)[item+1]);
        printf("pz: %f\t",((float*)CombinedPsNrUvDlP->items)[item+2]);
        printf("nx: %f\t",((float*)CombinedPsNrUvDlP->items)[item+4]);
        printf("ny: %f\t",((float*)CombinedPsNrUvDlP->items)[item+5]);
        printf("nz: %f\t",((float*)CombinedPsNrUvDlP->items)[item+6]);
        printf("u: %f\t",((float*)CombinedPsNrUvDlP->items)[item+8]);
        printf("v: %f\n",((float*)CombinedPsNrUvDlP->items)[item+9]);
    }
    outputDataP->CombinedPsNrUvDlP=CombinedPsNrUvDlP;
    outputDataP->IndexingDlP=NewIndexBuffer;
}