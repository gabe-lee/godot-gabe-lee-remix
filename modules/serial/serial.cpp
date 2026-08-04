
#include "serial.h"
#include "core/error/error_macros.h"
#include "core/io/stream_peer.h"
#include "core/math/aabb.h"
#include "core/math/color.h"
#include "core/math/math_defs.h"
#include "core/math/projection.h"
#include "core/math/quaternion.h"
#include "core/math/rect2.h"
#include "core/math/transform_2d.h"
#include "core/math/vector2.h"
#include "core/object/class_db.h"
#include "core/os/memory.h"
#include "core/string/ustring.h"
#include "core/templates/local_vector.h"
#include "core/templates/span.h"
#include "core/typedefs.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include <cstdint>
#include <type_traits>

#define ADD_ERROR_RETURN_FALSE(m_err) { \
    add_error(m_err); \
    return false; \
}

#define ADD_ERROR_IF(m_cond, m_err) if (m_cond) { \
    add_error(m_err); \
}
#define ELSE_ADD_ERROR_IF(m_cond, m_err) else if (m_cond) { \
    add_error(m_err); \
}

#define ADD_ERROR_RETURN_FALSE_IF(m_cond, m_err) if (m_cond) { \
    add_error(m_err); \
    return false; \
}

#define ADD_ERROR_RETURN_ZERO_IF(m_cond, m_err) if (m_cond) { \
    add_error(m_err); \
    return 0; \
}

#define ADD_ERROR_RETURN_VARIANT_IF(m_cond, m_err) if (m_cond) { \
    add_error(m_err); \
    return Variant(); \
}

#define RW_ERR_FAIL_V_MSG(m_err, m_ret, m_msg) \
add_error(m_err);\
ERR_FAIL_V_MSG(m_ret, m_msg);

#define RW_ERR_FAIL_COND_V_MSG(m_cond, m_err, m_ret, m_msg) \
if (cond) {\
    add_error(m_err);\
    ERR_FAIL_V_MSG(m_ret, m_msg); \
}

uint16_t HalfU16::float_to_half_u16(float f) {
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

float HalfU16::half_u16_to_float(uint16_t h) {
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

void ReaderWriter::add_result(int64_t p_seek_delta, uint32_t p_bytes_read_or_written, uint8_t p_error) {
    add_error(p_error);
    last_seek_delta = p_seek_delta;
    last_bytes_copied = p_bytes_read_or_written;
}

void ReaderWriter::add_error(uint8_t p_error) {
    if (p_error != ERROR::NONE) {
        if (first_error == ERROR::NONE) {
            first_error = p_error;
        }
        num_errors += 1;
        num_errors_this_op += 1;
        last_error = p_error;
    }
}

void ReaderWriter::_bind_methods() {
    BIND_ENUM_CONSTANT(SEEK::FROM_CURRENT);
    BIND_ENUM_CONSTANT(SEEK::FROM_END);
    BIND_ENUM_CONSTANT(SEEK::FROM_START);

    BIND_ENUM_CONSTANT(ERROR::NONE);
    BIND_ENUM_CONSTANT(ERROR::INVALID_STATE);
    BIND_ENUM_CONSTANT(ERROR::INVALID_ENUM_INPUT);
    BIND_ENUM_CONSTANT(ERROR::INVALID_TYPE_TAG);
    BIND_ENUM_CONSTANT(ERROR::READ_ERROR);
    BIND_ENUM_CONSTANT(ERROR::WRITE_ERROR);
    BIND_ENUM_CONSTANT(ERROR::OUT_OF_DATA_TO_READ);
    BIND_ENUM_CONSTANT(ERROR::OUT_OF_SPACE_TO_WRITE);
    BIND_ENUM_CONSTANT(ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT);
    BIND_ENUM_CONSTANT(ERROR::CANNOT_SEEK_READ);
    BIND_ENUM_CONSTANT(ERROR::CANNOT_SEEK_WRITE);
    BIND_ENUM_CONSTANT(ERROR::CANNOT_READ);
    BIND_ENUM_CONSTANT(ERROR::CANNOT_WRITE);
    BIND_ENUM_CONSTANT(ERROR::CANNOT_GET);
    BIND_ENUM_CONSTANT(ERROR::CANNOT_SET);
    BIND_ENUM_CONSTANT(ERROR::SEEK_ERROR);
    BIND_ENUM_CONSTANT(ERROR::SEEK_AFTER_DATA_RANGE);
    BIND_ENUM_CONSTANT(ERROR::SEEK_BEFORE_DATA_RANGE);

    BIND_ENUM_CONSTANT(GODOT_TYPE::BOOLEAN);
    BIND_ENUM_CONSTANT(GODOT_TYPE::INTEGER);
    BIND_ENUM_CONSTANT(GODOT_TYPE::FLOAT);
    BIND_ENUM_CONSTANT(GODOT_TYPE::VEC_2);
    BIND_ENUM_CONSTANT(GODOT_TYPE::VEC_2I);
    BIND_ENUM_CONSTANT(GODOT_TYPE::VEC_3);
    BIND_ENUM_CONSTANT(GODOT_TYPE::VEC_3I);
    BIND_ENUM_CONSTANT(GODOT_TYPE::VEC_4);
    BIND_ENUM_CONSTANT(GODOT_TYPE::VEC_4I);
    BIND_ENUM_CONSTANT(GODOT_TYPE::COLOR);
    BIND_ENUM_CONSTANT(GODOT_TYPE::RECT_2);
    BIND_ENUM_CONSTANT(GODOT_TYPE::RECT_2I);
    BIND_ENUM_CONSTANT(GODOT_TYPE::AABB);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PLANE);
    BIND_ENUM_CONSTANT(GODOT_TYPE::BASIS);
    BIND_ENUM_CONSTANT(GODOT_TYPE::TRANSFORM_2D);
    BIND_ENUM_CONSTANT(GODOT_TYPE::TRANSFORM_3D);
    BIND_ENUM_CONSTANT(GODOT_TYPE::QUATERNION);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PROJECTION);
    BIND_ENUM_CONSTANT(GODOT_TYPE::STRING);
    BIND_ENUM_CONSTANT(GODOT_TYPE::DICTIONARY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::ARRAY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PACKED_BYTE_ARRAY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PACKED_INT32_ARRAY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PACKED_INT64_ARRAY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PACKED_FLOAT32_ARRAY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PACKED_FLOAT64_ARRAY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PACKED_STRING_ARRAY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PACKED_VECTOR2_ARRAY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PACKED_VECTOR3_ARRAY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PACKED_VECTOR4_ARRAY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::PACKED_COLOR_ARRAY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::ANY);
    BIND_ENUM_CONSTANT(GODOT_TYPE::RECT_3);
    
    BIND_ENUM_CONSTANT(SERIAL_TYPE::BOOL);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::U8);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::I8);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::U16);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::I16);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::U32);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::I32);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::U64);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::I64);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::F16);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::F32);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::F64);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::DEFAULT);
    BIND_ENUM_CONSTANT(SERIAL_TYPE::REAL);

    ClassDB::bind_method(D_METHOD("clear_errors"), &ReaderWriter::clear_errors);
    ClassDB::bind_method(D_METHOD("clear_deltas"), &ReaderWriter::clear_deltas);
    ClassDB::bind_method(D_METHOD("has_errors"), &ReaderWriter::has_errors);
    ClassDB::bind_method(D_METHOD("get_first_error"), &ReaderWriter::get_first_error);
    ClassDB::bind_method(D_METHOD("get_last_error"), &ReaderWriter::get_last_error);
    ClassDB::bind_method(D_METHOD("get_num_errors"), &ReaderWriter::get_num_errors);
    ClassDB::bind_method(D_METHOD("get_last_seek_delta"), &ReaderWriter::get_last_seek_delta);
    ClassDB::bind_method(D_METHOD("get_last_bytes_copied"), &ReaderWriter::get_last_bytes_copied);
    ClassDB::bind_method(D_METHOD("add_result", "seek_delta", "bytes_read_or_written", "error"), &ReaderWriter::add_result);
    ClassDB::bind_method(D_METHOD("add_error", "error"), &ReaderWriter::add_error);

    ClassDB::bind_method(D_METHOD("get_read_pos"), &ReaderWriter::get_read_pos);
    ClassDB::bind_method(D_METHOD("get_write_pos"), &ReaderWriter::get_write_pos);
    ClassDB::bind_method(D_METHOD("seek_read_pos", "delta_bytes", "from"), &ReaderWriter::seek_read_pos);
    ClassDB::bind_method(D_METHOD("seek_write_pos", "delta_bytes", "from"), &ReaderWriter::seek_write_pos);
    ClassDB::bind_method(D_METHOD("get_var", "type", "serial_type"), &ReaderWriter::get_gds);
    ClassDB::bind_method(D_METHOD("read_var", "type", "serial_type"), &ReaderWriter::read_gds);
    ClassDB::bind_method(D_METHOD("set_var", "type", "val", "serial_type"), &ReaderWriter::set_gds);
    ClassDB::bind_method(D_METHOD("write_var", "type", "val", "serial_type"), &ReaderWriter::write_gds);
}

void ReaderWriter::clear_errors() {
    first_error = ERROR::NONE;
    last_error = ERROR::NONE;
    num_errors_this_op = 0;
    num_errors = 0;
}

void ReaderWriter::clear_deltas() {
    last_bytes_copied = 0;
    last_seek_delta = 0;
    num_errors_this_op = 0;
}

bool ReaderWriter::has_errors() {
    return first_error != ERROR::NONE;
}

uint8_t ReaderWriter::get_first_error() {
    return num_errors == 0;
}

uint8_t ReaderWriter::get_last_error() {
    return last_error;
}

uint32_t ReaderWriter::get_num_errors() {
    return num_errors;
}

int64_t ReaderWriter::get_last_seek_delta() {
    return last_seek_delta;
}
uint32_t ReaderWriter::get_last_bytes_copied() {
    return last_bytes_copied;
}

bool ReaderWriter::get_bytes(void* data_dst, uint32_t num_bytes) {
    return read_bytes(data_dst, num_bytes, true, false);
}

bool ReaderWriter::set_bytes(const void* data_src, uint32_t num_bytes) {
    return write_bytes(data_src, num_bytes, true, false);
}

template<typename T>
bool ReaderWriter::seek_read_by_t_size() {
    return seek_read_pos(sizeof(T), SEEK::FROM_CURRENT);
}

template<typename T>
bool ReaderWriter::seek_write_by_t_size() {
    return seek_write_pos(sizeof(T), SEEK::FROM_CURRENT);
}

template<typename T>
bool ReaderWriter::read_t(T* val_dst) {
    return read_bytes(reinterpret_cast<void*>(val_dst), sizeof(T));
}

template<typename T>
bool ReaderWriter::set_t(const T* val_src) {
    return set_bytes(reinterpret_cast<const void*>(val_src), sizeof(T));
}

template<typename T>
bool ReaderWriter::write_t(const T* val_src) {
    return write_bytes(reinterpret_cast<const void*>(val_src), sizeof(T));
}

template<typename T>
T ReaderWriter::get_t_val() {
    T val;
    get_bytes(reinterpret_cast<void*>(&val), sizeof(T));
    return val;
}

template<typename T>
T ReaderWriter::read_t_val() {
    T val;
    read_bytes(reinterpret_cast<void*>(&val), sizeof(T));
    return val;
}

template<typename T>
bool ReaderWriter::set_t_val(T val) {
    return set_bytes(reinterpret_cast<const void*>(&val), sizeof(T));
}

