#ifndef SOLSYSSIM_H_INCLUDED
#define SOLSYSSIM_H_INCLUDED
DlTypedef_plain(MAT4,mat4x4);
void solSys_initAndGetPlanetNames(Dl_DlP_utf32Char** astroBodyStringDlPP);
void solSim_updateModelMatrices(Dl_MAT4* ModelMatricesDlP);
void solSys_deinit();
#endif // SOLSYSSIM_H_INCLUDED
