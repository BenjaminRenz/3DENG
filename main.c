#include <stdio.h>
#include "submodules/xmlReader/debug.h"
#include "submodules/xmlReader/xmlReader.h"
#include "submodules/xmlReader/stringutils.h"
//void loadMeshWithId()

int main(int argc, char** argv){
    struct DynamicList* meshID=stringToUTF32Dynlist("Cylinder-mesh");

    FILE* cylinderDaeFileP=fopen("./res/kegel_ohne_camera.dae","rb");
    struct xmlTreeElement* xmlDaeRootP=0;
    readXML(cylinderDaeFileP,&xmlDaeRootP);
    struct xmlTreeElement* xmlColladaElementP=getNthSubelement(xmlDaeRootP,0);
    struct xmlTreeElement* xmlLibGeoElementP=getFirstSubelementWith_freeArg2345(xmlColladaElementP,stringToUTF32Dynlist("library_geometries"),NULL,NULL,NULL,0);
    struct xmlTreeElement* xmlGeoElementP=getFirstSubelementWith_freeArg2345(xmlLibGeoElementP,stringToUTF32Dynlist("geometry"),stringToUTF32Dynlist("id"),DlDuplicate(meshID),NULL,0); //does  not work?
    struct xmlTreeElement* xmlMeshElementP=getFirstSubelementWith_freeArg2345(xmlGeoElementP,stringToUTF32Dynlist("mesh"),NULL,NULL,NULL,0);

    //Get Triangles
    printXMLsubelements(xmlMeshElementP);

    struct xmlTreeElement* xmlTrianglesP=getFirstSubelementWith_freeArg2345(xmlMeshElementP,stringToUTF32Dynlist("triangles"),NULL,NULL,NULL,0);
     printXMLsubelements(xmlTrianglesP);
    struct DynamicList* TrianglesCountString=getValueFromKeyName_freeArg2(xmlTrianglesP->attributes,stringToUTF32Dynlist("count"));
    struct DynamicList* TrianglesCount=utf32dynlistToInts_freeArg1(createCharMatchList(2,' ',' '),TrianglesCountString);
    dprintf(DBGT_INFO,"Model has %d triangles",((int64_t*)TrianglesCount->items)[0]);

    struct xmlTreeElement* xmlTrianglesOrderP=getFirstSubelementWith_freeArg2345(xmlTrianglesP,stringToUTF32Dynlist("p"),NULL,NULL,NULL,0);

    printf("%d",xmlTrianglesOrderP->content->type);
    struct DynamicList* TrianglesOrder=utf32dynlistToInts_freeArg1(createCharMatchList(2,' ',' '),xmlTrianglesOrderP->content);
    dprintf(DBGT_INFO,"Found Vertex order list with %d entries",TrianglesOrder->itemcnt);

    //Get Normals
    struct xmlTreeElement* xmlNormalsSourceP=getFirstSubelementWith_freeArg2345(xmlMeshElementP,stringToUTF32Dynlist("source"),
                                                                    stringToUTF32Dynlist("id"),DlCombine_freeArg2(meshID,stringToUTF32Dynlist("-normals")),NULL,0);
    struct xmlTreeElement* xmlNormalsFloatP=getFirstSubelementWith_freeArg2345(xmlNormalsSourceP,stringToUTF32Dynlist("float_array"),
                                                                    stringToUTF32Dynlist("id"),DlCombine_freeArg2(meshID,stringToUTF32Dynlist("-normals-array")),NULL,0);
    struct DynamicList* NormalsCountString=getValueFromKeyName_freeArg2(xmlNormalsFloatP->attributes,stringToUTF32Dynlist("count"));
    //TODO wrong argument order
    uint32_t NormalsCount=utf32dynlistToInts_freeArg1(NormalsCountString,createCharMatchList(2,' ',' '));
    dprintf(DBGT_INFO,"Model has count %d normal coordinates",NormalsCount);
    if(xmlNormalsFloatP->content->type!=dynlisttype_utf32chars){dprintf(DBGT_ERROR,"Invalid content type");return 0;}
    struct DynamicList* NormalsDlP=utf32dynlistToFloats_freeArg123(createCharMatchList(4,' ',' ','\t','\t'),createCharMatchList(4,'e','e','E','E'),createCharMatchList(2,'.','.'),xmlNormalsFloatP->content);
    dprintf(DBGT_INFO,"Model has %d normal coordinates",NormalsDlP->itemcnt);

    //Get Positions
    struct xmlTreeElement* xmlPositionsSourceP=getFirstSubelementWith_freeArg2345(xmlMeshElementP,stringToUTF32Dynlist("source"),
                                                                     stringToUTF32Dynlist("id"),DlCombine_freeArg2(meshID,stringToUTF32Dynlist("-positions")),NULL,0);
    struct xmlTreeElement* xmlPositionsFloatP=getFirstSubelementWith_freeArg2345(xmlPositionsSourceP,stringToUTF32Dynlist("float_array"),
                                                                    stringToUTF32Dynlist("id"),DlCombine_freeArg2(meshID,stringToUTF32Dynlist("-normals-array")),NULL,0);
    struct DynamicList* PositionsCountString=getValueFromKeyName_freeArg2(xmlPositionsFloatP->attributes,stringToUTF32Dynlist("count"));
    uint32_t PositionsCount=utf32dynlistToInts_freeArg1(PositionsCountString,createCharMatchList(2,' ',' '));
    dprintf(DBGT_INFO,"Model has count %d position coordinates",PositionsCount);
    if(xmlPositionsFloatP->content->type!=dynlisttype_utf32chars){dprintf(DBGT_ERROR,"Invalid content type");return 0;}
    struct DynamicList* PositionsDlP=utf32dynlistToFloats_freeArg123(createCharMatchList(4,' ',' ','\t','\t'),createCharMatchList(4,'e','e','E','E'),createCharMatchList(2,'.','.'),xmlPositionsFloatP->content);
    dprintf(DBGT_INFO,"Model has %d position coordinates",PositionsDlP->itemcnt);

}
