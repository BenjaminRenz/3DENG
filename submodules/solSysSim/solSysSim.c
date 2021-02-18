#include "xmlReader/xmlReader.h"
#include "xmlReader/xmlHelper.h"
#include "xmlReader/stringutils.h"
#include "linmath/linmath.h"
#include "dynList/dynList.h"
#include "glfw/glfw3.h"
#include <math.h>
#include "solSysSim/solSysSim.h"

void rungeKuttaAlloc();

struct objectSimData{
    vec3 position;      //in m
    vec3 velocity;      //in m/s
    float mass;         //in kg
    float radius;       //in m
};

DlTypedef_plain(bodySimDat,struct objectSimData);
DlTypedef_plain(VEC3,vec3);

float G;
float length_au_m;

Dl_bodySimDat* simDatDlP;
Dl_VEC3* RungK_posDlP;
Dl_VEC3* RungK_velDlP;
Dl_VEC3* RungK_accelDlP;

Dl_VEC3* RungK_DeltaPosDlP;
Dl_VEC3* RungK_DeltaVelDlP;


void solSys_initAndGetPlanetNames(Dl_DlP_utf32Char** astroBodyStringDlPP){
    xmlTreeElement* solSysInitXmlP;
    FILE* solSysFileP = fopen("./res/solSysInit.xml","rb");
    readXML(solSysFileP, &solSysInitXmlP);
    fclose(solSysFileP);
    //read constants and conversion factors
    xmlTreeElement* refUnitsXmlP = getFirstSubelementWithASCII(solSysInitXmlP, "reference_units", NULL, NULL, xmltype_tag, 1);
    Dl_utf32Char* G_String              = getValueFromKeyNameASCII(refUnitsXmlP->attributes, "G");
    Dl_utf32Char* mass_sun_kg_String    = getValueFromKeyNameASCII(refUnitsXmlP->attributes, "mass_sun_kg");
    Dl_utf32Char* length_au_m_String    = getValueFromKeyNameASCII(refUnitsXmlP->attributes, "length_au_m");
    Dl_utf32Char* time_day_sec_String   = getValueFromKeyNameASCII(refUnitsXmlP->attributes, "time_day_sec");

    Dl_CM* numSep = Dl_CM_initFromList(' ',' ');
    Dl_CM* decSep = Dl_CM_initFromList('.','.',',',',');
    Dl_CM* oOfMag = Dl_CM_initFromList('e','e','E','E');
    G             = Dl_utf32Char_to_float(numSep,oOfMag,decSep, G_String)->items[0];
    length_au_m   = Dl_utf32Char_to_float(numSep,oOfMag,decSep, length_au_m_String)->items[0];
    float mass_sun_kg   = Dl_utf32Char_to_float(numSep,oOfMag,decSep, mass_sun_kg_String)->items[0];
    float time_day_sec  = Dl_utf32Char_to_float(numSep,oOfMag,decSep, time_day_sec_String)->items[0];

    //get astronomical body data
    Dl_xmlP* astroBodiesXmlDlP = getAllSubelementsWithASCII(solSysInitXmlP, "obj", NULL, NULL, xmltype_tag, 1);
    simDatDlP = Dl_bodySimDat_alloc(astroBodiesXmlDlP->itemcnt,NULL);
    *astroBodyStringDlPP = Dl_DlP_utf32Char_alloc(astroBodiesXmlDlP->itemcnt, NULL);
    for(size_t astroBodyIdx = 0; astroBodyIdx < astroBodiesXmlDlP->itemcnt; astroBodyIdx++){
        (*astroBodyStringDlPP)->items[astroBodyIdx] = getValueFromKeyNameASCII(astroBodiesXmlDlP->items[astroBodyIdx]->attributes, "name");
        xmlTreeElement* objCharDataXmlP=getNthChildWithType(astroBodiesXmlDlP->items[astroBodyIdx],0,xmltype_chardata);
        Dl_float* BodyFloatDlP=Dl_utf32Char_to_float(numSep,oOfMag,decSep,objCharDataXmlP->charData);
        if(BodyFloatDlP->itemcnt != 8){
            dprintf(DBGT_ERROR,"Missing Object data for Object named %s",Dl_utf32Char_toStringAlloc((*astroBodyStringDlPP)->items[astroBodyIdx]));
            exit(1);
        }
        simDatDlP->items[astroBodyIdx].position[0] = BodyFloatDlP->items[0] * length_au_m;
        simDatDlP->items[astroBodyIdx].position[1] = BodyFloatDlP->items[1] * length_au_m;
        simDatDlP->items[astroBodyIdx].position[2] = BodyFloatDlP->items[2] * length_au_m;

        simDatDlP->items[astroBodyIdx].velocity[0] = BodyFloatDlP->items[3] * length_au_m / time_day_sec;
        simDatDlP->items[astroBodyIdx].velocity[1] = BodyFloatDlP->items[4] * length_au_m / time_day_sec;
        simDatDlP->items[astroBodyIdx].velocity[2] = BodyFloatDlP->items[5] * length_au_m / time_day_sec;

        simDatDlP->items[astroBodyIdx].mass        = mass_sun_kg / BodyFloatDlP->items[6];
        simDatDlP->items[astroBodyIdx].radius      = BodyFloatDlP->items[7] * 1e3;
    }
    Dl_CM_delete(numSep);
    Dl_CM_delete(decSep);
    Dl_CM_delete(oOfMag);

    rungeKuttaAlloc();
}

