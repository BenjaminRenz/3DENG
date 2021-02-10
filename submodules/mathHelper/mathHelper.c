#include <stdint.h>

uint32_t countBitsInUint32(uint32_t input){
    //add subbits in increasing bin sizes (bit0+bit1),(bit2+bit3),...
    input=(input&0x55555555)+((input>>1)&0x55555555);
    input=(input&0x33333333)+((input>>2)&0x33333333);
    input=(input&0x0f0f0f0f)+((input>>4)&0x0f0f0f0f);
    input=(input&0x00ff00ff)+((input>>8)&0x00ff00ff);
    input=(input&0x0000ffff)+(input>>16);
    return input;
}


#define defineMinMaxForType(name,c_type)\
c_type min_##name(c_type a, c_type b){  \
    if(a<b){                            \
        return a;                       \
    }else{                              \
        return b;                       \
    }                                   \
}                                       \
c_type max_##name(c_type a, c_type b){  \
    if(a>b){                            \
        return a;                       \
    }else{                              \
        return b;                       \
    }                                   \
}

#define defineClampForType(name,c_type)                                 \
c_type clamp_##name(c_type lower,c_type clamped_value, c_type upper){   \
    return max_##name(min_##name(upper,clamped_value),lower);           \
}

#define MATHHELPER_C
#include "mathHelper/mathHelper.h"
