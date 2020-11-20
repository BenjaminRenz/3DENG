#include <stdio.h>
#include "submodules/xmlReader/debug.h"
#include "submodules/xmlReader/xmlReader.h"
#include "submodules/xmlReader/stringutils.h"
//void loadMeshWithId()

int main(int argc, char** argv){
    dprintf(DBGT_INFO,"%d,%d,%d,%d",sizeof(uint32_t),sizeof(void*),sizeof(void**),sizeof(unsigned int));
    struct DynamicList* meshID=stringToUTF32Dynlist("Cylinder-mesh");

    FILE* cylinderDaeFileP=fopen("./res/kegel_ohne_camera.dae","rb");
    struct xmlTreeElement* xmlDaeRootP=0;
    readXML(cylinderDaeFileP,&xmlDaeRootP);
    //printXMLsubelements(xmlDaeRootP);

    struct xmlTreeElement* xmlColladaElementP=getNthSubelement(xmlDaeRootP,0);
    struct xmlTreeElement* xmlLibGeoElementP=getFirstSubelementWith_freeArg2345(xmlColladaElementP,stringToUTF32Dynlist("library_geometries"),NULL,NULL,NULL,0);
    struct xmlTreeElement* xmlGeoElementP=getFirstSubelementWith_freeArg2345(xmlLibGeoElementP,stringToUTF32Dynlist("geometry"),stringToUTF32Dynlist("id"),DlDuplicate(sizeof(uint32_t),meshID),NULL,0); //does  not work?
    struct xmlTreeElement* xmlMeshElementP=getFirstSubelementWith_freeArg2345(xmlGeoElementP,stringToUTF32Dynlist("mesh"),NULL,NULL,NULL,0);

    //Get Triangles
    struct xmlTreeElement* xmlTrianglesP=getFirstSubelementWith_freeArg2345(xmlMeshElementP,stringToUTF32Dynlist("triangles"),NULL,NULL,NULL,0);
    struct DynamicList* TrianglesCountString=getValueFromKeyName_freeArg2(xmlTrianglesP->attributes,stringToUTF32Dynlist("count"));
    struct DynamicList* TrianglesCount=utf32dynlistToInts64_freeArg1(createCharMatchList(2,' ',' '),TrianglesCountString);
    dprintf(DBGT_INFO,"Model has %d triangles",((int64_t*)TrianglesCount->items)[0]);

    struct xmlTreeElement* xmlTrianglesOrderP=getFirstSubelementWith_freeArg2345(xmlTrianglesP,stringToUTF32Dynlist("p"),NULL,NULL,NULL,0);
    struct xmlTreeElement* xmlTrianglesOrderContentP=getNthSubelementOrMisc(xmlTrianglesOrderP,0);
    struct DynamicList* TrianglesOrder=utf32dynlistToInts64_freeArg1(createCharMatchList(2,' ',' '),xmlTrianglesOrderContentP->content);
    dprintf(DBGT_INFO,"Found Triangle order list with %d entries",TrianglesOrder->itemcnt);

    //Get Normals
    struct xmlTreeElement* xmlNormalsSourceP=getFirstSubelementWith_freeArg2345(xmlMeshElementP,stringToUTF32Dynlist("source"),
                                                                                                stringToUTF32Dynlist("id"),
                                                                                                DlCombine_freeArg3(sizeof(uint32_t),meshID,stringToUTF32Dynlist("-normals")),
                                                                                                NULL,0);
    struct xmlTreeElement* xmlNormalsFloatP=getFirstSubelementWith_freeArg2345(xmlNormalsSourceP,stringToUTF32Dynlist("float_array"),
                                                                                                stringToUTF32Dynlist("id"),
                                                                                                DlCombine_freeArg3(sizeof(uint32_t),meshID,stringToUTF32Dynlist("-normals-array")),
                                                                                                NULL,0);
    struct DynamicList* NormalsCountString=getValueFromKeyName_freeArg2(xmlNormalsFloatP->attributes,stringToUTF32Dynlist("count"));
    printUTF32Dynlist(NormalsCountString);
    struct DynamicList* NormalsCount=utf32dynlistToInts64_freeArg1(createCharMatchList(2,' ',' '),NormalsCountString);
    dprintf(DBGT_INFO,"Model has count %d normal coordinates",((int64_t*)NormalsCount->items)[0]);
    struct xmlTreeElement* xmlNormalsFloatContentP=getNthSubelementOrMisc(xmlNormalsFloatP,0);
    struct DynamicList* NormalsDlP=utf32dynlistToFloats_freeArg123(createCharMatchList(4,' ',' ','\t','\t'),createCharMatchList(4,'e','e','E','E'),createCharMatchList(2,'.','.'),xmlNormalsFloatContentP->content);
    dprintf(DBGT_INFO,"Model has %d normal coordinates",NormalsDlP->itemcnt);

    //Get Positions
    struct xmlTreeElement* xmlPositionsSourceP=getFirstSubelementWith_freeArg2345(xmlMeshElementP,stringToUTF32Dynlist("source"),
                                                                     stringToUTF32Dynlist("id"),DlCombine_freeArg3(sizeof(uint32_t),meshID,stringToUTF32Dynlist("-positions")),NULL,0);

    struct xmlTreeElement* xmlPositionsFloatP=getFirstSubelementWith_freeArg2345(xmlPositionsSourceP,stringToUTF32Dynlist("float_array"),
                                                                    stringToUTF32Dynlist("id"),DlCombine_freeArg3(sizeof(uint32_t),meshID,stringToUTF32Dynlist("-positions-array")),NULL,0);

    struct DynamicList* PositionsCountString=getValueFromKeyName_freeArg2(xmlPositionsFloatP->attributes,stringToUTF32Dynlist("count"));
    struct DynamicList* PositionsCount=utf32dynlistToInts64_freeArg1(createCharMatchList(2,' ',' '),PositionsCountString);
    //if(xmlPositionsFloatP->content->type!=dynlisttype_utf32chars){dprintf(DBGT_ERROR,"Invalid content type");return 0;}
    struct xmlTreeElement* xmlPositionsFloatContentP=getNthSubelementOrMisc(xmlPositionsFloatP,0);
    struct DynamicList* PositionsDlP=utf32dynlistToFloats_freeArg123(createCharMatchList(4,' ',' ','\t','\t'),createCharMatchList(4,'e','e','E','E'),createCharMatchList(2,'.','.'),xmlPositionsFloatContentP->content);
}