void rungeKuttaAlloc(){
    RungK_accelDlP=Dl_VEC3_alloc(simDatDlP->itemcnt,NULL);
    RungK_velDlP=Dl_VEC3_alloc(simDatDlP->itemcnt,NULL);
    RungK_posDlP=Dl_VEC3_alloc(simDatDlP->itemcnt,NULL);

    RungK_DeltaPosDlP=Dl_VEC3_alloc(simDatDlP->itemcnt,NULL);
    RungK_DeltaVelDlP=Dl_VEC3_alloc(simDatDlP->itemcnt,NULL);
}

void solSys_deinit(){
    Dl_VEC3_delete(RungK_posDlP);
    Dl_VEC3_delete(RungK_velDlP);
    Dl_VEC3_delete(RungK_accelDlP);

    Dl_VEC3_delete(RungK_DeltaPosDlP);
    Dl_VEC3_delete(RungK_DeltaVelDlP);

    Dl_bodySimDat_delete(simDatDlP);
}

void getRungeKAccel(vec3 accelReturn, Dl_VEC3* RkPosInDlP, size_t currentBodyIdx){
    //initialize accelReturn
    vec3 zero = {0.0f, 0.0f, 0.0f};
    vec3_dup(accelReturn, zero);

    //Calculate force to every other body
    for(size_t BodyIdx = 0; BodyIdx < simDatDlP->itemcnt; BodyIdx++){
        if(currentBodyIdx!=BodyIdx){
            vec3 deltaR;
            vec3_sub(deltaR,RkPosInDlP->items[currentBodyIdx],RkPosInDlP->items[BodyIdx]);
            float distance=vec4_len(deltaR);
            vec3_scale(deltaR, deltaR, -G * simDatDlP->items[BodyIdx].mass / pow(distance,3.0f));
            vec3_add(accelReturn,accelReturn,deltaR);
        }
    }
}

