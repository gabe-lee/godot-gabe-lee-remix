
#include "serial.h"
#include "core/object/class_db.h"
#include <type_traits>


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

#define bind_gds_single(m_serial, m_native, m_native_name, m_native_elem, m_native_count) \
ClassDB::bind_method(D_METHOD("get_" #m_native_name), &ReaderWriter::get_gds<m_serial, m_native, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("read_" #m_native_name), &ReaderWriter::read_gds<m_serial, m_native, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("set_" #m_native_name, "val"), &ReaderWriter::set_gds<m_serial, m_native, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("write_" #m_native_name, "val"), &ReaderWriter::write_gds<m_serial, m_native, m_native_elem, m_native_count>);

#define bind_gds_single_cast(m_serial, m_serial_name, m_native, m_native_name, m_native_elem, m_native_count) \
ClassDB::bind_method(D_METHOD("get_" #m_native_name "_using_" #m_serial_name), &ReaderWriter::get_gds<m_serial, m_native, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("read_" #m_native_name "_using_" #m_serial_name), &ReaderWriter::read_gds<m_serial, m_native, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("set_" #m_native_name "_using_" #m_serial_name, "val"), &ReaderWriter::set_gds<m_serial, m_native, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("write_" #m_native_name "_using_" #m_serial_name, "val"), &ReaderWriter::write_gds<m_serial, m_native, m_native_elem, m_native_count>);

#define bind_gds_single_float_elems(m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single(m_native_elem, m_native, m_native_name, m_native_elem, m_native_count)\
bind_gds_single_cast(HalfU16, f16, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single_cast(float, f32, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single_cast(double, f64, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single_cast(real_t, f_real, m_native, m_native_name, m_native_elem, m_native_count)

#define bind_gds_single_int_elems(m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single(m_native_elem, m_native, m_native_name, m_native_elem, m_native_count)\
bind_gds_single_cast(uint8_t, u8, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single_cast(int8_t, i8, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single_cast(uint16_t, u16, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single_cast(int16_t, i16, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single_cast(uint32_t, u32, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single_cast(int32_t, i32, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single_cast(uint64_t, u64, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_single_cast(int64_t, i64, m_native, m_native_name, m_native_elem, m_native_count)

#define bind_gds_array(m_serial, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
ClassDB::bind_method(D_METHOD("get_" #m_packed "array_of_" #m_native_name, "dest_array", "array_offset", "count"), &ReaderWriter::get_gds_array<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("read_" #m_packed "array_of_" #m_native_name, "dest_array", "array_offset", "count"), &ReaderWriter::read_gds_array<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("set_" #m_packed "array_of_" #m_native_name, "source_array", "array_offset", "count"), &ReaderWriter::set_gds_array<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("write_" #m_packed "array_of_" #m_native_name, "source_array", "array_offset", "count"), &ReaderWriter::write_gds_array<m_serial, m_native, m_array, m_native_elem, m_native_count>);

#define bind_gds_array_cast(m_serial, m_serial_name, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
ClassDB::bind_method(D_METHOD("get_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "dest_array", "array_offset", "count"), &ReaderWriter::get_gds_array<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("read_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "dest_array", "array_offset", "count"), &ReaderWriter::read_gds_array<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("set_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "source_array", "array_offset", "count"), &ReaderWriter::set_gds_array<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("write_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "source_array", "array_offset", "count"), &ReaderWriter::write_gds_array<m_serial, m_native, m_array, m_native_elem, m_native_count>);

#define bind_gds_array_float_elems(m_native, m_packed, m_array, m_native_name, m_native_elem, m_native_count) \
bind_gds_array(m_native_elem, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count)\
bind_gds_array_cast(HalfU16, f16, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast(float, f32, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast(double, f64, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast(real_t, f_real, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count)

#define bind_gds_array_int_elems(m_native, m_packed, m_array, m_native_name, m_native_elem, m_native_count) \
bind_gds_array(m_native_elem, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count)\
bind_gds_array_cast(uint8_t, u8, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast(int8_t, i8, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast(uint16_t, u16, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast(int16_t, i16, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast(uint32_t, u32, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast(int32_t, i32, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast(uint64_t, u64, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast(int64_t, i64, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count)

#define bind_gds_array_len_pfx(m_serial, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
ClassDB::bind_method(D_METHOD("get_len_prefix_" #m_packed "array_of_" #m_native_name, "dest_array", "array_offset"), &ReaderWriter::get_gds_array_len_prefix<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("read_len_prefix_" #m_packed "array_of_" #m_native_name, "dest_array", "array_offset"), &ReaderWriter::read_gds_array_len_prefix<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("set_len_prefix_" #m_packed "array_of_" #m_native_name, "source_array", "array_offset", "count"), &ReaderWriter::set_gds_array_len_prefix<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("write_len_prefix_" #m_packed "array_of_" #m_native_name, "source_array", "array_offset", "count"), &ReaderWriter::write_gds_array_len_prefix<m_serial, m_native, m_array, m_native_elem, m_native_count>);

#define bind_gds_array_cast_len_pfx(m_serial, m_serial_name, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
ClassDB::bind_method(D_METHOD("get_len_prefix_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "dest_array", "array_offset"), &ReaderWriter::get_gds_array_len_prefix<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("read_len_prefix_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "dest_array", "array_offset"), &ReaderWriter::read_gds_array_len_prefix<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("set_len_prefix_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "source_array", "array_offset", "count"), &ReaderWriter::set_gds_array_len_prefix<m_serial, m_native, m_array, m_native_elem, m_native_count>); \
ClassDB::bind_method(D_METHOD("write_len_prefix_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "source_array", "array_offset", "count"), &ReaderWriter::write_gds_array_len_prefix<m_serial, m_native, m_array, m_native_elem, m_native_count>);

#define bind_gds_array_len_pfx_float_elems(m_native, m_packed, m_array, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_len_pfx(m_native_elem, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count)\
bind_gds_array_cast_len_pfx(HalfU16, f16, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast_len_pfx(float, f32, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast_len_pfx(double, f64, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast_len_pfx(real_t, f_real, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count)

#define bind_gds_array_len_pfx_int_elems(m_native, m_packed, m_array, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_len_pfx(m_native_elem, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count)\
bind_gds_array_cast_len_pfx(uint8_t, u8, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast_len_pfx(int8_t, i8, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast_len_pfx(uint16_t, u16, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast_len_pfx(int16_t, i16, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast_len_pfx(uint32_t, u32, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast_len_pfx(int32_t, i32, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast_len_pfx(uint64_t, u64, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count) \
bind_gds_array_cast_len_pfx(int64_t, i64, m_packed, m_array, m_native, m_native_name, m_native_elem, m_native_count)

void ReaderWriter::add_result(int64_t p_seek_delta, uint32_t p_bytes_read_or_written, uint8_t p_error) {
    if (p_error != ERROR::NONE) {
        if (first_error == ERROR::NONE) {
            first_error = p_error;
        }
        num_errors += 1;
        last_error = p_error;
    }
    last_seek_delta = p_seek_delta;
    last_bytes_read_or_written = p_bytes_read_or_written;
}
void ReaderWriter::add_error(uint8_t p_error) {
    return add_result(0, 0, p_error);
}

void ReaderWriter::_bind_methods() {
    BIND_ENUM_CONSTANT(SEEK::FROM_CURRENT);
    BIND_ENUM_CONSTANT(SEEK::FROM_END);
    BIND_ENUM_CONSTANT(SEEK::FROM_START);
    BIND_ENUM_CONSTANT(ERROR::NONE);
    BIND_ENUM_CONSTANT(ERROR::INVALID_STATE);
    BIND_ENUM_CONSTANT(ERROR::OUT_OF_DATA_TO_READ);
    BIND_ENUM_CONSTANT(ERROR::OUT_OF_SPACE_TO_WRITE);
    BIND_ENUM_CONSTANT(ERROR::SEEK_ERROR);
    BIND_ENUM_CONSTANT(ERROR::SEEK_AFTER_DATA_RANGE);
    BIND_ENUM_CONSTANT(ERROR::SEEK_BEFORE_DATA_RANGE);
    ClassDB::bind_method(D_METHOD("get_read_pos"), &ReaderWriter::get_read_pos);
    ClassDB::bind_method(D_METHOD("get_write_pos"), &ReaderWriter::get_write_pos);
    ClassDB::bind_method(D_METHOD("seek_read_pos", "delta", "from"), &ReaderWriter::seek_read_pos);
    ClassDB::bind_method(D_METHOD("seek_write_pos", "delta", "from"), &ReaderWriter::seek_write_pos);
    bind_gds_single_int_elems(int64_t, integer, int64_t, 1);
    bind_gds_single_float_elems(double, float, double, 1);
    bind_gds_single(bool, bool, bool, bool, 1);
    bind_gds_single_float_elems(Vector2, vec2, real_t, 2);
    bind_gds_single_int_elems(Vector2i, vec2i, int32_t, 2);
    bind_gds_single_float_elems(Vector3, vec3, real_t, 3);
    bind_gds_single_int_elems(Vector3i, vec3i, int32_t, 3);
    bind_gds_single_float_elems(Vector4, vec4, real_t, 4);
    bind_gds_single_int_elems(Vector4i, vec4i, int32_t, 4);
    bind_gds_single_float_elems(Rect2, rect2, real_t, 4);
    bind_gds_single_int_elems(Rect2i, rect2i, int32_t, 4);
    bind_gds_single_float_elems(Color, color, float, 4);
    bind_gds_single_float_elems(Plane, plane, real_t, 4);
    bind_gds_single_float_elems(AABB, aabb, real_t, 6);
    bind_gds_single_float_elems(Transform2D, transform2d, real_t, 6);
    bind_gds_single_float_elems(Transform3D, transform3d, real_t, 12);
    bind_gds_single_float_elems(Projection, projection, real_t, 16);
    bind_gds_single_float_elems(Basis, basis, real_t, 9);

    bind_gds_array_int_elems(int64_t,, Array, integer, int64_t, 1);
    bind_gds_array_float_elems(double,, Array, float, double, 1);
    bind_gds_array(bool,, Array, bool, bool, bool, 1);
    bind_gds_array_float_elems(Vector2,, Array, vec2, real_t, 2);
    bind_gds_array_int_elems(Vector2i,, Array, vec2i, int32_t,2 );
    bind_gds_array_float_elems(Vector3,, Array, vec3, real_t, 3);
    bind_gds_array_int_elems(Vector3i,, Array, vec3i, int32_t, 3);
    bind_gds_array_float_elems(Vector4,, Array, vec4, real_t, 4);
    bind_gds_array_int_elems(Vector4i,, Array, vec4i, int32_t, 4);
    bind_gds_array_float_elems(Rect2,, Array, rect2, real_t, 4);
    bind_gds_array_int_elems(Rect2i,, Array, rect2i, int32_t, 4);
    bind_gds_array_float_elems(Color,, Array, color, float, 4);
    bind_gds_array_float_elems(Plane,, Array, plane, real_t, 4);
    bind_gds_array_float_elems(AABB,, Array, aabb, real_t, 6);
    bind_gds_array_float_elems(Transform2D,, Array, transform2d, real_t, 6);
    bind_gds_array_float_elems(Transform3D,, Array, transform3d, real_t, 12);
    bind_gds_array_float_elems(Projection,, Array, projection, real_t, 16);
    bind_gds_array_float_elems(Basis,, Array, basis, real_t, 9);
    
    // bind_gds_array_int_elems(uint8_t,packed_, PackedByteArray, bytes, uint8_t);
    // bind_gds_array_int_elems(int32_t,packed_, PackedInt32Array, int32, uint8_t);
}

void ReaderWriter::clear_errors() {
    first_error = ERROR::NONE;
    last_error = ERROR::NONE;
    num_errors = 0;
}

bool ReaderWriter::has_errors() {
    return first_error != ERROR::NONE;
}

uint8_t ReaderWriter::get_first_error() {
    return first_error;
}

uint8_t ReaderWriter::get_last_error() {
    return last_error;
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
    bool success = get_bytes(reinterpret_cast<void*>(&val_ser), sizeof(T_SERIAL))
    *val_dst = static_cast<T_NATIVE>(val_ser);
    return success;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::read_t_cast(T_NATIVE* val_dst) {
    T_SERIAL val_ser;
    bool success = read_bytes(reinterpret_cast<void*>(&val_ser), sizeof(T_SERIAL));
    *val_dst = static_cast<T_NATIVE>(val_ser);
    return success;
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
T_NATIVE ReaderWriter::get_gds() {
    int64_t initial_pos = get_read_pos();
    T_NATIVE out = read_gds<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>();
    seek_read_pos(initial_pos, SEEK::FROM_START);
    return out;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
T_NATIVE ReaderWriter::read_gds() {
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
bool ReaderWriter::set_gds(T_NATIVE val) {
    int64_t initial_pos = get_write_pos();
    bool err = write_gds<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(val);
    seek_write_pos(initial_pos, SEEK::FROM_START);
    return err;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
bool ReaderWriter::write_gds(T_NATIVE val) {
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
    return first_error;
}

template<typename T>
bool ReaderWriter::get_t_array(T* val_dst, uint32_t count) {
    get_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    return first_error;
}

template<typename T>
bool ReaderWriter::read_t_array(T* val_dst, uint32_t count) {
    read_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    return first_error;
}

template<typename T>
bool ReaderWriter::set_t_array(const T* val_src, uint32_t count) {
    set_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    return first_error;
}

template<typename T>
bool ReaderWriter::write_t_array(const T* val_src, uint32_t count) {
    write_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::get_t_cast_array(T_NATIVE* val_dst, uint32_t count) {
    int64_t init_pos = get_read_pos();
    for (uint32_t i = 0; i < count; i += 1) {
        read_t_cast<T_SERIAL>(val_dst);
        val_dst += 1;
    }
    seek_read_pos(init_pos, SEEK::FROM_START);
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::read_t_cast_array(T_NATIVE* val_dst, uint32_t count) {
    for (uint32_t i = 0; i < count; i += 1) {
        read_t_cast<T_SERIAL>(val_dst);
        val_dst += 1;
    }
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::set_t_cast_array(const T_NATIVE* val_src, uint32_t count) {
    int64_t init_pos = get_write_pos();
    for (uint32_t i = 0; i < count; i += 1) {
        set_t_cast<T_SERIAL>(val_src);
        val_src += 1;
    }
    seek_write_pos(init_pos, SEEK::FROM_START);
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::write_t_cast_array(const T_NATIVE* val_src, uint32_t count) {
    for (uint32_t i = 0; i < count; i += 1) {
        write_t_cast<T_SERIAL>(val_src);
        val_src += 1;
    }
    return first_error;
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
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
bool ReaderWriter::write_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
    uint32_t end = arr_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(arr.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`arr_offset` + `count` is greater than the `size()` of the array provided as the data source");
    for (uint32_t i = arr_offset; i < end; i += 1) {
        write_t_cast<T_SERIAL>((T_NATIVE)arr[i]);
    }
    return first_error;
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
        dest_array[i] = read_gds<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>();
    }
    return dest_array;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
bool ReaderWriter::set_gds_array(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    int64_t initial_pos = get_write_pos();
    write_gds_array<T_SERIAL, T_NATIVE, T_ARRAY, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(src_array, array_offset, count);
    seek_write_pos(initial_pos, SEEK::FROM_START);
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
bool ReaderWriter::write_gds_array(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    uint32_t end = array_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(src_array.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`array_offset` + `count` is greater than the `size()` of the array provided as the data source");
    for (uint32_t i = array_offset; i < end; i += 1) {
        write_gds<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(src_array[i]);
    }
    return first_error;
}

template<typename T>
bool ReaderWriter::get_t_array_len_prefix(T* val_dst) {
    int64_t init_pos = get_read_pos();
    uint32_t count = read_t_val<uint32_t>();
    get_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    seek_read_pos(init_pos, SEEK::FROM_START);
    return first_error;
}

template<typename T>
bool ReaderWriter::read_t_array_len_prefix(T* val_dst) {
    uint32_t count = read_t_val<uint32_t>();
    read_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    return first_error;
}

template<typename T>
bool ReaderWriter::set_t_array_len_prefix(const T* val_src, uint32_t count) {
    int64_t init_pos = get_write_pos();
    write_t_val(count);
    set_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    seek_write_pos(init_pos, SEEK::FROM_START);
    return first_error;
}

template<typename T>
bool ReaderWriter::write_t_array_len_prefix(const T* val_src, uint32_t count) {
    int64_t init_pos = get_write_pos();
    write_t_val(count);
    set_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    seek_write_pos(init_pos, SEEK::FROM_START);
    return first_error;
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
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::read_t_cast_array_len_prefix(T_NATIVE* val_dst) {
    uint32_t count;
    read_t(&count);
    for (uint32_t i = 0; i < count; i += 1) {
        read_t_cast<T_SERIAL>(val_dst);
        val_dst += 1;
    }
    return first_error;
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
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE>
bool ReaderWriter::write_t_cast_array_len_prefix(const T_NATIVE* val_src, uint32_t count) {
    write_t(&count);
    for (uint32_t i = 0; i < count; i += 1) {
        write_t_cast<T_SERIAL>(val_src);
        val_src += 1;
    }
    return first_error;
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
    return first_error;
}
template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
bool ReaderWriter::write_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
    uint32_t end = arr_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(arr.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`arr_offset` + `count` is greater than the `size()` of the array provided as the data source");
    write_t(&count);
    for (uint32_t i = arr_offset; i < end; i += 1) {
        write_t_cast<T_SERIAL>((T_NATIVE)arr[i]);
    }
    return first_error;
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
        dest_array[i] = read_gds<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>();
    }
    return dest_array;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
bool ReaderWriter::set_gds_array_len_prefix(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    int64_t initial_pos = get_write_pos();
    write_gds_array_len_prefix<T_SERIAL, T_NATIVE, T_ARRAY, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(src_array, array_offset, count);
    seek_write_pos(initial_pos, SEEK::FROM_START);
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
bool ReaderWriter::write_gds_array_len_prefix(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    uint32_t end = array_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(src_array.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`array_offset` + `count` is greater than the `size()` of the array provided as the data source");
    write_t_val(count);
    for (uint32_t i = array_offset; i < end; i += 1) {
        write_gds<T_SERIAL, T_NATIVE, T_NATIVE_ELEM, T_NATIVE_ELEM_COUNT>(src_array[i]);
    }
    return first_error;
}

// /*****************
//  *  FileAccess
//  *****************/
// bool file_access_position(Ref<RefCounted> p_object) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
//     if (fa == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     if (!fa->is_open()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     int64_t pos = static_cast<int64_t>(fa->get_position());
//     return bool{pos, 0, ReaderWriter::ERROR::NONE};
// }
// bool file_access_seek(Ref<RefCounted> p_object, int64_t delta, ReaderWriter::SEEK from) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
//     if (fa == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     if (!fa->is_open()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     int64_t pos = static_cast<int64_t>(fa->get_position());
//     int64_t new_pos = pos;
//     switch (from) {
//         case ReaderWriter::SEEK::FROM_START: {
//             ERR_FAIL_COND_V_MSG(delta < 0, bool::error(ReaderWriter::ERROR::SEEK_BEFORE_DATA_RANGE), "cannot seek to before beginning of data");
//             fa->seek(static_cast<uint64_t>(delta));
//             int64_t e = fa->get_error();
//             ERR_FAIL_COND_V_MSG(e > 0, bool::error(ReaderWriter::ERROR::SEEK_AFTER_DATA_RANGE), "cannot seek to after end of data");
//             break;
//         }
//         case ReaderWriter::SEEK::FROM_CURRENT: {
//             int64_t spos = pos;
//             spos += delta;
//             ERR_FAIL_COND_V_MSG(spos < 0, bool::error(ReaderWriter::ERROR::SEEK_BEFORE_DATA_RANGE), "cannot seek to before beginning of data");
//             fa->seek(static_cast<uint64_t>(spos));
//             int64_t e = fa->get_error();
//             ERR_FAIL_COND_V_MSG(e > 0, bool::error(ReaderWriter::ERROR::SEEK_AFTER_DATA_RANGE), "cannot seek to after end of data");
//             break;
//         }
//         case ReaderWriter::SEEK::FROM_END: {
//             ERR_FAIL_COND_V_MSG(delta > 0, bool::error(ReaderWriter::ERROR::SEEK_AFTER_DATA_RANGE), "cannot seek to after end of data");
//             fa->seek_end(delta);
//             int64_t e = fa->get_error();
//             ERR_FAIL_COND_V_MSG(e > 0, bool::error(ReaderWriter::ERROR::SEEK_BEFORE_DATA_RANGE), "cannot seek to before beginning of data");
//             break;
//         }
//     }
//     new_pos = static_cast<int64_t>(fa->get_position());
//     return bool{new_pos - pos, 0, ReaderWriter::ERROR::NONE};
// }
// bool file_access_read(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
//     if (fa == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     if (!fa->is_open()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     int64_t num_read = fa->get_buffer(reinterpret_cast<uint8_t*>(data_dst), static_cast<uint64_t>(num_bytes));
//     uint8_t e = ReaderWriter::ERROR::NONE;
//     if (num_read < num_bytes) {
//         e = ReaderWriter::ERROR::OUT_OF_DATA_TO_READ;
//     }
//     return bool{num_read, static_cast<uint32_t>(num_read), e};
// }
// bool file_access_get(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
//     if (fa == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     if (!fa->is_open()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     int64_t initial_pos = static_cast<int64_t>(fa->get_position());
//     uint64_t num_read = fa->get_buffer(reinterpret_cast<uint8_t*>(data_dst), static_cast<uint64_t>(num_bytes));
//     uint8_t e = ReaderWriter::ERROR::NONE;
//     if (num_read < num_bytes) {
//         e = ReaderWriter::ERROR::OUT_OF_DATA_TO_READ;
//     }
//     fa->seek(initial_pos);
//     if (e == 0) {
//         if (fa->get_error()) {
//             e = ReaderWriter::ERROR::SEEK_ERROR;
//         }
//     }
//     return bool{0, static_cast<uint32_t>(num_read), e};
// }
// bool file_access_write(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
//     if (fa == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     if (!fa->is_open()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     uint64_t num_written = fa->store_buffer(reinterpret_cast<const uint8_t*>(data_src), static_cast<uint64_t>(num_bytes));
//     uint8_t e = ReaderWriter::ERROR::NONE;
//     if (num_written < num_bytes) {
//         e = ReaderWriter::ERROR::OUT_OF_SPACE_TO_WRITE;
//     }
//     return bool{static_cast<int64_t>(num_written), static_cast<uint32_t>(num_written), e};
// }
// bool file_access_set(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
//     if (fa == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     if (!fa->is_open()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     int64_t initial_pos = static_cast<int64_t>(fa->get_position());
//     uint64_t num_written = fa->store_buffer(reinterpret_cast<const uint8_t*>(data_src), static_cast<uint64_t>(num_bytes));
//     uint8_t e = ReaderWriter::ERROR::NONE;
//     if (num_written < num_bytes) {
//         e = ReaderWriter::ERROR::OUT_OF_SPACE_TO_WRITE;
//     }
//     fa->seek(initial_pos);
//     if (e == 0) {
//         if (fa->get_error()) {
//             e = ReaderWriter::ERROR::SEEK_ERROR;
//         }
//     }
//     return bool{0, static_cast<uint32_t>(num_written), e};
// }
// /*****************
//  *  PackedByteArray
//  *****************/
// bool packed_bytes_read_pos(Ref<RefCounted> p_object) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
//     if (wrapper == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     return bool{wrapper->rpos, ReaderWriter::ERROR::NONE};
// }
// bool packed_bytes_write_pos(Ref<RefCounted> p_object) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
//     if (wrapper == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     return bool{wrapper->wpos, ReaderWriter::ERROR::NONE};
// }
// bool packed_bytes_seek_read(Ref<RefCounted> p_object, int64_t delta, ReaderWriter::SEEK from) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
//     if (wrapper == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     int64_t old_pos = wrapper->rpos;
//     int64_t new_pos = old_pos;
//     switch (from) {
//         case ReaderWriter::SEEK::FROM_START: {
//             new_pos = delta;
//             break;
//         }
//         case ReaderWriter::SEEK::FROM_CURRENT: {
//             new_pos = old_pos + delta;
//             break;
//         }
//         case ReaderWriter::SEEK::FROM_END: {
//             new_pos = static_cast<int64_t>(wrapper->arr.size()) + delta;
//             break;
//         }
//     }
//     uint8_t e = ReaderWriter::ERROR::NONE;
//     if (new_pos < 0) {e = ReaderWriter::ERROR::SEEK_BEFORE_DATA_RANGE;}
//     else if (new_pos > wrapper->arr.size()) {e = ReaderWriter::ERROR::SEEK_AFTER_DATA_RANGE;}
//     new_pos = MAX((int64_t)0, MIN(new_pos, MIN(wrapper->wpos, wrapper->arr.size())));
//     wrapper->rpos = new_pos;
//     int64_t true_delta = new_pos - old_pos;
//     return bool{true_delta, e};
// }
// bool packed_bytes_seek_write(Ref<RefCounted> p_object, int64_t delta, ReaderWriter::SEEK from) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
//     if (wrapper == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     int64_t old_pos = wrapper->wpos;
//     int64_t new_pos = old_pos;
//     switch (from) {
//         case ReaderWriter::SEEK::FROM_START: {
//             new_pos = delta;
//             break;
//         }
//         case ReaderWriter::SEEK::FROM_CURRENT: {
//             new_pos = old_pos + delta;
//             break;
//         }
//         case ReaderWriter::SEEK::FROM_END: {
//             new_pos = static_cast<int64_t>(wrapper->arr.size()) + delta;
//             break;
//         }
//     }
//     uint8_t e = ReaderWriter::ERROR::NONE;
//     if (new_pos < 0) {e = ReaderWriter::ERROR::SEEK_BEFORE_DATA_RANGE;}
//     else if (new_pos > wrapper->arr.size()) {e = ReaderWriter::ERROR::SEEK_AFTER_DATA_RANGE;}
//     new_pos = MAX(MAX((int64_t)0, wrapper->rpos), MIN(new_pos,  wrapper->arr.size()));
//     wrapper->wpos = new_pos;
//     int64_t true_delta = new_pos - old_pos;
//     return bool{true_delta, e};
// }
// bool packed_bytes_read(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
//     if (wrapper == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     int64_t end = wrapper->rpos + static_cast<int64_t>(num_bytes);
//     if (end > static_cast<int64_t>(wrapper->arr.size()))  {return bool::error(ReaderWriter::ERROR::OUT_OF_DATA_TO_READ);}
//     const uint8_t* data_src = wrapper->arr.ptr();
//     memcpy(data_dst, data_src, num_bytes);
//     wrapper->rpos = end;
//     return bool{num_bytes, num_bytes, ReaderWriter::ERROR::NONE};
// }
// bool packed_bytes_get(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
//     if (wrapper == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     int64_t end = wrapper->rpos + static_cast<int64_t>(num_bytes);
//     if (end > static_cast<int64_t>(wrapper->arr.size()))  {return bool::error(ReaderWriter::ERROR::OUT_OF_DATA_TO_READ);}
//     const uint8_t* data_src = wrapper->arr.ptr();
//     memcpy(data_dst, data_src, num_bytes);
//     return bool{num_bytes, ReaderWriter::ERROR::NONE};
// }
// bool packed_bytes_write(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
//     if (wrapper == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     int64_t end = wrapper->wpos + static_cast<int64_t>(num_bytes);
//     if (end > static_cast<int64_t>(wrapper->arr.size()))  {
//         if (wrapper->arr.resize(end)) {
//             {return bool::error(ReaderWriter::ERROR::OUT_OF_SPACE_TO_WRITE);}
//         }
//     }
//     uint8_t* data_dst = wrapper->arr.ptrw();
//     memcpy(data_dst, data_src, num_bytes);
//     wrapper->wpos = end;
//     return bool{num_bytes, num_bytes, ReaderWriter::ERROR::NONE};
// }
// bool packed_bytes_set(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
//     if (p_object.is_null()) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
//     if (wrapper == nullptr) {return bool::error(ReaderWriter::ERROR::INVALID_STATE);}
//     int64_t end = wrapper->wpos + static_cast<int64_t>(num_bytes);
//     if (end > static_cast<int64_t>(wrapper->arr.size())) {
//         if (wrapper->arr.resize(end)) {
//             {return bool::error(ReaderWriter::ERROR::OUT_OF_SPACE_TO_WRITE);}
//         }
//     }
//     uint8_t* data_dst = wrapper->arr.ptrw();
//     memcpy(data_dst, data_src, num_bytes);
//     return bool{num_bytes, ReaderWriter::ERROR::NONE};
// }

void Serializer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("serialize", "reader_writer"), &Serializer::serialize); \
    ClassDB::bind_method(D_METHOD("deserialize", "reader_writer"), &Serializer::deserialize); \
}