template<typename T>
bool ReaderWriter::write_t_val(T val) {
    return write_bytes(reinterpret_cast<const void*>(&val), sizeof(T));
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::get_t_cast(T_NATIVE* val_dst) {
    T_SERIAL val_ser;
    get_bytes(reinterpret_cast<void*>(&val_ser), sizeof(T_SERIAL))
    *val_dst = static_cast<T_NATIVE>(val_ser);
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::read_t_cast(T_NATIVE* val_dst) {
    T_SERIAL val_ser;
    read_bytes(reinterpret_cast<void*>(&val_ser), sizeof(T_SERIAL));
    *val_dst = static_cast<T_NATIVE>(val_ser);
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::set_t_cast(const T_NATIVE* val_src) {
    T_SERIAL val_ser = static_cast<T_SERIAL>(*val_src);
    return set_bytes(reinterpret_cast<const void*>(&val_ser), sizeof(T_SERIAL));
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::write_t_cast(const T_NATIVE* val_src) {
    T_SERIAL val_ser = static_cast<T_SERIAL>(*val_src);
    return write_bytes(reinterpret_cast<const void*>(&val_ser), sizeof(T_SERIAL));
}

template<typename T_SERIAL, typename T_NATIVE>
T_NATIVE ReaderWriter::get_t_cast_val() {
    T_SERIAL val_ser;
    get_bytes(reinterpret_cast<void*>(&val_ser), sizeof(T_SERIAL));
    return static_cast<T_NATIVE>(val_ser);
}

template<typename T_SERIAL, typename T_NATIVE>
T_NATIVE ReaderWriter::read_t_cast_val() {
    T_SERIAL val_ser;
    read_bytes(reinterpret_cast<void*>(&val_ser), sizeof(T_SERIAL));
    return static_cast<T_NATIVE>(val_ser);
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::set_t_cast_val(T_NATIVE val_src) {
    T_SERIAL val_ser = static_cast<T_SERIAL>(val_src);
    return set_bytes(reinterpret_cast<const void*>(&val_ser), sizeof(T_SERIAL));
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::write_t_cast_val(T_NATIVE val_src) {
    T_SERIAL val_ser = static_cast<T_SERIAL>(val_src);
    return write_bytes(reinterpret_cast<const void*>(&val_ser), sizeof(T_SERIAL));
}

template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
T_NATIVE ReaderWriter::get_gds_impl() {
    int64_t initial_pos = get_read_pos();
    T_NATIVE out = read_gds_impl<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>();
    seek_read_pos(initial_pos, SEEK::FROM_START);
    return out;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
T_NATIVE ReaderWriter::read_gds_impl() {
    T_NATIVE out;
    if constexpr (std::is_same_v<T_SERIAL, T_NATIVE>) {
        read_bytes(&out, sizeof(T_NATIVE));
    } else if constexpr (std::is_same_v<T_NATIVE, int64_t> || std::is_same_v<T_NATIVE, double> || std::is_same_v<T_NATIVE, bool>) {
        read_t_cast<T_SERIAL>(&out);
    } else {
        T_NATIVE_ELEM* elem_ptr = reinterpret_cast<T_NATIVE_ELEM*>(&out);
        for (uint32_t e = 0; e < T_NATIVE_ELEM_COUNT; e += 1) {
            read_t_cast<T_SERIAL>(elem_ptr);
            elem_ptr += 1;
        }
    }
    return out;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
bool ReaderWriter::set_gds_impl(T_NATIVE val) {
    int64_t initial_pos = get_write_pos();
    bool err = write_gds_impl<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(val);
    seek_write_pos(initial_pos, SEEK::FROM_START);
    return err;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
bool ReaderWriter::write_gds_impl(T_NATIVE val) {
    if constexpr (std::is_same_v<T_SERIAL, T_NATIVE>) {
        write_bytes(&val, sizeof(T_NATIVE));
    } else if constexpr (std::is_same_v<T_NATIVE, int64_t> || std::is_same_v<T_NATIVE, double> || std::is_same_v<T_NATIVE, bool>) {
        write_t_cast<T_SERIAL>(&val);
    } else {
        T_NATIVE_ELEM* elem_ptr = reinterpret_cast<T_NATIVE_ELEM*>(&val);
        for (uint32_t e = 0; e < T_NATIVE_ELEM_COUNT; e += 1) {
            write_t_cast<T_SERIAL>(elem_ptr);
            elem_ptr += 1;
        }
    }
    return num_errors == 0;
}

Variant ReaderWriter::get_gds(GODOT_TYPE type, SERIAL_TYPE serial_type) {
    int64_t initial_pos = get_read_pos();
    Variant out = read_gds(type, serial_type);
    seek_read_pos(initial_pos, SEEK::FROM_START);
    return out;
}
Variant ReaderWriter::read_gds(GODOT_TYPE type, SERIAL_TYPE serial_type) {
    ERR_FAIL_COND_V_MSG(type < _GD_TYPE_MIN || type >= _GD_TYPE_LIMIT, Variant(), "invalid godot type for serialization");
    ERR_FAIL_COND_V_MSG(serial_type < _SERIAL_TYPE_MIN || serial_type >= _SERIAL_TYPE_LIMIT, Variant(), "invalid serial type");
    if (type == ANY) {
        uint32_t type_tag = static_cast<uint32_t>(read_t_val<uint8_t>());
        if (type_tag < _GD_TYPE_MIN || type_tag >= ANY) {
            add_error(ERROR::INVALID_TYPE_TAG);
            ERR_FAIL_V_MSG(Variant(), "invalid type tag in serialized data");
        }
        type = (GODOT_TYPE)type_tag;
    }
    if (serial_type == SERIAL_TYPE::DEFAULT) {
        serial_type = (SERIAL_TYPE)GODOT_DEFAULT_ELEM[type - _GD_TYPE_MIN];
    }
    switch (type) {
        case GODOT_TYPE::BOOLEAN: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, bool, bool, 1>());
                case U8: return Variant(read_gds_impl<uint8_t, bool, bool, 1>());
                case I8: return Variant(read_gds_impl<int8_t, bool, bool, 1>());
                case U16: return Variant(read_gds_impl<uint16_t, bool, bool, 1>());
                case I16: return Variant(read_gds_impl<int16_t, bool, bool, 1>());
                case U32: return Variant(read_gds_impl<uint32_t, bool, bool, 1>());
                case I32: return Variant(read_gds_impl<int32_t, bool, bool, 1>());
                case U64: return Variant(read_gds_impl<uint64_t, bool, bool, 1>());
                case I64: return Variant(read_gds_impl<int64_t, bool, bool, 1>());
                case F16: return Variant(read_gds_impl<HalfU16, bool, bool, 1>());
                case F32: return Variant(read_gds_impl<float, bool, bool, 1>());
                case F64: return Variant(read_gds_impl<double, bool, bool, 1>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot boolean");
            }
            break;
        }
        case GODOT_TYPE::INTEGER: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, int64_t, int64_t, 1>());
                case U8: return Variant(read_gds_impl<uint8_t, int64_t, int64_t, 1>());
                case I8: return Variant(read_gds_impl<int8_t, int64_t, int64_t, 1>());
                case U16: return Variant(read_gds_impl<uint16_t, int64_t, int64_t, 1>());
                case I16: return Variant(read_gds_impl<int16_t, int64_t, int64_t, 1>());
                case U32: return Variant(read_gds_impl<uint32_t, int64_t, int64_t, 1>());
                case I32: return Variant(read_gds_impl<int32_t, int64_t, int64_t, 1>());
                case U64: return Variant(read_gds_impl<uint64_t, int64_t, int64_t, 1>());
                case I64: return Variant(read_gds_impl<int64_t, int64_t, int64_t, 1>());
                case F16: return Variant(read_gds_impl<HalfU16, int64_t, int64_t, 1>());
                case F32: return Variant(read_gds_impl<float, int64_t, int64_t, 1>());
                case F64: return Variant(read_gds_impl<double, int64_t, int64_t, 1>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot integer");
            }
            break;
        }
        case GODOT_TYPE::FLOAT: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, double, double, 1>());
                case U8: return Variant(read_gds_impl<uint8_t, double, double, 1>());
                case I8: return Variant(read_gds_impl<int8_t, double, double, 1>());
                case U16: return Variant(read_gds_impl<uint16_t, double, double, 1>());
                case I16: return Variant(read_gds_impl<int16_t, double, double, 1>());
                case U32: return Variant(read_gds_impl<uint32_t, double, double, 1>());
                case I32: return Variant(read_gds_impl<int32_t, double, double, 1>());
                case U64: return Variant(read_gds_impl<uint64_t, double, double, 1>());
                case I64: return Variant(read_gds_impl<int64_t, double, double, 1>());
                case F16: return Variant(read_gds_impl<HalfU16, double, double, 1>());
                case F32: return Variant(read_gds_impl<float, double, double, 1>());
                case F64: return Variant(read_gds_impl<double, double, double, 1>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot float");
            }
            break;
        }
        case GODOT_TYPE::VEC_2: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Vector2, real_t, 2>());
                case U8: return Variant(read_gds_impl<uint8_t, Vector2, real_t, 2>());
                case I8: return Variant(read_gds_impl<int8_t, Vector2, real_t, 2>());
                case U16: return Variant(read_gds_impl<uint16_t, Vector2, real_t, 2>());
                case I16: return Variant(read_gds_impl<int16_t, Vector2, real_t, 2>());
                case U32: return Variant(read_gds_impl<uint32_t, Vector2, real_t, 2>());
                case I32: return Variant(read_gds_impl<int32_t, Vector2, real_t, 2>());
                case U64: return Variant(read_gds_impl<uint64_t, Vector2, real_t, 2>());
                case I64: return Variant(read_gds_impl<int64_t, Vector2, real_t, 2>());
                case F16: return Variant(read_gds_impl<HalfU16, Vector2, real_t, 2>());
                case F32: return Variant(read_gds_impl<float, Vector2, real_t, 2>());
                case F64: return Variant(read_gds_impl<double, Vector2, real_t, 2>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Vector2");
            }
            break;
        }
        case GODOT_TYPE::VEC_2I: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Vector2i, int32_t, 2>());
                case U8: return Variant(read_gds_impl<uint8_t, Vector2i, int32_t, 2>());
                case I8: return Variant(read_gds_impl<int8_t, Vector2i, int32_t, 2>());
                case U16: return Variant(read_gds_impl<uint16_t, Vector2i, int32_t, 2>());
                case I16: return Variant(read_gds_impl<int16_t, Vector2i, int32_t, 2>());
                case U32: return Variant(read_gds_impl<uint32_t, Vector2i, int32_t, 2>());
                case I32: return Variant(read_gds_impl<int32_t, Vector2i, int32_t, 2>());
                case U64: return Variant(read_gds_impl<uint64_t, Vector2i, int32_t, 2>());
                case I64: return Variant(read_gds_impl<int64_t, Vector2i, int32_t, 2>());
                case F16: return Variant(read_gds_impl<HalfU16, Vector2i, int32_t, 2>());
                case F32: return Variant(read_gds_impl<float, Vector2i, int32_t, 2>());
                case F64: return Variant(read_gds_impl<double, Vector2i, int32_t, 2>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Vector2i");
            }
            break;
        }
        case GODOT_TYPE::VEC_3: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Vector3, real_t, 3>());
                case U8: return Variant(read_gds_impl<uint8_t, Vector3, real_t, 3>());
                case I8: return Variant(read_gds_impl<int8_t, Vector3, real_t, 3>());
                case U16: return Variant(read_gds_impl<uint16_t, Vector3, real_t, 3>());
                case I16: return Variant(read_gds_impl<int16_t, Vector3, real_t, 3>());
                case U32: return Variant(read_gds_impl<uint32_t, Vector3, real_t, 3>());
                case I32: return Variant(read_gds_impl<int32_t, Vector3, real_t, 3>());
                case U64: return Variant(read_gds_impl<uint64_t, Vector3, real_t, 3>());
                case I64: return Variant(read_gds_impl<int64_t, Vector3, real_t, 3>());
                case F16: return Variant(read_gds_impl<HalfU16, Vector3, real_t, 3>());
                case F32: return Variant(read_gds_impl<float, Vector3, real_t, 3>());
                case F64: return Variant(read_gds_impl<double, Vector3, real_t, 3>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Vector3");
            }
            break;
        }
        case GODOT_TYPE::VEC_3I: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Vector3i, int32_t, 3>());
                case U8: return Variant(read_gds_impl<uint8_t, Vector3i, int32_t, 3>());
                case I8: return Variant(read_gds_impl<int8_t, Vector3i, int32_t, 3>());
                case U16: return Variant(read_gds_impl<uint16_t, Vector3i, int32_t, 3>());
                case I16: return Variant(read_gds_impl<int16_t, Vector3i, int32_t, 3>());
                case U32: return Variant(read_gds_impl<uint32_t, Vector3i, int32_t, 3>());
                case I32: return Variant(read_gds_impl<int32_t, Vector3i, int32_t, 3>());
                case U64: return Variant(read_gds_impl<uint64_t, Vector3i, int32_t, 3>());
                case I64: return Variant(read_gds_impl<int64_t, Vector3i, int32_t, 3>());
                case F16: return Variant(read_gds_impl<HalfU16, Vector3i, int32_t, 3>());
                case F32: return Variant(read_gds_impl<float, Vector3i, int32_t, 3>());
                case F64: return Variant(read_gds_impl<double, Vector3i, int32_t, 3>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Vector3i");
            }
            break;
        }
        case GODOT_TYPE::VEC_4: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Vector4, real_t, 4>());
                case U8: return Variant(read_gds_impl<uint8_t, Vector4, real_t, 4>());
                case I8: return Variant(read_gds_impl<int8_t, Vector4, real_t, 4>());
                case U16: return Variant(read_gds_impl<uint16_t, Vector4, real_t, 4>());
                case I16: return Variant(read_gds_impl<int16_t, Vector4, real_t, 4>());
                case U32: return Variant(read_gds_impl<uint32_t, Vector4, real_t, 4>());
                case I32: return Variant(read_gds_impl<int32_t, Vector4, real_t, 4>());
                case U64: return Variant(read_gds_impl<uint64_t, Vector4, real_t, 4>());
                case I64: return Variant(read_gds_impl<int64_t, Vector4, real_t, 4>());
                case F16: return Variant(read_gds_impl<HalfU16, Vector4, real_t, 4>());
                case F32: return Variant(read_gds_impl<float, Vector4, real_t, 4>());
                case F64: return Variant(read_gds_impl<double, Vector4, real_t, 4>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Vector4");
            }
            break;
        }
        case GODOT_TYPE::VEC_4I: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Vector4i, int32_t, 4>());
                case U8: return Variant(read_gds_impl<uint8_t, Vector4i, int32_t, 4>());
                case I8: return Variant(read_gds_impl<int8_t, Vector4i, int32_t, 4>());
                case U16: return Variant(read_gds_impl<uint16_t, Vector4i, int32_t, 4>());
                case I16: return Variant(read_gds_impl<int16_t, Vector4i, int32_t, 4>());
                case U32: return Variant(read_gds_impl<uint32_t, Vector4i, int32_t, 4>());
                case I32: return Variant(read_gds_impl<int32_t, Vector4i, int32_t, 4>());
                case U64: return Variant(read_gds_impl<uint64_t, Vector4i, int32_t, 4>());
                case I64: return Variant(read_gds_impl<int64_t, Vector4i, int32_t, 4>());
                case F16: return Variant(read_gds_impl<HalfU16, Vector4i, int32_t, 4>());
                case F32: return Variant(read_gds_impl<float, Vector4i, int32_t, 4>());
                case F64: return Variant(read_gds_impl<double, Vector4i, int32_t, 4>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Vector4i");
            }
            break;
        }
        case GODOT_TYPE::COLOR: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Color, float, 4>());
                case U8: return Variant(read_gds_impl<uint8_t, Color, float, 4>());
                case I8: return Variant(read_gds_impl<int8_t, Color, float, 4>());
                case U16: return Variant(read_gds_impl<uint16_t, Color, float, 4>());
                case I16: return Variant(read_gds_impl<int16_t, Color, float, 4>());
                case U32: return Variant(read_gds_impl<uint32_t, Color, float, 4>());
                case I32: return Variant(read_gds_impl<int32_t, Color, float, 4>());
                case U64: return Variant(read_gds_impl<uint64_t, Color, float, 4>());
                case I64: return Variant(read_gds_impl<int64_t, Color, float, 4>());
                case F16: return Variant(read_gds_impl<HalfU16, Color, float, 4>());
                case F32: return Variant(read_gds_impl<float, Color, float, 4>());
                case F64: return Variant(read_gds_impl<double, Color, float, 4>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Color");
            }
            break;
        }
        case GODOT_TYPE::RECT_2: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Rect2, real_t, 4>());
                case U8: return Variant(read_gds_impl<uint8_t, Rect2, real_t, 4>());
                case I8: return Variant(read_gds_impl<int8_t, Rect2, real_t, 4>());
                case U16: return Variant(read_gds_impl<uint16_t, Rect2, real_t, 4>());
                case I16: return Variant(read_gds_impl<int16_t, Rect2, real_t, 4>());
                case U32: return Variant(read_gds_impl<uint32_t, Rect2, real_t, 4>());
                case I32: return Variant(read_gds_impl<int32_t, Rect2, real_t, 4>());
                case U64: return Variant(read_gds_impl<uint64_t, Rect2, real_t, 4>());
                case I64: return Variant(read_gds_impl<int64_t, Rect2, real_t, 4>());
                case F16: return Variant(read_gds_impl<HalfU16, Rect2, real_t, 4>());
                case F32: return Variant(read_gds_impl<float, Rect2, real_t, 4>());
                case F64: return Variant(read_gds_impl<double, Rect2, real_t, 4>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Rect2");
            }
            break;
        }
        case GODOT_TYPE::RECT_2I: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Rect2i, int32_t, 4>());
                case U8: return Variant(read_gds_impl<uint8_t, Rect2i, int32_t, 4>());
                case I8: return Variant(read_gds_impl<int8_t, Rect2i, int32_t, 4>());
                case U16: return Variant(read_gds_impl<uint16_t, Rect2i, int32_t, 4>());
                case I16: return Variant(read_gds_impl<int16_t, Rect2i, int32_t, 4>());
                case U32: return Variant(read_gds_impl<uint32_t, Rect2i, int32_t, 4>());
                case I32: return Variant(read_gds_impl<int32_t, Rect2i, int32_t, 4>());
                case U64: return Variant(read_gds_impl<uint64_t, Rect2i, int32_t, 4>());
                case I64: return Variant(read_gds_impl<int64_t, Rect2i, int32_t, 4>());
                case F16: return Variant(read_gds_impl<HalfU16, Rect2i, int32_t, 4>());
                case F32: return Variant(read_gds_impl<float, Rect2i, int32_t, 4>());
                case F64: return Variant(read_gds_impl<double, Rect2i, int32_t, 4>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Rect2i");
            }
            break;
        }
        case GODOT_TYPE::AABB: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, ::AABB, real_t, 6>());
                case U8: return Variant(read_gds_impl<uint8_t, ::AABB, real_t, 6>());
                case I8: return Variant(read_gds_impl<int8_t, ::AABB, real_t, 6>());
                case U16: return Variant(read_gds_impl<uint16_t, ::AABB, real_t, 6>());
                case I16: return Variant(read_gds_impl<int16_t, ::AABB, real_t, 6>());
                case U32: return Variant(read_gds_impl<uint32_t, ::AABB, real_t, 6>());
                case I32: return Variant(read_gds_impl<int32_t, ::AABB, real_t, 6>());
                case U64: return Variant(read_gds_impl<uint64_t, ::AABB, real_t, 6>());
                case I64: return Variant(read_gds_impl<int64_t, ::AABB, real_t, 6>());
                case F16: return Variant(read_gds_impl<HalfU16, ::AABB, real_t, 6>());
                case F32: return Variant(read_gds_impl<float, ::AABB, real_t, 6>());
                case F64: return Variant(read_gds_impl<double, ::AABB, real_t, 6>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot AABB");
            }
            break;
        }
        case GODOT_TYPE::PLANE: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Plane, real_t, 4>());
                case U8: return Variant(read_gds_impl<uint8_t, Plane, real_t, 4>());
                case I8: return Variant(read_gds_impl<int8_t, Plane, real_t, 4>());
                case U16: return Variant(read_gds_impl<uint16_t, Plane, real_t, 4>());
                case I16: return Variant(read_gds_impl<int16_t, Plane, real_t, 4>());
                case U32: return Variant(read_gds_impl<uint32_t, Plane, real_t, 4>());
                case I32: return Variant(read_gds_impl<int32_t, Plane, real_t, 4>());
                case U64: return Variant(read_gds_impl<uint64_t, Plane, real_t, 4>());
                case I64: return Variant(read_gds_impl<int64_t, Plane, real_t, 4>());
                case F16: return Variant(read_gds_impl<HalfU16, Plane, real_t, 4>());
                case F32: return Variant(read_gds_impl<float, Plane, real_t, 4>());
                case F64: return Variant(read_gds_impl<double, Plane, real_t, 4>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Plane");
            }
            break;
        }
        case GODOT_TYPE::QUATERNION: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Quaternion, real_t, 4>());
                case U8: return Variant(read_gds_impl<uint8_t, Quaternion, real_t, 4>());
                case I8: return Variant(read_gds_impl<int8_t, Quaternion, real_t, 4>());
                case U16: return Variant(read_gds_impl<uint16_t, Quaternion, real_t, 4>());
                case I16: return Variant(read_gds_impl<int16_t, Quaternion, real_t, 4>());
                case U32: return Variant(read_gds_impl<uint32_t, Quaternion, real_t, 4>());
                case I32: return Variant(read_gds_impl<int32_t, Quaternion, real_t, 4>());
                case U64: return Variant(read_gds_impl<uint64_t, Quaternion, real_t, 4>());
                case I64: return Variant(read_gds_impl<int64_t, Quaternion, real_t, 4>());
                case F16: return Variant(read_gds_impl<HalfU16, Quaternion, real_t, 4>());
                case F32: return Variant(read_gds_impl<float, Quaternion, real_t, 4>());
                case F64: return Variant(read_gds_impl<double, Quaternion, real_t, 4>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Quaternion");
            }
            break;
        }
        case GODOT_TYPE::BASIS: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Basis, real_t, 9>());
                case U8: return Variant(read_gds_impl<uint8_t, Basis, real_t, 9>());
                case I8: return Variant(read_gds_impl<int8_t, Basis, real_t, 9>());
                case U16: return Variant(read_gds_impl<uint16_t, Basis, real_t, 9>());
                case I16: return Variant(read_gds_impl<int16_t, Basis, real_t, 9>());
                case U32: return Variant(read_gds_impl<uint32_t, Basis, real_t, 9>());
                case I32: return Variant(read_gds_impl<int32_t, Basis, real_t, 9>());
                case U64: return Variant(read_gds_impl<uint64_t, Basis, real_t, 9>());
                case I64: return Variant(read_gds_impl<int64_t, Basis, real_t, 9>());
                case F16: return Variant(read_gds_impl<HalfU16, Basis, real_t, 9>());
                case F32: return Variant(read_gds_impl<float, Basis, real_t, 9>());
                case F64: return Variant(read_gds_impl<double, Basis, real_t, 9>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Basis");
            }
            break;
        }
        case GODOT_TYPE::TRANSFORM_2D: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Transform2D, real_t, 6>());
                case U8: return Variant(read_gds_impl<uint8_t, Transform2D, real_t, 6>());
                case I8: return Variant(read_gds_impl<int8_t, Transform2D, real_t, 6>());
                case U16: return Variant(read_gds_impl<uint16_t, Transform2D, real_t, 6>());
                case I16: return Variant(read_gds_impl<int16_t, Transform2D, real_t, 6>());
                case U32: return Variant(read_gds_impl<uint32_t, Transform2D, real_t, 6>());
                case I32: return Variant(read_gds_impl<int32_t, Transform2D, real_t, 6>());
                case U64: return Variant(read_gds_impl<uint64_t, Transform2D, real_t, 6>());
                case I64: return Variant(read_gds_impl<int64_t, Transform2D, real_t, 6>());
                case F16: return Variant(read_gds_impl<HalfU16, Transform2D, real_t, 6>());
                case F32: return Variant(read_gds_impl<float, Transform2D, real_t, 6>());
                case F64: return Variant(read_gds_impl<double, Transform2D, real_t, 6>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Transform2D");
            }
            break;
        }
        case GODOT_TYPE::TRANSFORM_3D: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Transform3D, real_t, 12>());
                case U8: return Variant(read_gds_impl<uint8_t, Transform3D, real_t, 12>());
                case I8: return Variant(read_gds_impl<int8_t, Transform3D, real_t, 12>());
                case U16: return Variant(read_gds_impl<uint16_t, Transform3D, real_t, 12>());
                case I16: return Variant(read_gds_impl<int16_t, Transform3D, real_t, 12>());
                case U32: return Variant(read_gds_impl<uint32_t, Transform3D, real_t, 12>());
                case I32: return Variant(read_gds_impl<int32_t, Transform3D, real_t, 12>());
                case U64: return Variant(read_gds_impl<uint64_t, Transform3D, real_t, 12>());
                case I64: return Variant(read_gds_impl<int64_t, Transform3D, real_t, 12>());
                case F16: return Variant(read_gds_impl<HalfU16, Transform3D, real_t, 12>());
                case F32: return Variant(read_gds_impl<float, Transform3D, real_t, 12>());
                case F64: return Variant(read_gds_impl<double, Transform3D, real_t, 12>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Transform3D");
            }
            break;
        }
        case GODOT_TYPE::PROJECTION: {
            switch (serial_type) {
                case BOOL: return Variant(read_gds_impl<bool, Projection, real_t, 16>());
                case U8: return Variant(read_gds_impl<uint8_t, Projection, real_t, 16>());
                case I8: return Variant(read_gds_impl<int8_t, Projection, real_t, 16>());
                case U16: return Variant(read_gds_impl<uint16_t, Projection, real_t, 16>());
                case I16: return Variant(read_gds_impl<int16_t, Projection, real_t, 16>());
                case U32: return Variant(read_gds_impl<uint32_t, Projection, real_t, 16>());
                case I32: return Variant(read_gds_impl<int32_t, Projection, real_t, 16>());
                case U64: return Variant(read_gds_impl<uint64_t, Projection, real_t, 16>());
                case I64: return Variant(read_gds_impl<int64_t, Projection, real_t, 16>());
                case F16: return Variant(read_gds_impl<HalfU16, Projection, real_t, 16>());
                case F32: return Variant(read_gds_impl<float, Projection, real_t, 16>());
                case F64: return Variant(read_gds_impl<double, Projection, real_t, 16>());
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot Projection");
            }
            break;
        }
        case GODOT_TYPE::STRING: {
            switch (serial_type) {
                case U8: {
                    uint32_t len = read_t_val<uint32_t>();
                    LocalVector<char> bytes;
                    bytes.resize(len);
                    read_bytes(bytes.ptr(), len);
                    return Variant(String::utf8(bytes.ptr(), static_cast<int>(len)));
                }
                case U16: {
                    uint32_t len = read_t_val<uint32_t>();
                    LocalVector<char16_t> bytes;
                    bytes.resize(len);
                    read_bytes(bytes.ptr(), len * sizeof(char16_t));
                    return Variant(String::utf16(bytes.ptr(), static_cast<int>(len)));
                }
                case U32: {
                    uint32_t len = read_t_val<uint32_t>();
                    LocalVector<char32_t> bytes;
                    bytes.resize(len);
                    read_bytes(bytes.ptr(), len * sizeof(char32_t));
                    Span<char32_t> span = Span<char32_t>(bytes.ptr(), len);
                    return Variant(String::utf32(span));
                }
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot String");
            }
            break;
        }
        case GODOT_TYPE::ARRAY: {
            if (serial_type != DEFAULT) {
                WARN_PRINT("godot type `ARRAY` ignores specific `serial_type` and always uses the default native type for each element");
            }
            Array arr;
            uint32_t len = read_t_val<uint32_t>();
            arr.resize(len);
            for (uint32_t i = 0; i < len; i += 1) {
                arr[i] = Variant(read_gds(ANY, DEFAULT));
            }
            return Variant(arr);
        }
        case GODOT_TYPE::DICTIONARY: {
            if (serial_type != DEFAULT) {
                WARN_PRINT("godot type `DICTIONARY` ignores specific `serial_type` and always uses the default native type for each element");
            }
            Dictionary dict;
            uint32_t len = read_t_val<uint32_t>();
            for (uint32_t i = 0; i < len; i += 1) {
                Variant key = read_gds(ANY, DEFAULT);
                Variant val = read_gds(ANY, DEFAULT);
                dict.set(key, val);
            }
            return Variant(dict);
        }
        case GODOT_TYPE::PACKED_BYTE_ARRAY: {
            PackedByteArray arr;
            uint32_t len = read_t_val<uint32_t>();
            arr.resize(len);
            switch (serial_type) {
                case BOOL: [[fallthrough]];
                case I8: [[fallthrough]];
                case U8: {
                    read_bytes(arr.ptrw(), len);
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint16_t, uint8_t>());
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int16_t, uint8_t>());
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint32_t, uint8_t>());
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int32_t, uint8_t>());
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint64_t, uint8_t>());
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int64_t, uint8_t>());
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<HalfU16, uint8_t>());
                    }
                    break;
                }
                case F32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<float, uint8_t>());
                    }
                    break;
                }
                case F64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<double, uint8_t>());
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot PackedByteArray");
            }
            return Variant(arr);
        }
        case GODOT_TYPE::PACKED_INT32_ARRAY: {
            PackedInt32Array arr;
            uint32_t len = read_t_val<uint32_t>();
            arr.resize(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<bool, int32_t>());
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int8_t, int32_t>());
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint8_t, int32_t>());
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint16_t, int32_t>());
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int16_t, int32_t>());
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint32_t, int32_t>());
                    }
                    break;
                }
                case I32: {
                    read_t_array(arr.ptrw(), len);
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint64_t, int32_t>());
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int64_t, int32_t>());
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<HalfU16, int32_t>());
                    }
                    break;
                }
                case F32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<float, int32_t>());
                    }
                    break;
                }
                case F64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<double, int32_t>());
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot PackedInt32Array");
            }
            return Variant(arr);
        }
        case GODOT_TYPE::PACKED_INT64_ARRAY: {
            PackedInt64Array arr;
            uint32_t len = read_t_val<uint32_t>();
            arr.resize(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<bool, int64_t>());
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int8_t, int64_t>());
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint8_t, int64_t>());
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint16_t, int64_t>());
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int16_t, int64_t>());
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint32_t, int64_t>());
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int32_t, int64_t>());
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint64_t, int64_t>());
                    }
                    break;
                }
                case I64: {
                    read_t_array(arr.ptrw(), len);
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<HalfU16, int64_t>());
                    }
                    break;
                }
                case F32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<float, int64_t>());
                    }
                    break;
                }
                case F64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<double, int64_t>());
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot PackedInt64Array");
            }
            return Variant(arr);
        }
        case GODOT_TYPE::PACKED_FLOAT32_ARRAY: {
            PackedFloat32Array arr;
            uint32_t len = read_t_val<uint32_t>();
            arr.resize(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<bool, float>());
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int8_t, float>());
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint8_t, float>());
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint16_t, float>());
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int16_t, float>());
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint32_t, float>());
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int32_t, float>());
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint64_t, float>());
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int64_t, float>());
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<HalfU16, float>());
                    }
                    break;
                }
                case F32: {
                    read_t_array(arr.ptrw(), len);
                    break;
                }
                case F64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<double, float>());
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot PackedFloat32Array");
            }
            return Variant(arr);
        }
        case GODOT_TYPE::PACKED_FLOAT64_ARRAY: {
            PackedFloat64Array arr;
            uint32_t len = read_t_val<uint32_t>();
            arr.resize(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<bool, double>());
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int8_t, double>());
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint8_t, double>());
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint16_t, double>());
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int16_t, double>());
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint32_t, double>());
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int32_t, double>());
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<uint64_t, double>());
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<int64_t, double>());
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<HalfU16, double>());
                    }
                    break;
                }
                case F32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_t_cast_val<float, double>());
                    }
                    break;
                }
                case F64: {
                    read_t_array(arr.ptrw(), len);
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot PackedFloat64Array");
            }
            return Variant(arr);
        }
        case GODOT_TYPE::PACKED_VECTOR2_ARRAY: {
            PackedVector2Array arr;
            uint32_t len = read_t_val<uint32_t>();
            arr.resize(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<bool, Vector2, real_t, 2>());
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int8_t, Vector2, real_t, 2>());
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint8_t, Vector2, real_t, 2>());
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint16_t, Vector2, real_t, 2>());
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int16_t, Vector2, real_t, 2>());
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint32_t, Vector2, real_t, 2>());
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int32_t, Vector2, real_t, 2>());
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint64_t, Vector2, real_t, 2>());
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int64_t, Vector2, real_t, 2>());
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<HalfU16, Vector2, real_t, 2>());
                    }
                    break;
                }
                case F32: {
                    if constexpr (sizeof(real_t) == 4) {
                        read_t_array(arr.ptrw(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            arr.set(i, read_gds_impl<float, Vector2, real_t, 2>());
                        }
                    }
                    break;
                }
                case F64: {
                    if constexpr (sizeof(real_t) == 8) {
                        read_t_array(arr.ptrw(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            arr.set(i, read_gds_impl<double, Vector2, real_t, 2>());
                        }
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot PackedVector2Array");
            }
            return Variant(arr);
        }
        case GODOT_TYPE::PACKED_VECTOR3_ARRAY: {
            PackedVector3Array arr;
            uint32_t len = read_t_val<uint32_t>();
            arr.resize(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<bool, Vector3, real_t, 3>());
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int8_t, Vector3, real_t, 3>());
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint8_t, Vector3, real_t, 3>());
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint16_t, Vector3, real_t, 3>());
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int16_t, Vector3, real_t, 3>());
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint32_t, Vector3, real_t, 3>());
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int32_t, Vector3, real_t, 3>());
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint64_t, Vector3, real_t, 3>());
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int64_t, Vector3, real_t, 3>());
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<HalfU16, Vector3, real_t, 3>());
                    }
                    break;
                }
                case F32: {
                    if constexpr (sizeof(real_t) == 4) {
                        read_t_array(arr.ptrw(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            arr.set(i, read_gds_impl<float, Vector3, real_t, 3>());
                        }
                    }
                    break;
                }
                case F64: {
                    if constexpr (sizeof(real_t) == 8) {
                        read_t_array(arr.ptrw(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            arr.set(i, read_gds_impl<double, Vector3, real_t, 3>());
                        }
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot PackedVector3Array");
            }
            return Variant(arr);
        }
        case GODOT_TYPE::PACKED_VECTOR4_ARRAY: {
            PackedVector4Array arr;
            uint32_t len = read_t_val<uint32_t>();
            arr.resize(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<bool, Vector4, real_t, 4>());
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int8_t, Vector4, real_t, 4>());
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint8_t, Vector4, real_t, 4>());
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint16_t, Vector4, real_t, 4>());
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int16_t, Vector4, real_t, 4>());
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint32_t, Vector4, real_t, 4>());
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int32_t, Vector4, real_t, 4>());
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint64_t, Vector4, real_t, 4>());
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int64_t, Vector4, real_t, 4>());
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<HalfU16, Vector4, real_t, 4>());
                    }
                    break;
                }
                case F32: {
                    if constexpr (sizeof(real_t) == 4) {
                        read_t_array(arr.ptrw(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            arr.set(i, read_gds_impl<float, Vector4, real_t, 4>());
                        }
                    }
                    break;
                }
                case F64: {
                    if constexpr (sizeof(real_t) == 8) {
                        read_t_array(arr.ptrw(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            arr.set(i, read_gds_impl<double, Vector4, real_t, 4>());
                        }
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot PackedVector4Array");
            }
            return Variant(arr);
        }
        case GODOT_TYPE::PACKED_COLOR_ARRAY: {
            PackedColorArray arr;
            uint32_t len = read_t_val<uint32_t>();
            arr.resize(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<bool, Color, float, 4>());
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int8_t, Color, float, 4>());
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint8_t, Color, float, 4>());
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint16_t, Color, float, 4>());
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int16_t, Color, float, 4>());
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint32_t, Color, float, 4>());
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int32_t, Color, float, 4>());
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<uint64_t, Color, float, 4>());
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<int64_t, Color, float, 4>());
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<HalfU16, Color, float, 4>());
                    }
                    break;
                }
                case F32: {
                    read_t_array(arr.ptrw(), len);
                    break;
                }
                case F64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        arr.set(i, read_gds_impl<double, Color, float, 4>());
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot PackedColorArray");
            }
            return Variant(arr);
        }
        case GODOT_TYPE::PACKED_STRING_ARRAY: {
            PackedStringArray arr;
            uint32_t len = read_t_val<uint32_t>();
            arr.resize(len);
            switch (serial_type) {
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        uint32_t len2 = read_t_val<uint32_t>();
                        LocalVector<char> bytes;
                        bytes.resize(len2);
                        read_bytes(bytes.ptr(), len2);
                        arr.set(i, String::utf8(bytes.ptr(), static_cast<int>(len2)));
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        uint32_t len2 = read_t_val<uint32_t>();
                        LocalVector<char16_t> bytes;
                        bytes.resize(len2);
                        read_bytes(bytes.ptr(), len2 * sizeof(char16_t));
                        arr.set(i, String::utf16(bytes.ptr(), static_cast<int>(len2)));
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        uint32_t len2 = read_t_val<uint32_t>();
                        LocalVector<char32_t> bytes;
                        bytes.resize(len2);
                        read_bytes(bytes.ptr(), len2 * sizeof(char32_t));
                        Span<char32_t> span = Span<char32_t>(bytes.ptr(), len2);
                        arr.set(i, String::utf32(span));
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "invalid serial type for godot String");
            }
            return Variant(arr);
        }
        default: ERR_FAIL_V_MSG(Variant(), "invalid godot type for serialization");
    }
}
bool ReaderWriter::set_gds(GODOT_TYPE type, Variant val, SERIAL_TYPE serial_type) {
    int64_t initial_pos = get_write_pos();
    bool res = write_gds(type, val, serial_type);
    seek_write_pos(initial_pos, SEEK::FROM_START);
    return res;
}
bool ReaderWriter::write_gds(GODOT_TYPE type, Variant val, SERIAL_TYPE serial_type) {
    ERR_FAIL_COND_V_MSG(type < _GD_TYPE_MIN || type >= _GD_TYPE_LIMIT, false, "invalid godot type for serialization");
    ERR_FAIL_COND_V_MSG(serial_type < _SERIAL_TYPE_MIN || serial_type >= _SERIAL_TYPE_LIMIT, false, "invalid serial type");
    if (type == ANY) {
        uint32_t type_tag = static_cast<uint32_t>(read_t_val<uint8_t>());
        if (type_tag < _GD_TYPE_MIN || type_tag >= ANY) {
            add_error(ERROR::INVALID_TYPE_TAG);
            ERR_FAIL_V_MSG(false, "invalid type tag in serialized data");
        }
        type = (GODOT_TYPE)type_tag;
    }
    if (serial_type == SERIAL_TYPE::DEFAULT) {
        serial_type = (SERIAL_TYPE)GODOT_DEFAULT_ELEM[type - _GD_TYPE_MIN];
    }
    switch (type) {
        case GODOT_TYPE::BOOLEAN: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, bool, bool, 1>((bool)val);
                case U8: return write_gds_impl<uint8_t, bool, bool, 1>((bool)val);
                case I8: return write_gds_impl<int8_t, bool, bool, 1>((bool)val);
                case U16: return write_gds_impl<uint16_t, bool, bool, 1>((bool)val);
                case I16: return write_gds_impl<int16_t, bool, bool, 1>((bool)val);
                case U32: return write_gds_impl<uint32_t, bool, bool, 1>((bool)val);
                case I32: return write_gds_impl<int32_t, bool, bool, 1>((bool)val);
                case U64: return write_gds_impl<uint64_t, bool, bool, 1>((bool)val);
                case I64: return write_gds_impl<int64_t, bool, bool, 1>((bool)val);
                case F16: return write_gds_impl<HalfU16, bool, bool, 1>((bool)val);
                case F32: return write_gds_impl<float, bool, bool, 1>((bool)val);
                case F64: return write_gds_impl<double, bool, bool, 1>((bool)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot boolean");
            }
            break;
        }
        case GODOT_TYPE::INTEGER: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, int64_t, int64_t, 1>((int64_t)val);
                case U8: return write_gds_impl<uint8_t, int64_t, int64_t, 1>((int64_t)val);
                case I8: return write_gds_impl<int8_t, int64_t, int64_t, 1>((int64_t)val);
                case U16: return write_gds_impl<uint16_t, int64_t, int64_t, 1>((int64_t)val);
                case I16: return write_gds_impl<int16_t, int64_t, int64_t, 1>((int64_t)val);
                case U32: return write_gds_impl<uint32_t, int64_t, int64_t, 1>((int64_t)val);
                case I32: return write_gds_impl<int32_t, int64_t, int64_t, 1>((int64_t)val);
                case U64: return write_gds_impl<uint64_t, int64_t, int64_t, 1>((int64_t)val);
                case I64: return write_gds_impl<int64_t, int64_t, int64_t, 1>((int64_t)val);
                case F16: return write_gds_impl<HalfU16, int64_t, int64_t, 1>((int64_t)val);
                case F32: return write_gds_impl<float, int64_t, int64_t, 1>((int64_t)val);
                case F64: return write_gds_impl<double, int64_t, int64_t, 1>((int64_t)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot integer");
            }
            break;
        }
        case GODOT_TYPE::FLOAT: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, double, double, 1>((double)val);
                case U8: return write_gds_impl<uint8_t, double, double, 1>((double)val);
                case I8: return write_gds_impl<int8_t, double, double, 1>((double)val);
                case U16: return write_gds_impl<uint16_t, double, double, 1>((double)val);
                case I16: return write_gds_impl<int16_t, double, double, 1>((double)val);
                case U32: return write_gds_impl<uint32_t, double, double, 1>((double)val);
                case I32: return write_gds_impl<int32_t, double, double, 1>((double)val);
                case U64: return write_gds_impl<uint64_t, double, double, 1>((double)val);
                case I64: return write_gds_impl<int64_t, double, double, 1>((double)val);
                case F16: return write_gds_impl<HalfU16, double, double, 1>((double)val);
                case F32: return write_gds_impl<float, double, double, 1>((double)val);
                case F64: return write_gds_impl<double, double, double, 1>((double)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot float");
            }
            break;
        }
        case GODOT_TYPE::VEC_2: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Vector2, real_t, 2>((Vector2)val);
                case U8: return write_gds_impl<uint8_t, Vector2, real_t, 2>((Vector2)val);
                case I8: return write_gds_impl<int8_t, Vector2, real_t, 2>((Vector2)val);
                case U16: return write_gds_impl<uint16_t, Vector2, real_t, 2>((Vector2)val);
                case I16: return write_gds_impl<int16_t, Vector2, real_t, 2>((Vector2)val);
                case U32: return write_gds_impl<uint32_t, Vector2, real_t, 2>((Vector2)val);
                case I32: return write_gds_impl<int32_t, Vector2, real_t, 2>((Vector2)val);
                case U64: return write_gds_impl<uint64_t, Vector2, real_t, 2>((Vector2)val);
                case I64: return write_gds_impl<int64_t, Vector2, real_t, 2>((Vector2)val);
                case F16: return write_gds_impl<HalfU16, Vector2, real_t, 2>((Vector2)val);
                case F32: return write_gds_impl<float, Vector2, real_t, 2>((Vector2)val);
                case F64: return write_gds_impl<double, Vector2, real_t, 2>((Vector2)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Vector2");
            }
            break;
        }
        case GODOT_TYPE::VEC_2I: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Vector2i, int32_t, 2>((Vector2i)val);
                case U8: return write_gds_impl<uint8_t, Vector2i, int32_t, 2>((Vector2i)val);
                case I8: return write_gds_impl<int8_t, Vector2i, int32_t, 2>((Vector2i)val);
                case U16: return write_gds_impl<uint16_t, Vector2i, int32_t, 2>((Vector2i)val);
                case I16: return write_gds_impl<int16_t, Vector2i, int32_t, 2>((Vector2i)val);
                case U32: return write_gds_impl<uint32_t, Vector2i, int32_t, 2>((Vector2i)val);
                case I32: return write_gds_impl<int32_t, Vector2i, int32_t, 2>((Vector2i)val);
                case U64: return write_gds_impl<uint64_t, Vector2i, int32_t, 2>((Vector2i)val);
                case I64: return write_gds_impl<int64_t, Vector2i, int32_t, 2>((Vector2i)val);
                case F16: return write_gds_impl<HalfU16, Vector2i, int32_t, 2>((Vector2i)val);
                case F32: return write_gds_impl<float, Vector2i, int32_t, 2>((Vector2i)val);
                case F64: return write_gds_impl<double, Vector2i, int32_t, 2>((Vector2i)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Vector2i");
            }
            break;
        }
        case GODOT_TYPE::VEC_3: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Vector3, real_t, 3>((Vector3)val);
                case U8: return write_gds_impl<uint8_t, Vector3, real_t, 3>((Vector3)val);
                case I8: return write_gds_impl<int8_t, Vector3, real_t, 3>((Vector3)val);
                case U16: return write_gds_impl<uint16_t, Vector3, real_t, 3>((Vector3)val);
                case I16: return write_gds_impl<int16_t, Vector3, real_t, 3>((Vector3)val);
                case U32: return write_gds_impl<uint32_t, Vector3, real_t, 3>((Vector3)val);
                case I32: return write_gds_impl<int32_t, Vector3, real_t, 3>((Vector3)val);
                case U64: return write_gds_impl<uint64_t, Vector3, real_t, 3>((Vector3)val);
                case I64: return write_gds_impl<int64_t, Vector3, real_t, 3>((Vector3)val);
                case F16: return write_gds_impl<HalfU16, Vector3, real_t, 3>((Vector3)val);
                case F32: return write_gds_impl<float, Vector3, real_t, 3>((Vector3)val);
                case F64: return write_gds_impl<double, Vector3, real_t, 3>((Vector3)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Vector3");
            }
            break;
        }
        case GODOT_TYPE::VEC_3I: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Vector3i, int32_t, 3>((Vector3i)val);
                case U8: return write_gds_impl<uint8_t, Vector3i, int32_t, 3>((Vector3i)val);
                case I8: return write_gds_impl<int8_t, Vector3i, int32_t, 3>((Vector3i)val);
                case U16: return write_gds_impl<uint16_t, Vector3i, int32_t, 3>((Vector3i)val);
                case I16: return write_gds_impl<int16_t, Vector3i, int32_t, 3>((Vector3i)val);
                case U32: return write_gds_impl<uint32_t, Vector3i, int32_t, 3>((Vector3i)val);
                case I32: return write_gds_impl<int32_t, Vector3i, int32_t, 3>((Vector3i)val);
                case U64: return write_gds_impl<uint64_t, Vector3i, int32_t, 3>((Vector3i)val);
                case I64: return write_gds_impl<int64_t, Vector3i, int32_t, 3>((Vector3i)val);
                case F16: return write_gds_impl<HalfU16, Vector3i, int32_t, 3>((Vector3i)val);
                case F32: return write_gds_impl<float, Vector3i, int32_t, 3>((Vector3i)val);
                case F64: return write_gds_impl<double, Vector3i, int32_t, 3>((Vector3i)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Vector3i");
            }
            break;
        }
        case GODOT_TYPE::VEC_4: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Vector4, real_t, 4>((Vector4)val);
                case U8: return write_gds_impl<uint8_t, Vector4, real_t, 4>((Vector4)val);
                case I8: return write_gds_impl<int8_t, Vector4, real_t, 4>((Vector4)val);
                case U16: return write_gds_impl<uint16_t, Vector4, real_t, 4>((Vector4)val);
                case I16: return write_gds_impl<int16_t, Vector4, real_t, 4>((Vector4)val);
                case U32: return write_gds_impl<uint32_t, Vector4, real_t, 4>((Vector4)val);
                case I32: return write_gds_impl<int32_t, Vector4, real_t, 4>((Vector4)val);
                case U64: return write_gds_impl<uint64_t, Vector4, real_t, 4>((Vector4)val);
                case I64: return write_gds_impl<int64_t, Vector4, real_t, 4>((Vector4)val);
                case F16: return write_gds_impl<HalfU16, Vector4, real_t, 4>((Vector4)val);
                case F32: return write_gds_impl<float, Vector4, real_t, 4>((Vector4)val);
                case F64: return write_gds_impl<double, Vector4, real_t, 4>((Vector4)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Vector4");
            }
            break;
        }
        case GODOT_TYPE::VEC_4I: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Vector4i, int32_t, 4>((Vector4i)val);
                case U8: return write_gds_impl<uint8_t, Vector4i, int32_t, 4>((Vector4i)val);
                case I8: return write_gds_impl<int8_t, Vector4i, int32_t, 4>((Vector4i)val);
                case U16: return write_gds_impl<uint16_t, Vector4i, int32_t, 4>((Vector4i)val);
                case I16: return write_gds_impl<int16_t, Vector4i, int32_t, 4>((Vector4i)val);
                case U32: return write_gds_impl<uint32_t, Vector4i, int32_t, 4>((Vector4i)val);
                case I32: return write_gds_impl<int32_t, Vector4i, int32_t, 4>((Vector4i)val);
                case U64: return write_gds_impl<uint64_t, Vector4i, int32_t, 4>((Vector4i)val);
                case I64: return write_gds_impl<int64_t, Vector4i, int32_t, 4>((Vector4i)val);
                case F16: return write_gds_impl<HalfU16, Vector4i, int32_t, 4>((Vector4i)val);
                case F32: return write_gds_impl<float, Vector4i, int32_t, 4>((Vector4i)val);
                case F64: return write_gds_impl<double, Vector4i, int32_t, 4>((Vector4i)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Vector4i");
            }
            break;
        }
        case GODOT_TYPE::COLOR: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Color, float, 4>((Color)val);
                case U8: return write_gds_impl<uint8_t, Color, float, 4>((Color)val);
                case I8: return write_gds_impl<int8_t, Color, float, 4>((Color)val);
                case U16: return write_gds_impl<uint16_t, Color, float, 4>((Color)val);
                case I16: return write_gds_impl<int16_t, Color, float, 4>((Color)val);
                case U32: return write_gds_impl<uint32_t, Color, float, 4>((Color)val);
                case I32: return write_gds_impl<int32_t, Color, float, 4>((Color)val);
                case U64: return write_gds_impl<uint64_t, Color, float, 4>((Color)val);
                case I64: return write_gds_impl<int64_t, Color, float, 4>((Color)val);
                case F16: return write_gds_impl<HalfU16, Color, float, 4>((Color)val);
                case F32: return write_gds_impl<float, Color, float, 4>((Color)val);
                case F64: return write_gds_impl<double, Color, float, 4>((Color)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Color");
            }
            break;
        }
        case GODOT_TYPE::RECT_2: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Rect2, real_t, 4>((Rect2)val);
                case U8: return write_gds_impl<uint8_t, Rect2, real_t, 4>((Rect2)val);
                case I8: return write_gds_impl<int8_t, Rect2, real_t, 4>((Rect2)val);
                case U16: return write_gds_impl<uint16_t, Rect2, real_t, 4>((Rect2)val);
                case I16: return write_gds_impl<int16_t, Rect2, real_t, 4>((Rect2)val);
                case U32: return write_gds_impl<uint32_t, Rect2, real_t, 4>((Rect2)val);
                case I32: return write_gds_impl<int32_t, Rect2, real_t, 4>((Rect2)val);
                case U64: return write_gds_impl<uint64_t, Rect2, real_t, 4>((Rect2)val);
                case I64: return write_gds_impl<int64_t, Rect2, real_t, 4>((Rect2)val);
                case F16: return write_gds_impl<HalfU16, Rect2, real_t, 4>((Rect2)val);
                case F32: return write_gds_impl<float, Rect2, real_t, 4>((Rect2)val);
                case F64: return write_gds_impl<double, Rect2, real_t, 4>((Rect2)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Rect2");
            }
            break;
        }
        case GODOT_TYPE::RECT_2I: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Rect2i, int32_t, 4>((Rect2i)val);
                case U8: return write_gds_impl<uint8_t, Rect2i, int32_t, 4>((Rect2i)val);
                case I8: return write_gds_impl<int8_t, Rect2i, int32_t, 4>((Rect2i)val);
                case U16: return write_gds_impl<uint16_t, Rect2i, int32_t, 4>((Rect2i)val);
                case I16: return write_gds_impl<int16_t, Rect2i, int32_t, 4>((Rect2i)val);
                case U32: return write_gds_impl<uint32_t, Rect2i, int32_t, 4>((Rect2i)val);
                case I32: return write_gds_impl<int32_t, Rect2i, int32_t, 4>((Rect2i)val);
                case U64: return write_gds_impl<uint64_t, Rect2i, int32_t, 4>((Rect2i)val);
                case I64: return write_gds_impl<int64_t, Rect2i, int32_t, 4>((Rect2i)val);
                case F16: return write_gds_impl<HalfU16, Rect2i, int32_t, 4>((Rect2i)val);
                case F32: return write_gds_impl<float, Rect2i, int32_t, 4>((Rect2i)val);
                case F64: return write_gds_impl<double, Rect2i, int32_t, 4>((Rect2i)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Rect2i");
            }
            break;
        }
        case GODOT_TYPE::AABB: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, ::AABB, real_t, 6>((::AABB)val);
                case U8: return write_gds_impl<uint8_t, ::AABB, real_t, 6>((::AABB)val);
                case I8: return write_gds_impl<int8_t, ::AABB, real_t, 6>((::AABB)val);
                case U16: return write_gds_impl<uint16_t, ::AABB, real_t, 6>((::AABB)val);
                case I16: return write_gds_impl<int16_t, ::AABB, real_t, 6>((::AABB)val);
                case U32: return write_gds_impl<uint32_t, ::AABB, real_t, 6>((::AABB)val);
                case I32: return write_gds_impl<int32_t, ::AABB, real_t, 6>((::AABB)val);
                case U64: return write_gds_impl<uint64_t, ::AABB, real_t, 6>((::AABB)val);
                case I64: return write_gds_impl<int64_t, ::AABB, real_t, 6>((::AABB)val);
                case F16: return write_gds_impl<HalfU16, ::AABB, real_t, 6>((::AABB)val);
                case F32: return write_gds_impl<float, ::AABB, real_t, 6>((::AABB)val);
                case F64: return write_gds_impl<double, ::AABB, real_t, 6>((::AABB)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot AABB");
            }
            break;
        }
        case GODOT_TYPE::PLANE: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Plane, real_t, 4>((Plane)val);
                case U8: return write_gds_impl<uint8_t, Plane, real_t, 4>((Plane)val);
                case I8: return write_gds_impl<int8_t, Plane, real_t, 4>((Plane)val);
                case U16: return write_gds_impl<uint16_t, Plane, real_t, 4>((Plane)val);
                case I16: return write_gds_impl<int16_t, Plane, real_t, 4>((Plane)val);
                case U32: return write_gds_impl<uint32_t, Plane, real_t, 4>((Plane)val);
                case I32: return write_gds_impl<int32_t, Plane, real_t, 4>((Plane)val);
                case U64: return write_gds_impl<uint64_t, Plane, real_t, 4>((Plane)val);
                case I64: return write_gds_impl<int64_t, Plane, real_t, 4>((Plane)val);
                case F16: return write_gds_impl<HalfU16, Plane, real_t, 4>((Plane)val);
                case F32: return write_gds_impl<float, Plane, real_t, 4>((Plane)val);
                case F64: return write_gds_impl<double, Plane, real_t, 4>((Plane)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Plane");
            }
            break;
        }
        case GODOT_TYPE::QUATERNION: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Quaternion, real_t, 4>((Quaternion)val);
                case U8: return write_gds_impl<uint8_t, Quaternion, real_t, 4>((Quaternion)val);
                case I8: return write_gds_impl<int8_t, Quaternion, real_t, 4>((Quaternion)val);
                case U16: return write_gds_impl<uint16_t, Quaternion, real_t, 4>((Quaternion)val);
                case I16: return write_gds_impl<int16_t, Quaternion, real_t, 4>((Quaternion)val);
                case U32: return write_gds_impl<uint32_t, Quaternion, real_t, 4>((Quaternion)val);
                case I32: return write_gds_impl<int32_t, Quaternion, real_t, 4>((Quaternion)val);
                case U64: return write_gds_impl<uint64_t, Quaternion, real_t, 4>((Quaternion)val);
                case I64: return write_gds_impl<int64_t, Quaternion, real_t, 4>((Quaternion)val);
                case F16: return write_gds_impl<HalfU16, Quaternion, real_t, 4>((Quaternion)val);
                case F32: return write_gds_impl<float, Quaternion, real_t, 4>((Quaternion)val);
                case F64: return write_gds_impl<double, Quaternion, real_t, 4>((Quaternion)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Quaternion");
            }
            break;
        }
        case GODOT_TYPE::BASIS: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Basis, real_t, 9>((Basis)val);
                case U8: return write_gds_impl<uint8_t, Basis, real_t, 9>((Basis)val);
                case I8: return write_gds_impl<int8_t, Basis, real_t, 9>((Basis)val);
                case U16: return write_gds_impl<uint16_t, Basis, real_t, 9>((Basis)val);
                case I16: return write_gds_impl<int16_t, Basis, real_t, 9>((Basis)val);
                case U32: return write_gds_impl<uint32_t, Basis, real_t, 9>((Basis)val);
                case I32: return write_gds_impl<int32_t, Basis, real_t, 9>((Basis)val);
                case U64: return write_gds_impl<uint64_t, Basis, real_t, 9>((Basis)val);
                case I64: return write_gds_impl<int64_t, Basis, real_t, 9>((Basis)val);
                case F16: return write_gds_impl<HalfU16, Basis, real_t, 9>((Basis)val);
                case F32: return write_gds_impl<float, Basis, real_t, 9>((Basis)val);
                case F64: return write_gds_impl<double, Basis, real_t, 9>((Basis)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Basis");
            }
            break;
        }
        case GODOT_TYPE::TRANSFORM_2D: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Transform2D, real_t, 6>((Transform2D)val);
                case U8: return write_gds_impl<uint8_t, Transform2D, real_t, 6>((Transform2D)val);
                case I8: return write_gds_impl<int8_t, Transform2D, real_t, 6>((Transform2D)val);
                case U16: return write_gds_impl<uint16_t, Transform2D, real_t, 6>((Transform2D)val);
                case I16: return write_gds_impl<int16_t, Transform2D, real_t, 6>((Transform2D)val);
                case U32: return write_gds_impl<uint32_t, Transform2D, real_t, 6>((Transform2D)val);
                case I32: return write_gds_impl<int32_t, Transform2D, real_t, 6>((Transform2D)val);
                case U64: return write_gds_impl<uint64_t, Transform2D, real_t, 6>((Transform2D)val);
                case I64: return write_gds_impl<int64_t, Transform2D, real_t, 6>((Transform2D)val);
                case F16: return write_gds_impl<HalfU16, Transform2D, real_t, 6>((Transform2D)val);
                case F32: return write_gds_impl<float, Transform2D, real_t, 6>((Transform2D)val);
                case F64: return write_gds_impl<double, Transform2D, real_t, 6>((Transform2D)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Transform2D");
            }
            break;
        }
        case GODOT_TYPE::TRANSFORM_3D: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Transform3D, real_t, 12>((Transform3D)val);
                case U8: return write_gds_impl<uint8_t, Transform3D, real_t, 12>((Transform3D)val);
                case I8: return write_gds_impl<int8_t, Transform3D, real_t, 12>((Transform3D)val);
                case U16: return write_gds_impl<uint16_t, Transform3D, real_t, 12>((Transform3D)val);
                case I16: return write_gds_impl<int16_t, Transform3D, real_t, 12>((Transform3D)val);
                case U32: return write_gds_impl<uint32_t, Transform3D, real_t, 12>((Transform3D)val);
                case I32: return write_gds_impl<int32_t, Transform3D, real_t, 12>((Transform3D)val);
                case U64: return write_gds_impl<uint64_t, Transform3D, real_t, 12>((Transform3D)val);
                case I64: return write_gds_impl<int64_t, Transform3D, real_t, 12>((Transform3D)val);
                case F16: return write_gds_impl<HalfU16, Transform3D, real_t, 12>((Transform3D)val);
                case F32: return write_gds_impl<float, Transform3D, real_t, 12>((Transform3D)val);
                case F64: return write_gds_impl<double, Transform3D, real_t, 12>((Transform3D)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Transform3D");
            }
            break;
        }
        case GODOT_TYPE::PROJECTION: {
            switch (serial_type) {
                case BOOL: return write_gds_impl<bool, Projection, real_t, 16>((Projection)val);
                case U8: return write_gds_impl<uint8_t, Projection, real_t, 16>((Projection)val);
                case I8: return write_gds_impl<int8_t, Projection, real_t, 16>((Projection)val);
                case U16: return write_gds_impl<uint16_t, Projection, real_t, 16>((Projection)val);
                case I16: return write_gds_impl<int16_t, Projection, real_t, 16>((Projection)val);
                case U32: return write_gds_impl<uint32_t, Projection, real_t, 16>((Projection)val);
                case I32: return write_gds_impl<int32_t, Projection, real_t, 16>((Projection)val);
                case U64: return write_gds_impl<uint64_t, Projection, real_t, 16>((Projection)val);
                case I64: return write_gds_impl<int64_t, Projection, real_t, 16>((Projection)val);
                case F16: return write_gds_impl<HalfU16, Projection, real_t, 16>((Projection)val);
                case F32: return write_gds_impl<float, Projection, real_t, 16>((Projection)val);
                case F64: return write_gds_impl<double, Projection, real_t, 16>((Projection)val);
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot Projection");
            }
            break;
        }
        case GODOT_TYPE::STRING: {
            String sval = (String)val;
            switch (serial_type) {
                case U8: {
                    Vector<uint8_t> as_utf8 = sval.to_utf8_buffer();
                    return write_t_array_len_prefix(as_utf8.ptr(), as_utf8.size());
                }
                case U16: {
                    Vector<uint8_t> as_utf16 = sval.to_utf16_buffer();
                    return write_t_array_len_prefix(as_utf16.ptr(), as_utf16.size());
                }
                case U32: return write_t_array_len_prefix(sval.ptr(), sval.size());
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot String");
            }
            break;
        }
        case GODOT_TYPE::ARRAY: {
            if (serial_type != DEFAULT) {
                WARN_PRINT("godot type `ARRAY` ignores specific `serial_type` and always uses the default native type for each element");
            }
            Array arr = (Array)val;
            uint32_t len = arr.size();
            write_t_val(len);
            for (uint32_t i = 0; i < len; i += 1) {
                write_gds(ANY, arr[i], DEFAULT);
            }
            return num_errors == 0;
        }
        case GODOT_TYPE::DICTIONARY: {
            if (serial_type != DEFAULT) {
                WARN_PRINT("godot type `DICTIONARY` ignores specific `serial_type` and always uses the default native type for each element");
            }
            Dictionary dict = (Dictionary)val;
            uint32_t len = dict.size();
            write_t_val(len);
            Array keys = dict.keys();
            Array vals = dict.values();
            for (uint32_t i = 0; i < len; i += 1) {
                write_gds(ANY, keys[i], DEFAULT);
                write_gds(ANY, vals[i], DEFAULT);
            }
            return num_errors == 0;
        }
        case GODOT_TYPE::PACKED_BYTE_ARRAY: {
            PackedByteArray arr = PackedByteArray(val);
            uint32_t len = arr.size();
            write_t_val(len);
            switch (serial_type) {
                case BOOL: [[fallthrough]];
                case I8: [[fallthrough]];
                case U8: {
                    write_bytes(arr.ptr(), len);
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint16_t>(arr[i]);
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int16_t>(arr[i]);
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint32_t>(arr[i]);
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int32_t>(arr[i]);
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint64_t>(arr[i]);
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int64_t>(arr[i]);
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<HalfU16>(arr[i]);
                    }
                    break;
                }
                case F32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<float>(arr[i]);
                    }
                    break;
                }
                case F64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<double>(arr[i]);
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot PackedByteArray");
            }
            return num_errors == 0;
        }
        case GODOT_TYPE::PACKED_INT32_ARRAY: {
            PackedInt32Array arr = PackedInt32Array(val);
            uint32_t len = arr.size();
            write_t_val(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<bool>(arr[i]);
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int8_t>(arr[i]);
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint8_t>(arr[i]);
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint16_t>(arr[i]);
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int16_t>(arr[i]);
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint32_t>(arr[i]);
                    }
                    break;
                }
                case I32: {
                    write_t_array(arr.ptr(), len);
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint64_t>(arr[i]);
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int64_t>(arr[i]);
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<HalfU16>(arr[i]);
                    }
                    break;
                }
                case F32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<float>(arr[i]);
                    }
                    break;
                }
                case F64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<double>(arr[i]);
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot PackedInt32Array");
            }
            return num_errors == 0;
        }
        case GODOT_TYPE::PACKED_INT64_ARRAY: {
            PackedInt64Array arr = PackedInt64Array(val);
            uint32_t len = arr.size();
            write_t_val(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<bool>(arr[i]);
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int8_t>(arr[i]);
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint8_t>(arr[i]);
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint16_t>(arr[i]);
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int16_t>(arr[i]);
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint32_t>(arr[i]);
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int32_t>(arr[i]);
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint64_t>(arr[i]);
                    }
                    break;
                }
                case I64: {
                    write_t_array(arr.ptr(), len);
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<HalfU16>(arr[i]);
                    }
                    break;
                }
                case F32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<float>(arr[i]);
                    }
                    break;
                }
                case F64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<double>(arr[i]);
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot PackedInt64Array");
            }
            return num_errors == 0;
        }
        case GODOT_TYPE::PACKED_FLOAT32_ARRAY: {
            PackedFloat32Array arr = PackedFloat32Array(val);
            uint32_t len = arr.size();
            write_t_val(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<bool>(arr[i]);
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int8_t>(arr[i]);
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint8_t>(arr[i]);
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint16_t>(arr[i]);
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int16_t>(arr[i]);
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint32_t>(arr[i]);
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int32_t>(arr[i]);
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint64_t>(arr[i]);
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int64_t>(arr[i]);
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<HalfU16>(arr[i]);
                    }
                    break;
                }
                case F32: {
                    write_t_array(arr.ptr(), len);
                    break;
                }
                case F64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<double>(arr[i]);
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot PackedFloat32Array");
            }
            return num_errors == 0;
        }
        case GODOT_TYPE::PACKED_FLOAT64_ARRAY: {
            PackedFloat64Array arr = PackedFloat64Array(val);
            uint32_t len = arr.size();
            write_t_val(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<bool>(arr[i]);
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int8_t>(arr[i]);
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint8_t>(arr[i]);
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint16_t>(arr[i]);
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int16_t>(arr[i]);
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint32_t>(arr[i]);
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int32_t>(arr[i]);
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<uint64_t>(arr[i]);
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<int64_t>(arr[i]);
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<HalfU16>(arr[i]);
                    }
                    break;
                }
                case F32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_t_cast_val<float>(arr[i]);
                    }
                    break;
                }
                case F64: {
                    write_t_array(arr.ptr(), len);
                    break;
                }
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot PackedFloat64Array");
            }
            return num_errors == 0;
        }
        case GODOT_TYPE::PACKED_VECTOR2_ARRAY: {
            PackedVector2Array arr = (PackedVector2Array)val;
            uint32_t len = arr.size();
            write_t_val(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<bool, Vector2, real_t, 2>(arr[i]);
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int8_t, Vector2, real_t, 2>(arr[i]);
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint8_t, Vector2, real_t, 2>(arr[i]);
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint16_t, Vector2, real_t, 2>(arr[i]);
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int16_t, Vector2, real_t, 2>(arr[i]);
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint32_t, Vector2, real_t, 2>(arr[i]);
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int32_t, Vector2, real_t, 2>(arr[i]);
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint64_t, Vector2, real_t, 2>(arr[i]);
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int64_t, Vector2, real_t, 2>(arr[i]);
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<HalfU16, Vector2, real_t, 2>(arr[i]);
                    }
                    break;
                }
                case F32: {
                    if constexpr (sizeof(real_t) == 4) {
                        write_t_array(arr.ptr(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            write_gds_impl<float, Vector2, real_t, 2>(arr[i]);
                        }
                    }
                    break;
                }
                case F64: {
                    if constexpr (sizeof(real_t) == 8) {
                        write_t_array(arr.ptr(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            write_gds_impl<double, Vector2, real_t, 2>(arr[i]);
                        }
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot PackedVector2Array");
            }
            return num_errors == 0;
        }
        case GODOT_TYPE::PACKED_VECTOR3_ARRAY: {
            PackedVector3Array arr = (PackedVector3Array)val;
            uint32_t len = arr.size();
            write_t_val(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<bool, Vector3, real_t, 3>(arr[i]);
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int8_t, Vector3, real_t, 3>(arr[i]);
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint8_t, Vector3, real_t, 3>(arr[i]);
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint16_t, Vector3, real_t, 3>(arr[i]);
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int16_t, Vector3, real_t, 3>(arr[i]);
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint32_t, Vector3, real_t, 3>(arr[i]);
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int32_t, Vector3, real_t, 3>(arr[i]);
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint64_t, Vector3, real_t, 3>(arr[i]);
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int64_t, Vector3, real_t, 3>(arr[i]);
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<HalfU16, Vector3, real_t, 3>(arr[i]);
                    }
                    break;
                }
                case F32: {
                    if constexpr (sizeof(real_t) == 4) {
                        write_t_array(arr.ptr(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            write_gds_impl<float, Vector3, real_t, 3>(arr[i]);
                        }
                    }
                    break;
                }
                case F64: {
                    if constexpr (sizeof(real_t) == 8) {
                        write_t_array(arr.ptr(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            write_gds_impl<double, Vector3, real_t, 3>(arr[i]);
                        }
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot PackedVector3Array");
            }
            return num_errors == 0;
        }
        case GODOT_TYPE::PACKED_VECTOR4_ARRAY: {
            PackedVector4Array arr = (PackedVector4Array)val;
            uint32_t len = arr.size();
            write_t_val(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<bool, Vector4, real_t, 4>(arr[i]);
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int8_t, Vector4, real_t, 4>(arr[i]);
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint8_t, Vector4, real_t, 4>(arr[i]);
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint16_t, Vector4, real_t, 4>(arr[i]);
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int16_t, Vector4, real_t, 4>(arr[i]);
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint32_t, Vector4, real_t, 4>(arr[i]);
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int32_t, Vector4, real_t, 4>(arr[i]);
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint64_t, Vector4, real_t, 4>(arr[i]);
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int64_t, Vector4, real_t, 4>(arr[i]);
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<HalfU16, Vector4, real_t, 4>(arr[i]);
                    }
                    break;
                }
                case F32: {
                    if constexpr (sizeof(real_t) == 4) {
                        write_t_array(arr.ptr(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            write_gds_impl<float, Vector4, real_t, 4>(arr[i]);
                        }
                    }
                    break;
                }
                case F64: {
                    if constexpr (sizeof(real_t) == 8) {
                        write_t_array(arr.ptr(), len);
                    } else {
                        for (uint32_t i = 0; i < len; i += 1) {
                            write_gds_impl<double, Vector4, real_t, 4>(arr[i]);
                        }
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot PackedVector4Array");
            }
            return num_errors == 0;
        }
        case GODOT_TYPE::PACKED_COLOR_ARRAY: {
            PackedColorArray arr = (PackedColorArray)val;
            uint32_t len = arr.size();
            write_t_val(len);
            switch (serial_type) {
                case BOOL: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<bool, Color, float, 4>(arr[i]);
                    }
                    break;
                }
                case I8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int8_t, Color, float, 4>(arr[i]);
                    }
                    break;
                }
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint8_t, Color, float, 4>(arr[i]);
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint16_t, Color, float, 4>(arr[i]);
                    }
                    break;
                }
                case I16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int16_t, Color, float, 4>(arr[i]);
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint32_t, Color, float, 4>(arr[i]);
                    }
                    break;
                }
                case I32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int32_t, Color, float, 4>(arr[i]);
                    }
                    break;
                }
                case U64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<uint64_t, Color, float, 4>(arr[i]);
                    }
                    break;
                }
                case I64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<int64_t, Color, float, 4>(arr[i]);
                    }
                    break;
                }
                case F16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<HalfU16, Color, float, 4>(arr[i]);
                    }
                    break;
                }
                case F32: {
                    write_t_array(arr.ptr(), len);
                    break;
                }
                case F64: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        write_gds_impl<double, Color, float, 4>(arr[i]);
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot PackedColorArray");
            }
            return num_errors == 0;
        }
        case GODOT_TYPE::PACKED_STRING_ARRAY: {
            PackedStringArray arr = (PackedStringArray)val;
            uint32_t len = arr.size();
            switch (serial_type) {
                case U8: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        String str = arr[i];
                        Vector<uint8_t> bytes = str.to_utf8_buffer();
                        uint32_t len2 = bytes.size();
                        write_t_val(len2);
                        write_bytes(bytes.ptr(), len2);
                    }
                    break;
                }
                case U16: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        String str = arr[i];
                        Vector<uint8_t> bytes = str.to_utf16_buffer();
                        uint32_t len2 = bytes.size();
                        write_t_val(len2);
                        write_bytes(bytes.ptr(), len2);
                    }
                    break;
                }
                case U32: {
                    for (uint32_t i = 0; i < len; i += 1) {
                        String str = arr[i];
                        uint32_t len2 = str.size() * sizeof(char32_t);
                        write_t_val(len2);
                        write_bytes(str.ptr(), len2);
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(false, "invalid serial type for godot String");
            }
            return num_errors == 0;
        }
        default: ERR_FAIL_V_MSG(false, "invalid godot type for serialization");
    }
}

