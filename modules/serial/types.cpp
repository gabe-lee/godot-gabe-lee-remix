
#include "types.h"

SerialType::SerialType() {
    ERR_FAIL_MSG("SerialType is a static-only class and connot be instantiated");
}
SerialType::~SerialType() {
}

void SerialType::_bind_methods() {
    
}

uint16_t SerialType::float_to_half_u16(float f) {
    uint32_t ia = float_as_uint32(f);
    
    uint16_t ir = (ia >> 16) & 0x8000;

    if ((ia & 0x7f800000) == 0x7f800000) {
        if ((ia & 0x7fffffff) == 0x7f800000) {
            ir |= 0x7c00;
        } else {
            ir |= 0x7e00 | ((ia >> 13) & 0x01ff);
        }
    } 
    else if ((ia & 0x7f800000) >= 0x33000000) {
        int shift = static_cast<int>((ia >> 23) & 0xff) - 127;
        
        if (shift > 15) { 
            ir |= 0x7c00; 
        } else {
            ia = (ia & 0x007fffff) | 0x00800000; 
            
            if (shift < -14) { 
                ir |= ia >> (-1 - shift);
                ia = ia << (32 - (-1 - shift));
            } else { 
                ir |= ia >> 13;
                ia = ia << 19;
                ir += static_cast<uint16_t>((14 + shift) << 10);
            }
            
            if ((ia > 0x80000000) || ((ia == 0x80000000) && (ir & 1))) {
                ir++;
            }
        }
    }
    return ir;
}

float SerialType::half_u16_to_float(uint16_t h) {
    const uint32_t magic_bits = (254 - 15) << 23; 
    const float magic = uint32_as_float(magic_bits);
    
    const uint32_t was_infnan_bits = (127 + 16) << 23;
    const float was_infnan = uint32_as_float(was_infnan_bits);

    uint32_t bits = (h & 0x7fff) << 13;
    float f = uint32_as_float(bits);
    
    f *= magic;

    std::memcpy(&bits, &f, sizeof(bits));

    if (f >= was_infnan) {
        bits |= 255 << 23;
    }

    bits |= (h & 0x8000) << 16;

    return uint32_as_float(bits);
}