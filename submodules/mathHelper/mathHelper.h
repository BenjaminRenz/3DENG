#ifndef MATHHELPER_H_INCLUDED
#define MATHHELPER_H_INCLUDED
#include <stdint.h>
//hacky solution so the source file can redefine the macros below
//this has the advantage the the define...(typename,type) calls can be isolated to the header file
#ifndef MATHHELPER_C

#define defineMinMaxForType(name,c_type)      \
c_type min_##name(c_type a, c_type b);  \
c_type max_##name(c_type a, c_type b);

#define defineClampForType(name,c_type)                                       \
c_type clamp_##name(c_type lower,c_type clamped_value, c_type upper);
uint32_t countBitsInUint32(uint32_t input);

#endif // MATHHELPER_C

defineMinMaxForType(uint32,uint32_t)
defineMinMaxForType(int32,int32_t)
defineMinMaxForType(float,float)
defineClampForType(uint32,uint32_t)
defineClampForType(int32,int32_t)
defineClampForType(float,float)

#endif //MATHHELPER_H_INCLUDED
