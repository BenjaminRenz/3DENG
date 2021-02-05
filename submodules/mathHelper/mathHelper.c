#include <stdint.h>

uint32_t max_uint32_t(uint32_t a, uint32_t b){
    if(a>b){
        return a;
    }else{
        return b;
    }
}

uint32_t min_uint32_t(uint32_t a, uint32_t b){
    if(a<b){
        return a;
    }else{
        return b;
    }
}

int32_t max_int32_t(int32_t a, int32_t b){
    if(a>b){
        return a;
    }else{
        return b;
    }
}

int32_t min_int32_t(int32_t a, int32_t b){
    if(a<b){
        return a;
    }else{
        return b;
    }
}

uint32_t clamp_uint32_t(uint32_t lower_bound,uint32_t clampedValueInput,uint32_t upper_bound){
    return max_uint32_t(min_uint32_t(upper_bound,clampedValueInput),lower_bound);
}

int32_t clamp_int32_t(int32_t lower_bound,int32_t clampedValueInput,int32_t upper_bound){
    return max_int32_t(min_int32_t(upper_bound,clampedValueInput),lower_bound);
}


uint32_t countBitsInUint32(uint32_t input){
    //add subbits in increasing bin sizes (bit0+bit1),(bit2+bit3),...
    input=(input&0x55555555)+((input>>1)&0x55555555);
    input=(input&0x33333333)+((input>>2)&0x33333333);
    input=(input&0x0f0f0f0f)+((input>>4)&0x0f0f0f0f);
    input=(input&0x00ff00ff)+((input>>8)&0x00ff00ff);
    input=(input&0x0000ffff)+(input>>16);
    return input;
}
