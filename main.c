#include <stdio.h>
#include "submodules/xmlReader/xmlReader.h"
//void loadMeshWithId()

int main(int argc, char** argv){
    FILE* cylinderDaeFileP=fopen("./res/kegel_ohne_camera.dae","rb");
    struct xmlTreeElement* xmlDaeRootP=0;
    readXML(cylinderDaeFileP,&xmlDaeRootP);
}
