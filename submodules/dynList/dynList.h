#include "debugPrint/debugPrint.h"
#include "mathHelper/mathHelper.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
//macro based implementation of c++ like vectors
//functions are declared static so that this header only library can be included in multiple different
//c files, without the compiler complaining about multiple definitions of the function in these different source files

#define DlTypedef_generic(name,c_type)                                                                      \
typedef struct Dl_##name{                                                                                   \
    size_t itemcnt;                                                                                         \
    c_type*    items;                                                                                       \
}Dl_##name ;                                                                                                \
static void Dl_##name##_delete(Dl_##name* ToDeleteDlP);                                                     \
static Dl_##name* Dl_##name##_alloc(size_t numOfNewElements, c_type* optionalInitP){                        \
    Dl_##name* newDlP=(Dl_##name*)malloc(sizeof(Dl_##name));                                                \
    newDlP->itemcnt=numOfNewElements;                                                                       \
    newDlP->items=(c_type*)malloc(sizeof(c_type)*numOfNewElements);                                         \
    if(numOfNewElements&&optionalInitP){                                                                    \
        memcpy(newDlP->items,optionalInitP,sizeof(c_type)*numOfNewElements);                                \
    }                                                                                                       \
    return newDlP;                                                                                          \
}                                                                                                           \
static void Dl_##name##_resize(Dl_##name* ToResizeDlP,size_t NewNumOfElements){                             \
    if(!ToResizeDlP){                                                                                       \
        dprintf(DBGT_ERROR,"Dynlist to resize was a nullptr");                                              \
        exit(1);                                                                                            \
    }                                                                                                       \
    ToResizeDlP->items=(c_type*)realloc(ToResizeDlP->items,sizeof(c_type)*NewNumOfElements);                \
    ToResizeDlP->itemcnt=NewNumOfElements;                                                                  \
};                                                                                                          \
static void Dl_##name##_append(Dl_##name* ToAppendDlP,size_t NumOfAdditionalElements, c_type* InitP){       \
    size_t oldItemcnt=ToAppendDlP->itemcnt;                                                                 \
    Dl_##name##_resize(ToAppendDlP,oldItemcnt+NumOfAdditionalElements);                                     \
    if((NumOfAdditionalElements>0)&&InitP){                                                                 \
        memcpy(&(ToAppendDlP->items[oldItemcnt]),InitP,sizeof(c_type)*NumOfAdditionalElements);             \
    }                                                                                                       \
}                                                                                                           \
static Dl_##name* Dl_##name##_shallowCopy(Dl_##name* ToCopyDlP){                                            \
    return Dl_##name##_alloc(ToCopyDlP->itemcnt,ToCopyDlP->items);                                          \
}                                                                                                           \
static Dl_##name* Dl_##name##_mergeDelete(Dl_##name* FirstDlP,Dl_##name* SecondDlP){                        \
    Dl_##name##_append(FirstDlP,SecondDlP->itemcnt,SecondDlP->items);                                       \
    Dl_##name##_delete(SecondDlP);                                                                          \
    return FirstDlP;                                                                                        \
}                                                                                                           \
static Dl_##name* Dl_##name##_mergeDulplicate(Dl_##name* FirstDlP,Dl_##name* SecondDlP){                    \
    Dl_##name* newDlP=Dl_##name##_alloc(FirstDlP->itemcnt+SecondDlP->itemcnt,0);                            \
    if(FirstDlP->itemcnt){                                                                                  \
        memcpy(newDlP->items,FirstDlP->items,sizeof(c_type)*FirstDlP->itemcnt);                             \
    }                                                                                                       \
    if(SecondDlP->itemcnt){                                                                                 \
        memcpy(&(newDlP->items[FirstDlP->itemcnt]),SecondDlP->items,sizeof(c_type)*SecondDlP->itemcnt);     \
    }                                                                                                       \
    return newDlP;                                                                                          \
}                                                                                                           \
static Dl_##name* Dl_##name##_subList(Dl_##name* CompleteDlP,int32_t startIdx,int32_t endIdx){              \
    if(startIdx<0){                                                                                         \
        startIdx=CompleteDlP->itemcnt+startIdx;                                                             \
    }                                                                                                       \
    if(endIdx<0){                                                                                           \
        endIdx=CompleteDlP->itemcnt+endIdx;                                                                 \
    }                                                                                                       \
    startIdx=clamp_int32(0,startIdx,CompleteDlP->itemcnt-1);                                                \
    endIdx=clamp_int32(0,endIdx,CompleteDlP->itemcnt-1);                                                    \
    int32_t newStringLength=1+endIdx-startIdx;                                                              \
    return Dl_##name##_alloc(newStringLength,&(CompleteDlP->items[startIdx]));                              \
}

#define DlTypedef_nested(name,c_type,childname)                                                             \
DlTypedef_generic(name,c_type)                                                                              \
static void Dl_##name##_delete(Dl_##name* ToDeleteDlP){                                                     \
    for(size_t subitem=0;subitem<ToDeleteDlP->itemcnt;subitem++){                                           \
        Dl_##childname##_delete(ToDeleteDlP->items[subitem]);                                               \
    }                                                                                                       \
    if(ToDeleteDlP->itemcnt){                                                                               \
        free(ToDeleteDlP->items);                                                                           \
    }                                                                                                       \
    free(ToDeleteDlP);                                                                                      \
}

#define DlTypedef_plain(name,c_type)                                                                        \
DlTypedef_generic(name,c_type)                                                                              \
static void Dl_##name##_delete(Dl_##name* ToDeleteDlP){                                                     \
    if(ToDeleteDlP->itemcnt){                                                                               \
        free(ToDeleteDlP->items);                                                                           \
    }                                                                                                       \
    free(ToDeleteDlP);                                                                                      \
}

#define DlTypedef_compareFunc(name,c_type)                                                                  \
static int Dl_##name##_equal(Dl_##name* FirstDlP,Dl_##name* SecondDlP){                                     \
    if(!(FirstDlP->itemcnt) || !(SecondDlP->itemcnt)){                                                      \
        return 0;                                                                                           \
    }                                                                                                       \
    if(FirstDlP->itemcnt!=SecondDlP->itemcnt){                                                              \
        return 0;                                                                                           \
    }                                                                                                       \
    for(size_t idx=0;idx<FirstDlP->itemcnt;idx++){                                                          \
        if(FirstDlP->items[idx]!=SecondDlP->items[idx]){                                                    \
            return 0;                                                                                       \
        }                                                                                                   \
    }                                                                                                       \
    return 1;                                                                                               \
}                                                                                                           \
static int Dl_##name##_equal_freeArg2(Dl_##name* FirstDlP,Dl_##name* SecondDlP){                            \
    int result=Dl_##name##_equal(FirstDlP,SecondDlP);                                                       \
    Dl_##name##_delete(SecondDlP);                                                                          \
    return result;                                                                                          \
}