void rungeKuttaSimStep(float dt){
    //init RungK
    vec3 zero = {0.0f, 0.0f, 0.0f};
    for(size_t BodyIdx = 0; BodyIdx < simDatDlP->itemcnt; BodyIdx++){
        vec3_dup(RungK_posDlP->items[BodyIdx]  , simDatDlP->items[BodyIdx].position);
        vec3_dup(RungK_velDlP->items[BodyIdx]  , zero);
        vec3_dup(RungK_accelDlP->items[BodyIdx], zero);
    }

    float RKCoeff[]={0.0f, dt/2, dt/2, dt, 0.0f};
    float RKdeltaCoeff[]={dt/6, dt/3, dt/3, dt/6};
    for(int RKstep = 0; RKstep < 4; RKstep++){
        for(size_t BodyIdx = 0; BodyIdx < simDatDlP->itemcnt; BodyIdx++){
            //calculate v
            vec3_scale(RungK_velDlP->items[BodyIdx], RungK_accelDlP->items[BodyIdx], RKCoeff[RKstep]);
            vec3_add(RungK_velDlP->items[BodyIdx], RungK_velDlP->items[BodyIdx], simDatDlP->items[BodyIdx].velocity);
            //add this to deltaR
            vec3 tempV;
            vec3_scale(tempV, RungK_velDlP->items[BodyIdx], RKdeltaCoeff[BodyIdx]);
            vec3_add(RungK_DeltaPosDlP->items[BodyIdx], RungK_DeltaPosDlP->items[BodyIdx], tempV);

            //calculate a
            getRungeKAccel(RungK_accelDlP->items[BodyIdx], RungK_posDlP, BodyIdx);
            //add this to deltaV
            vec3 tempA;
            vec3_scale(tempA, RungK_accelDlP->items[BodyIdx], RKdeltaCoeff[BodyIdx]);
            vec3_add(RungK_DeltaVelDlP->items[BodyIdx], RungK_DeltaVelDlP->items[BodyIdx], tempA);

            //calculate r
            vec3_scale(RungK_posDlP->items[BodyIdx], RungK_velDlP->items[BodyIdx], RKCoeff[RKstep+1]);
            vec3_add(RungK_posDlP->items[BodyIdx], RungK_posDlP->items[BodyIdx], simDatDlP->items[BodyIdx].position);
        }
    }
    //update position
    for(size_t BodyIdx = 0; BodyIdx < simDatDlP->itemcnt; BodyIdx++){
        vec3_add(simDatDlP->items[BodyIdx].position, simDatDlP->items[BodyIdx].position, RungK_DeltaPosDlP->items[BodyIdx]);
        vec3_add(simDatDlP->items[BodyIdx].velocity, simDatDlP->items[BodyIdx].velocity, RungK_DeltaVelDlP->items[BodyIdx]);
    }
}
/*
    vec3 k1_v[numBodies] = v;
    vec3 k1_a[numBodies] = get_acc(r);
    vec3 k2_v[numBodies] = v+k1_v*dt/2;
    vec3 k2_a[numBodies] = get_acc(r+k1_r*dt/2);
    vec3 k3_v[numBodies] = v+k2_v*dt/2;
    vec3 k3_a[numBodies] = get_acc(r+k2_r*dt/2);
    vec3 k4_v[numBodies] = v+k3_v*dt;
    vec3 k4_a[numBodies] = get_acc(r+k3_r*dt);
    r=r+dt/6*(k1_v+2*k2_v+2*k3_v+k4_v);
    v=v+dt/6*(k1_a+2*k2_a+2*k3_a+k4_a);
*/

float updateDeltaTime() {             //Get the current time with glfwGetTime and subtract last time to return deltatime
    static double last_glfw_time = 0.0;
    if(last_glfw_time == 0.0){
        last_glfw_time = glfwGetTime();
    }
    double current_glfw_time;
    current_glfw_time = glfwGetTime();
    float delta = (float)(current_glfw_time - last_glfw_time);
    last_glfw_time = current_glfw_time;
    return delta;
}

void solSim_updateModelMatrices(Dl_MAT4* ModelMatricesDlP){
    float deltaTime=0.0f;//updateDeltaTime();
    rungeKuttaSimStep(deltaTime*1e2);

    for(size_t objectIdx = 0; objectIdx < simDatDlP->itemcnt; objectIdx++){
        mat4x4 ModelMatrix;
        mat4x4_identity(ModelMatrix);
        float radiusscale=sqrt(simDatDlP->items[objectIdx].radius* 2e-8) ;
        if(objectIdx == 0){
            radiusscale=simDatDlP->items[objectIdx].radius * 5e-10;
        }


        //mat4x4_scale(ModelMatrix, ModelMatrix, );
        mat4x4_translate_in_place(ModelMatrix,
                         simDatDlP->items[objectIdx].position[0] / length_au_m,
                         simDatDlP->items[objectIdx].position[1] / length_au_m,
                         simDatDlP->items[objectIdx].position[2] / length_au_m);
        mat4x4_scale_aniso(ModelMatrix, ModelMatrix, radiusscale, radiusscale, radiusscale);
        mat4x4_dup(ModelMatricesDlP->items[objectIdx],ModelMatrix);
    }
}