template<typename T>
bool ReaderWriter::get_t_array(T* val_dst, uint32_t count) {
    get_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    return num_errors == 0;
}

template<typename T>
bool ReaderWriter::read_t_array(T* val_dst, uint32_t count) {
    read_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    return num_errors == 0;
}

template<typename T>
bool ReaderWriter::set_t_array(const T* val_src, uint32_t count) {
    set_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    return num_errors == 0;
}

template<typename T>
bool ReaderWriter::write_t_array(const T* val_src, uint32_t count) {
    write_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    return num_errors > 0;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::get_t_cast_array(T_NATIVE* val_dst, uint32_t count) {
    int64_t init_pos = get_read_pos();
    for (uint32_t i = 0; i < count; i += 1) {
        read_t_cast<T_SERIAL>(val_dst);
        val_dst += 1;
    }
    seek_read_pos(init_pos, SEEK::FROM_START);
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::read_t_cast_array(T_NATIVE* val_dst, uint32_t count) {
    for (uint32_t i = 0; i < count; i += 1) {
        read_t_cast<T_SERIAL>(val_dst);
        val_dst += 1;
    }
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::set_t_cast_array(const T_NATIVE* val_src, uint32_t count) {
    int64_t init_pos = get_write_pos();
    for (uint32_t i = 0; i < count; i += 1) {
        set_t_cast<T_SERIAL>(val_src);
        val_src += 1;
    }
    seek_write_pos(init_pos, SEEK::FROM_START);
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::write_t_cast_array(const T_NATIVE* val_src, uint32_t count) {
    for (uint32_t i = 0; i < count; i += 1) {
        write_t_cast<T_SERIAL>(val_src);
        val_src += 1;
    }
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
T_ARRAY ReaderWriter::get_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
    uint32_t end = arr_offset + count;
    int64_t init_pos = get_read_pos();
    arr.resize(end);
    for (uint32_t i = arr_offset; i < end; i += 1) {
        T_SERIAL val;
        read_t_cast<T_SERIAL>(&val);
        arr[i] = Variant(val);
    }
    seek_read_pos(init_pos, SEEK::FROM_START);
    return arr;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
T_ARRAY ReaderWriter::read_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
    uint32_t end = arr_offset + count;
    arr.resize(end);
    for (uint32_t i = arr_offset; i < end; i += 1) {
        T_SERIAL val;
        read_t_cast<T_SERIAL>(&val);
        arr[i] = Variant(val);
    }
    return arr;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
bool ReaderWriter::set_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
    uint32_t end = arr_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(arr.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`arr_offset` + `count` is greater than the `size()` of the array provided as the data source");
    int64_t init_pos = get_write_pos();
    for (uint32_t i = arr_offset; i < end; i += 1) {
        write_t_cast<T_SERIAL>((T_NATIVE)arr[i]);
    }
    seek_write_pos(init_pos, SEEK::FROM_START);
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
bool ReaderWriter::write_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
    uint32_t end = arr_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(arr.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`arr_offset` + `count` is greater than the `size()` of the array provided as the data source");
    for (uint32_t i = arr_offset; i < end; i += 1) {
        write_t_cast<T_SERIAL>((T_NATIVE)arr[i]);
    }
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
T_ARRAY ReaderWriter::get_gds_array(T_ARRAY dest_array, uint32_t array_offset, uint32_t count) {
    int64_t initial_pos = get_read_pos();
    T_ARRAY arr = read_gds_array<T_SERIAL, T_NATIVE, T_ARRAY, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(dest_array, array_offset, count);
    seek_read_pos(initial_pos, SEEK::FROM_START);
    return arr;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
T_ARRAY ReaderWriter::read_gds_array(T_ARRAY dest_array, uint32_t array_offset, uint32_t count) {
    uint32_t end = array_offset + count;
    dest_array.resize(end);
    for (uint32_t i = array_offset; i < end; i += 1) {
        dest_array[i] = read_gds_impl<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>();
    }
    return dest_array;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
bool ReaderWriter::set_gds_array(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    int64_t initial_pos = get_write_pos();
    write_gds_array<T_SERIAL, T_NATIVE, T_ARRAY, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(src_array, array_offset, count);
    seek_write_pos(initial_pos, SEEK::FROM_START);
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
bool ReaderWriter::write_gds_array(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    uint32_t end = array_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(src_array.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`array_offset` + `count` is greater than the `size()` of the array provided as the data source");
    for (uint32_t i = array_offset; i < end; i += 1) {
        write_gds_impl<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(src_array[i]);
    }
    return num_errors == 0;
}

template<typename T>
bool ReaderWriter::get_t_array_len_prefix(T* val_dst) {
    int64_t init_pos = get_read_pos();
    uint32_t count = read_t_val<uint32_t>();
    get_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    seek_read_pos(init_pos, SEEK::FROM_START);
    return num_errors == 0;
}

template<typename T>
bool ReaderWriter::read_t_array_len_prefix(T* val_dst) {
    uint32_t count = read_t_val<uint32_t>();
    read_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    return num_errors == 0;
}

template<typename T>
bool ReaderWriter::set_t_array_len_prefix(const T* val_src, uint32_t count) {
    int64_t init_pos = get_write_pos();
    write_t_val(count);
    set_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    seek_write_pos(init_pos, SEEK::FROM_START);
    return num_errors == 0;
}

template<typename T>
bool ReaderWriter::write_t_array_len_prefix(const T* val_src, uint32_t count) {
    write_t_val(count);
    write_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::get_t_cast_array_len_prefix(T_NATIVE* val_dst) {
    int64_t init_pos = get_read_pos();
    uint32_t count;
    read_t(&count);
    for (uint32_t i = 0; i < count; i += 1) {
        read_t_cast<T_SERIAL>(val_dst);
        val_dst += 1;
    }
    seek_read_pos(init_pos, SEEK::FROM_START);
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::read_t_cast_array_len_prefix(T_NATIVE* val_dst) {
    uint32_t count;
    read_t(&count);
    for (uint32_t i = 0; i < count; i += 1) {
        read_t_cast<T_SERIAL>(val_dst);
        val_dst += 1;
    }
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::set_t_cast_array_len_prefix(const T_NATIVE* val_src, uint32_t count) {
    int64_t init_pos = get_write_pos();
    write_t(&count);
    for (uint32_t i = 0; i < count; i += 1) {
        set_t_cast<T_SERIAL>(val_src);
        val_src += 1;
    }
    seek_write_pos(init_pos, SEEK::FROM_START);
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::write_t_cast_array_len_prefix(const T_NATIVE* val_src, uint32_t count) {
    write_t(&count);
    for (uint32_t i = 0; i < count; i += 1) {
        write_t_cast<T_SERIAL>(val_src);
        val_src += 1;
    }
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
T_ARRAY ReaderWriter::get_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset) {
    int64_t init_pos = get_read_pos();
    uint32_t count;
    read_t(&count);
    uint32_t end = arr_offset + count;
    arr.resize(end);
    for (uint32_t i = arr_offset; i < end; i += 1) {
        T_SERIAL val;
        read_t_cast<T_SERIAL>(&val);
        arr[i] = Variant(val);
    }
    seek_read_pos(init_pos, SEEK::FROM_START);
    return arr;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
T_ARRAY ReaderWriter::read_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset) {
    uint32_t count;
    read_t(&count);
    uint32_t end = arr_offset + count;
    arr.resize(end);
    for (uint32_t i = arr_offset; i < end; i += 1) {
        T_SERIAL val;
        read_t_cast<T_SERIAL>(&val);
        arr[i] = Variant(val);
    }
    return arr;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
bool ReaderWriter::set_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
    uint32_t end = arr_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(arr.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`arr_offset` + `count` is greater than the `size()` of the array provided as the data source");
    int64_t init_pos = get_write_pos();
    write_t(&count);
    for (uint32_t i = arr_offset; i < end; i += 1) {
        write_t_cast<T_SERIAL>((T_NATIVE)arr[i]);
    }
    seek_write_pos(init_pos, SEEK::FROM_START);
    return num_errors == 0;
}
template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
bool ReaderWriter::write_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
    uint32_t end = arr_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(arr.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`arr_offset` + `count` is greater than the `size()` of the array provided as the data source");
    write_t(&count);
    for (uint32_t i = arr_offset; i < end; i += 1) {
        write_t_cast<T_SERIAL>((T_NATIVE)arr[i]);
    }
    return num_errors == 0;
}


template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
T_ARRAY ReaderWriter::get_gds_array_len_prefix(T_ARRAY dest_array, uint32_t array_offset) {
    int64_t initial_pos = get_read_pos();
    T_ARRAY arr = read_gds_array_len_prefix<T_SERIAL, T_NATIVE, T_ARRAY, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(dest_array, array_offset);
    seek_read_pos(initial_pos, SEEK::FROM_START);
    return arr;
}
template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
T_ARRAY ReaderWriter::read_gds_array_len_prefix(T_ARRAY dest_array, uint32_t array_offset) {
    uint32_t count = read_t_val<uint32_t>();
    uint32_t end = array_offset + count;
    dest_array.resize(end);
    for (uint32_t i = array_offset; i < end; i += 1) {
        dest_array[i] = read_gds_impl<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>();
    }
    return dest_array;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
bool ReaderWriter::set_gds_array_len_prefix(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    int64_t initial_pos = get_write_pos();
    write_gds_array_len_prefix<T_SERIAL, T_NATIVE, T_ARRAY, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(src_array, array_offset, count);
    seek_write_pos(initial_pos, SEEK::FROM_START);
    return num_errors == 0;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
bool ReaderWriter::write_gds_array_len_prefix(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    uint32_t end = array_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(src_array.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`array_offset` + `count` is greater than the `size()` of the array provided as the data source");
    write_t_val(count);
    for (uint32_t i = array_offset; i < end; i += 1) {
        write_gds_impl<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(src_array[i]);
    }
    return num_errors == 0;
}



/*****************
* FileAccess
*****************/

Ref<ReaderWriter_FileAccess> ReaderWriter::from_file_access(Ref<FileAccess> file_access) {
    Ref<ReaderWriter_FileAccess> ref = memnew(ReaderWriter_FileAccess);
    ref->file_access = file_access;
    return ref;
}

int64_t ReaderWriter_FileAccess::get_read_pos() {
    ADD_ERROR_RETURN_ZERO_IF(!file_access->is_open(), ERROR::INVALID_STATE)
    return static_cast<int64_t>(file_access->get_position());
}
int64_t ReaderWriter_FileAccess::get_write_pos() {
    ADD_ERROR_RETURN_ZERO_IF(!file_access->is_open(), ERROR::INVALID_STATE)
    return static_cast<int64_t>(file_access->get_position());
}
bool ReaderWriter_FileAccess::seek_read_pos(int64_t delta, SEEK from) {
    clear_deltas();
    ADD_ERROR_RETURN_FALSE_IF(!file_access->is_open(), ERROR::INVALID_STATE)
    int64_t pos = static_cast<int64_t>(file_access->get_position());
    int64_t new_pos = pos;
    switch (from) {
        case ReaderWriter::SEEK::FROM_START: {
            ADD_ERROR_IF(delta < 0, ERROR::SEEK_BEFORE_DATA_RANGE)
            delta = MAX(delta, (int64_t)0);
            file_access->seek(static_cast<uint64_t>(delta));
            int64_t e = file_access->get_error();
            ADD_ERROR_IF(e > 0, ERROR::SEEK_AFTER_DATA_RANGE)
            break;
        }
        case ReaderWriter::SEEK::FROM_CURRENT: {
            int64_t spos = pos;
            spos += delta;
            ADD_ERROR_IF(delta < 0, ERROR::SEEK_BEFORE_DATA_RANGE)
            delta = MAX(delta, (int64_t)0);
            file_access->seek(static_cast<uint64_t>(spos));
            int64_t e = file_access->get_error();
            ADD_ERROR_IF(e > 0, ERROR::SEEK_AFTER_DATA_RANGE)
            break;
        }
        case ReaderWriter::SEEK::FROM_END: {
            ADD_ERROR_IF(delta > 0, ERROR::SEEK_AFTER_DATA_RANGE)
            delta = MIN(delta, (int64_t)0);
            file_access->seek_end(delta);
            int64_t e = file_access->get_error();
            ADD_ERROR_IF(e > 0, ERROR::SEEK_BEFORE_DATA_RANGE)
            break;
        }
        default: ADD_ERROR_RETURN_FALSE(ERROR::INVALID_ENUM_INPUT)
    }
    new_pos = static_cast<int64_t>(file_access->get_position());
    last_seek_delta = new_pos - pos;
    last_bytes_copied = 0;
    return num_errors_this_op == 0;
}
bool ReaderWriter_FileAccess::seek_write_pos(int64_t delta, SEEK from) {
    return seek_read_pos(delta, from);
}
bool ReaderWriter_FileAccess::read_bytes(void* data_dst, uint32_t num_bytes, bool no_seek, bool no_copy) {
    clear_deltas();
    ADD_ERROR_RETURN_FALSE_IF(!file_access->is_open(), ERROR::INVALID_STATE)
    uint64_t num_read = 0;
    uint64_t init_pos = file_access->get_position();
    if (no_copy) {
        num_read = MIN(file_access->get_length() - init_pos, (uint64_t)num_bytes);
        file_access->seek(init_pos + num_read);
    } else {
        num_read = file_access->get_buffer(reinterpret_cast<uint8_t*>(data_dst), static_cast<uint64_t>(num_bytes));
        last_bytes_copied = num_read;
    }
    ADD_ERROR_IF(num_read < num_bytes || file_access->get_error() > 0, ERROR::OUT_OF_DATA_TO_READ)
    if (no_seek) {
        file_access->seek(init_pos);
        ADD_ERROR_IF(file_access->get_error() > 0, ERROR::SEEK_ERROR)
    } else {
        last_seek_delta = num_read;
    }
    return num_errors_this_op == 0;
}
bool ReaderWriter_FileAccess::write_bytes(const void* data_src, uint32_t num_bytes, bool no_seek, bool no_copy) {
    clear_deltas();
    ADD_ERROR_RETURN_FALSE_IF(!file_access->is_open(), ERROR::INVALID_STATE)
    uint64_t num_written = 0;
    uint64_t init_pos = file_access->get_position();
    if (no_copy) {
        num_written = MIN(file_access->get_length() - init_pos, (uint64_t)num_bytes);
        file_access->seek(init_pos + num_written);
        ADD_ERROR_IF(file_access->get_error() > 0, ERROR::SEEK_AFTER_DATA_RANGE)
    } else {
        num_written = file_access->store_buffer(reinterpret_cast<const uint8_t*>(data_src), static_cast<uint64_t>(num_bytes));
        last_bytes_copied = num_written;
    }
    ADD_ERROR_IF(num_written < num_bytes || file_access->get_error() > 0, ERROR::OUT_OF_SPACE_TO_WRITE)
    if (no_seek) {
        file_access->seek(init_pos);
        ADD_ERROR_IF(file_access->get_error() > 0, ERROR::SEEK_ERROR)
    } else {
        last_seek_delta = num_written;
    }
    return num_errors_this_op == 0;
}

/*****************
* PackedByteArray
*****************/

Ref<ReaderWriter_PackedByteArray> ReaderWriter::from_packed_byte_array(PackedByteArray array) {
    Ref<ReaderWriter_PackedByteArray> ref = memnew(ReaderWriter_PackedByteArray);
    ref->array = array;
    return ref;
}

int64_t ReaderWriter_PackedByteArray::get_read_pos() {
    return rpos;
}
int64_t ReaderWriter_PackedByteArray::get_write_pos() {
    return wpos;
}
bool ReaderWriter_PackedByteArray::seek_read_pos(int64_t delta, SEEK from) {
    clear_deltas();
    int64_t old_pos = rpos;
    int64_t new_pos = old_pos;
    int64_t len = static_cast<int64_t>(array.size());
    switch (from) {
        case ReaderWriter::SEEK::FROM_START: {
            new_pos = delta;
            break;
        }
        case ReaderWriter::SEEK::FROM_CURRENT: {
            new_pos = old_pos + delta;
            break;
        }
        case ReaderWriter::SEEK::FROM_END: {
            new_pos = len + delta;
            break;
        }
        default: ADD_ERROR_RETURN_FALSE(ERROR::INVALID_ENUM_INPUT)
    }
    ADD_ERROR_IF(new_pos < 0, ERROR::SEEK_BEFORE_DATA_RANGE)
    ELSE_ADD_ERROR_IF(new_pos > len, ERROR::SEEK_AFTER_DATA_RANGE)
    new_pos = MAX((int64_t)0, MIN(new_pos, len));
    rpos = new_pos;
    last_seek_delta = new_pos - old_pos;
    return num_errors_this_op == 0;
}
bool ReaderWriter_PackedByteArray::seek_write_pos(int64_t delta, SEEK from) {
    clear_deltas();
    int64_t old_pos = wpos;
    int64_t new_pos = old_pos;
    int64_t len = static_cast<int64_t>(array.size());
    switch (from) {
        case ReaderWriter::SEEK::FROM_START: {
            new_pos = delta;
            break;
        }
        case ReaderWriter::SEEK::FROM_CURRENT: {
            new_pos = old_pos + delta;
            break;
        }
        case ReaderWriter::SEEK::FROM_END: {
            new_pos = len + delta;
            break;
        }
        default: ADD_ERROR_RETURN_FALSE(ERROR::INVALID_ENUM_INPUT)
    }
    ADD_ERROR_IF(new_pos < 0, ERROR::SEEK_BEFORE_DATA_RANGE)
    ELSE_ADD_ERROR_IF(new_pos > len, ERROR::SEEK_AFTER_DATA_RANGE)
    new_pos = MAX((int64_t)0, MIN(new_pos, len));
    wpos = new_pos;
    last_seek_delta = new_pos - old_pos;
    return num_errors_this_op == 0;
}
bool ReaderWriter_PackedByteArray::read_bytes(void* data_dst, uint32_t num_bytes, bool no_seek, bool no_copy) {
    clear_deltas();
    int64_t end = rpos + static_cast<int64_t>(num_bytes);
    int64_t len = static_cast<int64_t>(array.size());
    ADD_ERROR_IF(end > len, ERROR::OUT_OF_DATA_TO_READ)
    end = MIN(end, len);
    uint64_t real_num_bytes = end - rpos;
    if (!no_copy) {
        const uint8_t* data_src = array.ptr() + rpos;
        memcpy(data_dst, data_src, real_num_bytes);
        last_bytes_copied = real_num_bytes;
    }
    if (!no_seek) {
        last_seek_delta = real_num_bytes;
        rpos = end;
    }
    return num_errors_this_op == 0;
}
bool ReaderWriter_PackedByteArray::write_bytes(const void* data_src, uint32_t num_bytes, bool no_seek, bool no_copy) {
    clear_deltas();
    int64_t end = wpos + static_cast<int64_t>(num_bytes);
    if (!no_copy) {
        array.resize(end);
        uint8_t* data_dst = array.ptrw() + wpos;
        memcpy(data_dst, data_src, num_bytes);
        last_bytes_copied = num_bytes;
    }
    if (!no_seek) {
        last_seek_delta = num_bytes;
        wpos = end;
    }
    return true;
}

/*****************
* StreamPeer
*****************/

Ref<ReaderWriter_StreamPeer> ReaderWriter::from_stream_peer(Ref<StreamPeer> stream) {
    Ref<ReaderWriter_StreamPeer> ref = memnew(ReaderWriter_StreamPeer);
    ref->stream = stream;
    return ref;
}

int64_t ReaderWriter_StreamPeer::get_read_pos() {
    ADD_ERROR_RETURN_ZERO_IF(!stream.is_valid(), ERROR::INVALID_STATE)
    return 0;
}
int64_t ReaderWriter_StreamPeer::get_write_pos() {
    ADD_ERROR_RETURN_ZERO_IF(!stream.is_valid(), ERROR::INVALID_STATE)
    return 0;
}
bool ReaderWriter_StreamPeer::seek_read_pos(int64_t delta, SEEK from) {
    clear_deltas();
    ADD_ERROR_RETURN_FALSE(ERROR::CANNOT_SEEK_READ);
}
bool ReaderWriter_StreamPeer::seek_write_pos(int64_t delta, SEEK from) {
    clear_deltas();
    ADD_ERROR_RETURN_FALSE(ERROR::CANNOT_SEEK_WRITE);
}
bool ReaderWriter_StreamPeer::read_bytes(void* data_dst, uint32_t num_bytes, bool no_seek, bool no_copy) {
    clear_deltas();
    ADD_ERROR_RETURN_FALSE_IF(!stream.is_valid(), ERROR::INVALID_STATE)
    int num_read = 0;
    last_bytes_copied = static_cast<int64_t>(num_read);
    if (no_copy) {
        uint8_t discard[32];
        int num_rec;
        int remaining = MIN((int)32, (int)num_bytes);
        int block;
        while (remaining > 0) {
            block = MIN((int)32, (int)remaining);
            stream->get_partial_data(&discard[0], block, num_rec);
            if (num_rec < block) {
                add_error(ERROR::OUT_OF_DATA_TO_READ);
                break;
            }
            remaining -= block;
        }
    } else {
        Error e = stream->get_partial_data(reinterpret_cast<uint8_t*>(data_dst), num_bytes, num_read);
        ADD_ERROR_IF(num_read < (int)num_bytes, ERROR::OUT_OF_DATA_TO_READ)
        ELSE_ADD_ERROR_IF(e > 0, ERROR::READ_ERROR)
        last_bytes_copied = num_bytes;
    }
    return num_errors_this_op == 0;
}
bool ReaderWriter_StreamPeer::write_bytes(const void* data_src, uint32_t num_bytes, bool no_seek, bool no_copy) {
    clear_deltas();
    ADD_ERROR_RETURN_FALSE_IF(!stream.is_valid(), ERROR::INVALID_STATE)
    int num_written = 0;
    if (no_copy) {
        WARN_PRINT("performing a 'write' on a StreamPeer with `no_copy == true` does nothing");
    } else {
        Error e = stream->put_partial_data(reinterpret_cast<const uint8_t*>(data_src), num_bytes, num_written);
        ADD_ERROR_IF(num_written < (int)num_bytes, ERROR::OUT_OF_DATA_TO_READ)
        ELSE_ADD_ERROR_IF(e > 0, ERROR::READ_ERROR)
        last_bytes_copied = static_cast<int64_t>(num_written);
    }
    return num_errors_this_op == 0;
}

/*****************
* RawPtr
*****************/

template<typename T>
Ref<ReaderWriter_PtrWithLimit> ReaderWriter::from_ptr_with_len(T* ptr, uint64_t ptr_len, bool can_realloc) {
    Ref<ReaderWriter_PtrWithLimit> ref = memnew(ReaderWriter_PtrWithLimit);
    ref->ptr = reinterpret_cast<uint8_t*>(ptr);
    ref->limit = ptr_len * sizeof(T);
    ref->can_realloc = can_realloc;
    return ref;
}

int64_t ReaderWriter_PtrWithLimit::get_read_pos() {
    return rpos;
}
int64_t ReaderWriter_PtrWithLimit::get_write_pos() {
    return wpos;
}
bool ReaderWriter_PtrWithLimit::seek_read_pos(int64_t delta, SEEK from) {
    clear_deltas();
    ADD_ERROR_RETURN_FALSE_IF(ptr == nullptr, ERROR::INVALID_STATE)
    int64_t old_pos = rpos;
    int64_t new_pos = old_pos;
    int64_t len = static_cast<int64_t>(limit);
    switch (from) {
        case ReaderWriter::SEEK::FROM_START: {
            new_pos = delta;
            break;
        }
        case ReaderWriter::SEEK::FROM_CURRENT: {
            new_pos = old_pos + delta;
            break;
        }
        case ReaderWriter::SEEK::FROM_END: {
            new_pos = len + delta;
            break;
        }
        default: ADD_ERROR_RETURN_FALSE(ERROR::INVALID_ENUM_INPUT)
    }
    ADD_ERROR_IF(new_pos < 0, ERROR::SEEK_BEFORE_DATA_RANGE)
    ELSE_ADD_ERROR_IF(new_pos > len, ERROR::SEEK_AFTER_DATA_RANGE)
    new_pos = MAX((int64_t)0, MIN(new_pos, len));
    rpos = new_pos;
    last_seek_delta = new_pos - old_pos;
    return num_errors_this_op == 0;
}
bool ReaderWriter_PtrWithLimit::seek_write_pos(int64_t delta, SEEK from) {
    clear_deltas();
    ADD_ERROR_RETURN_FALSE_IF(ptr == nullptr, ERROR::INVALID_STATE)
    int64_t old_pos = wpos;
    int64_t new_pos = old_pos;
    int64_t len = static_cast<int64_t>(limit);
    switch (from) {
        case ReaderWriter::SEEK::FROM_START: {
            new_pos = delta;
            break;
        }
        case ReaderWriter::SEEK::FROM_CURRENT: {
            new_pos = old_pos + delta;
            break;
        }
        case ReaderWriter::SEEK::FROM_END: {
            new_pos = len + delta;
            break;
        }
        default: ADD_ERROR_RETURN_FALSE(ERROR::INVALID_ENUM_INPUT)
    }
    ADD_ERROR_IF(new_pos < 0, ERROR::SEEK_BEFORE_DATA_RANGE)
    ELSE_ADD_ERROR_IF(new_pos > len, ERROR::SEEK_AFTER_DATA_RANGE)
    new_pos = MAX((int64_t)0, MIN(new_pos, len));
    wpos = new_pos;
    last_seek_delta = new_pos - old_pos;
    return num_errors_this_op == 0;
}
bool ReaderWriter_PtrWithLimit::read_bytes(void* data_dst, uint32_t num_bytes, bool no_seek, bool no_copy) {
    clear_deltas();
    ADD_ERROR_RETURN_FALSE_IF(ptr == nullptr, ERROR::INVALID_STATE)
    int64_t end = rpos + static_cast<int64_t>(num_bytes);
    int64_t len = static_cast<int64_t>(limit);
    ADD_ERROR_IF(end > len, ERROR::OUT_OF_DATA_TO_READ)
    end = MIN(end, len);
    uint64_t real_num_bytes = end - rpos;
    if (!no_copy) {
        const uint8_t* data_src = ptr + rpos;
        memcpy(data_dst, data_src, real_num_bytes);
        last_bytes_copied = real_num_bytes;
    }
    if (!no_seek) {
        rpos = end;
        last_seek_delta = real_num_bytes;
    }
    return num_errors_this_op == 0;
}
bool ReaderWriter_PtrWithLimit::write_bytes(const void* data_src, uint32_t num_bytes, bool no_seek, bool no_copy) {
    clear_deltas();
    ADD_ERROR_RETURN_FALSE_IF(ptr == nullptr, ERROR::INVALID_STATE)
    int64_t end = wpos + static_cast<int64_t>(num_bytes);
    int64_t len = static_cast<int64_t>(limit);
    if (!no_copy) {
        if (end > len && can_realloc) {
            ptr = reinterpret_cast<uint8_t*>(memrealloc(ptr, end));
            limit = end;
        } else {
            add_error(ERROR::OUT_OF_SPACE_TO_WRITE);
            end = len;
        }
        num_bytes = end - wpos;
        uint8_t* data_dst = ptr + wpos;
        memcpy(data_dst, data_src, num_bytes);
        last_bytes_copied = num_bytes;
    }
    if (!no_seek) {
        last_seek_delta = num_bytes;
        wpos = end;
    }
    return num_errors_this_op == 0;
}

void Serializer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("serialize", "reader_writer"), &Serializer::serialize); \
    ClassDB::bind_method(D_METHOD("deserialize", "reader_writer"), &Serializer::deserialize); \
}

