
#pragma once

#include "core/math/math_defs.h"
#include "core/object/object.h"
#include "core/typedefs.h"
#include "core/variant/type_info.h"
class SerialType : public Object {
    GDCLASS(SerialType, Object);

private:
    _FORCE_INLINE_ static uint32_t float_as_uint32(float f) {
        uint32_t bits;
        memcpy(&bits, &f, sizeof(bits));
        return bits;
    }
    _FORCE_INLINE_ static float uint32_as_float(uint32_t bits) {
        float f;
        std::memcpy(&f, &bits, sizeof(f));
        return f;
    }
protected:
    static void _bind_methods();

public:
    SerialType();
    ~SerialType();

    enum SERIAL_TYPE {
        BOOL,
        U8,
        I8,
        U16,
        I16,
        U32,
        I32,
        U64,
        I64,
        F16,
        F32,
        F64,
        _S_INVALID,
        F_REAL = sizeof(real_t) == 4 ? F32 : F64,
    };
    static constexpr uint32_t SERIAL_SIZE[] = {
        1, //BOOL
        1, //U8
        1, //I8
        2, //U16
        2, //I16
        4, //U32
        4, //I32
        8, //U64
        8, //I64
        2, //F16
        sizeof(float), //F32
        sizeof(double), //F64
        sizeof(real_t), //F_REAL
    };
    enum NATIVE_TYPE {
        BOOLEAN,
        INTEGER,
        FLOAT,
        VEC2,
        VEC2I,
        VEC3,
        VEC3I,
        VEC4,
        VEC4I,
        RECT2,
        RECT2I,
        COLOR,
        TRANSFORM2D,
        TRANSFORM3D,
        PLANE,
        BASIS,
        PROJECTION,
        QUATERNION,
        RECT3,
        _N_INVALID,
    };
    static constexpr uint32_t NATIVE_ELEM_COUNT[] = {
        1, // BOOL,
        1, // INTEGER,
        1, // FLOAT,
        2, // VEC2,
        2, // VEC2I,
        3, // VEC3,
        3, // VEC3I,
        4, // VEC4,
        4, // VEC4I,
        4, // RECT2,
        4, // RECT2I,
        4, // COLOR,
        6, // TRANSFORM2D,
        12, // TRANSFORM3D,
        4, // PLANE,
        9, // BASIS,
        16, // PROJECTION,
        4, // QUATERNION,
        6, // RECT3,
    };
    enum ARRAY_TYPE {
        SINGLE_VALUE,
        VARIANT_ARRAY,
        PACKED_ARRAY,
        _A_INVALID,
    };
    static constexpr uint32_t STRUCT_NATIVE_ELEMS[] = {
        BOOL, // BOOL,
        I64, // INTEGER,
        F64, // FLOAT,
        F_REAL, // VEC2,
        I32, // VEC2I,
        F_REAL, // VEC3,
        I32, // VEC3I,
        F_REAL, // VEC4,
        I32, // VEC4I,
        F_REAL, // RECT2,
        I32, // RECT2I,
        F32, // COLOR,
        F_REAL, // TRANSFORM2D,
        F_REAL, // TRANSFORM3D,
        F_REAL, // PLANE,
        F_REAL, // BASIS,
        F_REAL, // PROJECTION,
        F_REAL, // QUATERNION,
        F_REAL, // RECT3,
    };
    _FORCE_INLINE_ constexpr static uint32_t serial_elem_size(SERIAL_TYPE t_elem) {
        return SERIAL_SIZE[t_elem];
    }
    _FORCE_INLINE_ constexpr static uint32_t native_elem_count(NATIVE_TYPE t_native) {
        return NATIVE_ELEM_COUNT[t_native];
    }
    _FORCE_INLINE_ constexpr static uint32_t native_elem_type(NATIVE_TYPE t_native) {
        return STRUCT_NATIVE_ELEMS[t_native];
    }
    _FORCE_INLINE_ constexpr static uint32_t native_elem_size(NATIVE_TYPE t_native) {
        return SERIAL_SIZE[STRUCT_NATIVE_ELEMS[t_native]];
    }
    _FORCE_INLINE_ constexpr static uint32_t serial_stride(SERIAL_TYPE t_serial, NATIVE_TYPE t_native) {
        return SERIAL_SIZE[t_serial] * NATIVE_ELEM_COUNT[t_native];
    }
    _FORCE_INLINE_ constexpr static uint32_t native_stride(NATIVE_TYPE t_native) {
        return SERIAL_SIZE[STRUCT_NATIVE_ELEMS[t_native]] * NATIVE_ELEM_COUNT[t_native];
    }

    static uint16_t float_to_half_u16(float f);
    _FORCE_INLINE_ static uint16_t double_to_half_u16(double d) {
        return float_to_half_u16(static_cast<float>(d));
    }
    _FORCE_INLINE_ static uint16_t real_to_half_u16(real_t r) {
        return float_to_half_u16(static_cast<float>(r));
    }

    static float half_u16_to_float(uint16_t h);
    _FORCE_INLINE_ static double half_u16_to_double(uint16_t h) {
        return static_cast<double>(half_u16_to_float(h));
    }
    _FORCE_INLINE_ static real_t half_u16_to_real(uint16_t h) {
        return static_cast<real_t>(half_u16_to_float(h));
    }
};

class HalfU16 {
public:
    uint16_t raw = 0;

    HalfU16() = default;

    _FORCE_INLINE_ HalfU16(float f) {
        raw = SerialType::float_to_half_u16(f);
    }
    _FORCE_INLINE_ operator float() const {
        return SerialType::half_u16_to_float(raw);
    }
};

VARIANT_ENUM_CAST(SerialType::SERIAL_TYPE);
VARIANT_ENUM_CAST(SerialType::NATIVE_TYPE);
VARIANT_ENUM_CAST(SerialType::ARRAY_TYPE);
