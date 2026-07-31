
#include "reader_writer.h"
#include "core/error/error_macros.h"
#include "core/math/math_defs.h"
#include "core/math/rect2i.h"
#include "core/math/transform_2d.h"
#include "core/math/vector2.h"
#include <cstdint>
#include <tuple>
#include "types.h"
#include "core/object/class_db.h"


#define FIND_S(m_s) constexpr SerialType::SERIAL_TYPE S = std::is_same_v<m_s, bool> ? SerialType::BOOL : std::is_same_v<m_s, uint8_t> ? SerialType::U8 : std::is_same_v<m_s, int8_t> ? SerialType::I8 : std::is_same_v<m_s, uint16_t> ? SerialType::U16 : std::is_same_v<m_s, uint16_t> ? SerialType::I16 : std::is_same_v<m_s, uint32_t> ? SerialType::U32 : std::is_same_v<m_s, int32_t> ? SerialType::I32 : std::is_same_v<m_s, uint64_t> ? SerialType::U64 : std::is_same_v<m_s, int64_t> ? SerialType::I64 : std::is_same_v<m_s, float> ? SerialType::F32 : std::is_same_v<m_s, double> ? SerialType::F64 : std::is_same_v<m_s, HalfU16> ? SerialType::F16 : SerialType::_S_INVALID;    
#define FIND_N(m_n) constexpr SerialType::NATIVE_TYPE N = std::is_same_v<m_n, bool> ? SerialType::BOOLEAN : std::is_same_v<m_n, int64_t> ? SerialType::INTEGER : std::is_same_v<m_n, double> ? SerialType::FLOAT : std::is_same_v<m_n, Vector2> ? SerialType::VEC2 : std::is_same_v<m_n, Vector2i> ? SerialType::VEC2I : std::is_same_v<m_n, Vector3> ? SerialType::VEC3 : std::is_same_v<m_n, Vector3i> ? SerialType::VEC3I : std::is_same_v<m_n, Vector4> ? SerialType::VEC4 : std::is_same_v<m_n, Vector4i> ? SerialType::VEC4I : std::is_same_v<m_n, Rect2> ? SerialType::RECT2 : std::is_same_v<m_n, Rect2i> ? SerialType::RECT2I : std::is_same_v<m_n, Color> ? SerialType::COLOR : std::is_same_v<m_n, Basis> ? SerialType::BASIS : std::is_same_v<m_n, Transform2D> ? SerialType::TRANSFORM2D : std::is_same_v<m_n, Transform3D> ? SerialType::TRANSFORM3D : std::is_same_v<m_n, Quaternion> ? SerialType::QUATERNION : std::is_same_v<m_n, Plane> ? SerialType::PLANE : std::is_same_v<m_n, ::AABB> ? SerialType::RECT3 : std::is_same_v<m_n, Projection> ? SerialType::PROJECTION : SerialType::_N_INVALID;    

#define bind_gds_single(m_serial, m_native, m_native_name) \
ClassDB::bind_method(D_METHOD("get_" #m_native_name), &ReaderWriter::get_gds<m_serial, m_native>); \
ClassDB::bind_method(D_METHOD("read_" #m_native_name), &ReaderWriter::read_gds<m_serial, m_native>); \
ClassDB::bind_method(D_METHOD("set_" #m_native_name, "val"), &ReaderWriter::set_gds<m_serial, m_native>); \
ClassDB::bind_method(D_METHOD("write_" #m_native_name, "val"), &ReaderWriter::write_gds<m_serial, m_native>);

#define bind_gds_single_cast(m_serial, m_serial_name, m_native, m_native_name) \
ClassDB::bind_method(D_METHOD("get_" #m_native_name "_using_" #m_serial_name), &ReaderWriter::get_gds<m_serial, m_native>); \
ClassDB::bind_method(D_METHOD("read_" #m_native_name "_using_" #m_serial_name), &ReaderWriter::read_gds<m_serial, m_native>); \
ClassDB::bind_method(D_METHOD("set_" #m_native_name "_using_" #m_serial_name, "val"), &ReaderWriter::set_gds<m_serial, m_native>); \
ClassDB::bind_method(D_METHOD("write_" #m_native_name "_using_" #m_serial_name, "val"), &ReaderWriter::write_gds<m_serial, m_native>);

#define bind_gds_single_float_elems(m_native, m_native_name, m_native_default_elem) \
bind_gds_single(m_native_default_elem, m_native, m_native_name)\
bind_gds_single_cast(HalfU16, f16, m_native, m_native_name) \
bind_gds_single_cast(float, f32, m_native, m_native_name) \
bind_gds_single_cast(double, f64, m_native, m_native_name) \
bind_gds_single_cast(real_t, f_real, m_native, m_native_name)

#define bind_gds_single_int_elems(m_native, m_native_name, m_native_default_elem) \
bind_gds_single(m_native_default_elem, m_native, m_native_name)\
bind_gds_single_cast(uint8_t, u8, m_native, m_native_name) \
bind_gds_single_cast(int8_t, i8, m_native, m_native_name) \
bind_gds_single_cast(uint16_t, u16, m_native, m_native_name) \
bind_gds_single_cast(int16_t, i16, m_native, m_native_name) \
bind_gds_single_cast(uint32_t, u32, m_native, m_native_name) \
bind_gds_single_cast(int32_t, i32, m_native, m_native_name) \
bind_gds_single_cast(uint64_t, u64, m_native, m_native_name) \
bind_gds_single_cast(int64_t, i64, m_native, m_native_name)

#define bind_gds_array(m_serial, m_packed, m_array, m_native, m_native_name) \
ClassDB::bind_method(D_METHOD("get_" #m_packed "array_of_" #m_native_name, "dest_array", "array_offset", "count"), &ReaderWriter::get_gds_array<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("read_" #m_packed "array_of_" #m_native_name, "dest_array", "array_offset", "count"), &ReaderWriter::read_gds_array<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("set_" #m_packed "array_of_" #m_native_name, "source_array", "array_offset", "count"), &ReaderWriter::set_gds_array<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("write_" #m_packed "array_of_" #m_native_name, "source_array", "array_offset", "count"), &ReaderWriter::write_gds_array<m_serial, m_native, m_array>);

#define bind_gds_array_cast(m_serial, m_serial_name, m_packed, m_array, m_native, m_native_name) \
ClassDB::bind_method(D_METHOD("get_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "dest_array", "array_offset", "count"), &ReaderWriter::get_gds_array<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("read_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "dest_array", "array_offset", "count"), &ReaderWriter::read_gds_array<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("set_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "source_array", "array_offset", "count"), &ReaderWriter::set_gds_array<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("write_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "source_array", "array_offset", "count"), &ReaderWriter::write_gds_array<m_serial, m_native, m_array>);

#define bind_gds_array_float_elems(m_native, m_packed, m_array, m_native_name, m_native_default_elem) \
bind_gds_array(m_native_default_elem, m_packed, m_array, m_native, m_native_name)\
bind_gds_array_cast(HalfU16, f16, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast(float, f32, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast(double, f64, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast(real_t, f_real, m_packed, m_array, m_native, m_native_name)

#define bind_gds_array_int_elems(m_native, m_packed, m_array, m_native_name, m_native_default_elem) \
bind_gds_array(m_native_default_elem, m_packed, m_array, m_native, m_native_name)\
bind_gds_array_cast(uint8_t, u8, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast(int8_t, i8, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast(uint16_t, u16, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast(int16_t, i16, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast(uint32_t, u32, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast(int32_t, i32, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast(uint64_t, u64, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast(int64_t, i64, m_packed, m_array, m_native, m_native_name)

#define bind_gds_array_len_pfx(m_serial, m_packed, m_array, m_native, m_native_name) \
ClassDB::bind_method(D_METHOD("get_len_prefix_" #m_packed "array_of_" #m_native_name, "dest_array", "array_offset"), &ReaderWriter::get_gds_array_len_prefix<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("read_len_prefix_" #m_packed "array_of_" #m_native_name, "dest_array", "array_offset"), &ReaderWriter::read_gds_array_len_prefix<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("set_len_prefix_" #m_packed "array_of_" #m_native_name, "source_array", "array_offset", "count"), &ReaderWriter::set_gds_array_len_prefix<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("write_len_prefix_" #m_packed "array_of_" #m_native_name, "source_array", "array_offset", "count"), &ReaderWriter::write_gds_array_len_prefix<m_serial, m_native, m_array>);

#define bind_gds_array_cast_len_pfx(m_serial, m_serial_name, m_packed, m_array, m_native, m_native_name) \
ClassDB::bind_method(D_METHOD("get_len_prefix_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "dest_array", "array_offset"), &ReaderWriter::get_gds_array_len_prefix<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("read_len_prefix_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "dest_array", "array_offset"), &ReaderWriter::read_gds_array_len_prefix<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("set_len_prefix_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "source_array", "array_offset", "count"), &ReaderWriter::set_gds_array_len_prefix<m_serial, m_native, m_array>); \
ClassDB::bind_method(D_METHOD("write_len_prefix_" #m_packed "array_of_" #m_native_name "_using_" #m_serial_name, "source_array", "array_offset", "count"), &ReaderWriter::write_gds_array_len_prefix<m_serial, m_native, m_array>);

#define bind_gds_array_len_pfx_float_elems(m_native, m_packed, m_array, m_native_name, m_native_default_elem) \
bind_gds_array_len_pfx(m_native_default_elem,m_packed, m_array, m_native, m_native_name)\
bind_gds_array_cast_len_pfx(HalfU16, f16, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast_len_pfx(float, f32, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast_len_pfx(double, f64, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast_len_pfx(real_t, f_real, m_packed, m_array, m_native, m_native_name)

#define bind_gds_array_len_pfx_int_elems(m_native, m_packed, m_array, m_native_name, m_native_default_elem) \
bind_gds_array_len_pfx(m_native_default_elem,m_packed, m_array, m_native, m_native_name)\
bind_gds_array_cast_len_pfx(uint8_t, u8, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast_len_pfx(int8_t, i8, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast_len_pfx(uint16_t, u16, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast_len_pfx(int16_t, i16, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast_len_pfx(uint32_t, u32, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast_len_pfx(int32_t, i32, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast_len_pfx(uint64_t, u64, m_packed, m_array, m_native, m_native_name) \
bind_gds_array_cast_len_pfx(int64_t, i64, m_packed, m_array, m_native, m_native_name)

template<typename T>
T ReaderWriter::collect_results(T res) {
    if (res.err != ERROR::NONE) {
        if (first_error == ERROR::NONE) {
            first_error = res.err;
        }
        num_errors += 1;
        last_error = res.err;
    }
    if constexpr (std::is_same_v<T, SeekResult>) {
        last_seek_delta = res.real_seek_delta;
        last_bytes_read_or_written = 0;
    } else if constexpr (std::is_same_v<T, GetSetResult>) {
        last_seek_delta = 0;
        last_bytes_read_or_written = res.real_bytes_get_or_set;
    } else if constexpr (std::is_same_v<T, ReadWriteResult>) {
        last_seek_delta = res.real_seek_delta;
        last_bytes_read_or_written = res.bytes_read_or_written;
    } else {
        static_assert(false, "invalid result type");
    }
    return res;
}

void ReaderWriter::_bind_methods() {
    BIND_ENUM_CONSTANT(FROM_CURRENT);
    BIND_ENUM_CONSTANT(FROM_END);
    BIND_ENUM_CONSTANT(FROM_START);
    BIND_ENUM_CONSTANT(NONE);
    BIND_ENUM_CONSTANT(INVALID_STATE);
    BIND_ENUM_CONSTANT(OUT_OF_DATA_TO_READ);
    BIND_ENUM_CONSTANT(OUT_OF_SPACE_TO_WRITE);
    BIND_ENUM_CONSTANT(SEEK_ERROR);
    BIND_ENUM_CONSTANT(SEEK_AFTER_DATA_RANGE);
    BIND_ENUM_CONSTANT(SEEK_BEFORE_DATA_RANGE);
    // ClassDB::bind_method(D_METHOD("seek_read_pos", "delta", "from"), &ReaderWriter::seek_read_pos);
    // ClassDB::bind_method(D_METHOD("seek_write_pos", "delta", "from"), &ReaderWriter::seek_write_pos);
    // ClassDB::bind_method(D_METHOD("get_bytes", "delta", "from"), &ReaderWriter::get_bytes);
    // ClassDB::bind_method(D_METHOD("read_bytes", "delta", "from"), &ReaderWriter::read_bytes);
    bind_gds_single_int_elems(int64_t, integer, int64_t);
    bind_gds_single_float_elems(double, float, double);
    bind_gds_single(bool, bool, bool);
    bind_gds_single_float_elems(Vector2, vec2, real_t);
    bind_gds_single_int_elems(Vector2i, vec2i, int32_t);
    bind_gds_single_float_elems(Vector3, vec3, real_t);
    bind_gds_single_int_elems(Vector3i, vec3i, int32_t);
    bind_gds_single_float_elems(Vector4, vec4, real_t);
    bind_gds_single_int_elems(Vector4i, vec4i, int32_t);
    bind_gds_single_float_elems(Rect2, rect2, real_t);
    bind_gds_single_int_elems(Rect2i, rect2i, int32_t);
    bind_gds_single_float_elems(Color, color, float);
    bind_gds_single_float_elems(Plane, plane, real_t);
    bind_gds_single_float_elems(::AABB, aabb, real_t);
    bind_gds_single_float_elems(Transform2D, transform2d, real_t);
    bind_gds_single_float_elems(Transform3D, transform3d, real_t);
    bind_gds_single_float_elems(Projection, projection, real_t);
    bind_gds_single_float_elems(Basis, basis, real_t);

    bind_gds_array_int_elems(int64_t,, Array, integer, int64_t);
    bind_gds_array_float_elems(double,, Array, float, double);
    bind_gds_array(bool,, Array, bool, bool);
    bind_gds_array_float_elems(Vector2,, Array, vec2, real_t);
    bind_gds_array_int_elems(Vector2i,, Array, vec2i, int32_t);
    bind_gds_array_float_elems(Vector3,, Array, vec3, real_t);
    bind_gds_array_int_elems(Vector3i,, Array, vec3i, int32_t);
    bind_gds_array_float_elems(Vector4,, Array, vec4, real_t);
    bind_gds_array_int_elems(Vector4i,, Array, vec4i, int32_t);
    bind_gds_array_float_elems(Rect2,, Array, rect2, real_t);
    bind_gds_array_int_elems(Rect2i,, Array, rect2i, int32_t);
    bind_gds_array_float_elems(Color,, Array, color, float);
    bind_gds_array_float_elems(Plane,, Array, plane, real_t);
    bind_gds_array_float_elems(::AABB,, Array, aabb, real_t);
    bind_gds_array_float_elems(Transform2D,, Array, transform2d, real_t);
    bind_gds_array_float_elems(Transform3D,, Array, transform3d, real_t);
    bind_gds_array_float_elems(Projection,, Array, projection, real_t);
    bind_gds_array_float_elems(Basis,, Array, basis, real_t);
}

void ReaderWriter::clear_errors() {
    first_error = ERROR::NONE;
    last_error = ERROR::NONE;
}
bool ReaderWriter::has_error() {
    return first_error != ERROR::NONE;
}
uint8_t ReaderWriter::get_first_error() {
    return first_error;
}
uint8_t ReaderWriter::get_last_error() {
    return last_error;
}
int64_t ReaderWriter::get_read_pos() {
    return collect_results(vtable->get_read_pos(object)).real_seek_delta;
}
int64_t ReaderWriter::get_write_pos() {
    return collect_results(vtable->get_write_pos(object)).real_seek_delta;
}
ReaderWriter::SeekResult ReaderWriter::seek_read_pos(int64_t delta, SEEK from) {
    return collect_results(vtable->seek_read_pos(object, delta, from));
}
ReaderWriter::SeekResult ReaderWriter::seek_write_pos(int64_t delta, SEEK from) {
    return collect_results(vtable->seek_write_pos(object, delta, from));
}
ReaderWriter::GetSetResult ReaderWriter::get_bytes(void* data_dst, uint32_t num_bytes) {
    return collect_results(vtable->get_bytes(object, data_dst, num_bytes));
}
ReaderWriter::ReadWriteResult ReaderWriter::read_bytes(void* data_dst, uint32_t num_bytes) {
    return collect_results(vtable->read_bytes(object, data_dst, num_bytes));
}
ReaderWriter::GetSetResult ReaderWriter::set_bytes(const void* data_src, uint32_t num_bytes) {
    return collect_results(vtable->set_bytes(object, data_src, num_bytes));
}
ReaderWriter::ReadWriteResult ReaderWriter::write_bytes(const void* data_src, uint32_t num_bytes) {
    return collect_results(vtable->write_bytes(object, data_src, num_bytes));
}
template<typename T>
ReaderWriter::SeekResult ReaderWriter::seek_read_by_t_size() {
    return collect_results(vtable->seek_read_pos(object, sizeof(T), SEEK::FROM_CURRENT));
}
template<typename T>
ReaderWriter::SeekResult ReaderWriter::seek_write_by_t_size() {
    return collect_results(vtable->seek_write_pos(object, sizeof(T), SEEK::FROM_CURRENT));
}
template<typename T>
ReaderWriter::ReadWriteResult ReaderWriter::read_t(T* val_dst) {
    return read_bytes(reinterpret_cast<void*>(val_dst), sizeof(T));
}
template<typename T>
ReaderWriter::GetSetResult ReaderWriter::set_t(const T* val_src) {
    return set_bytes(reinterpret_cast<const void*>(val_src), sizeof(T));
}
template<typename T>
ReaderWriter::ReadWriteResult ReaderWriter::write_t(const T* val_src) {
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
ReaderWriter::SerialError ReaderWriter::set_t_val(T val) {
    return set_bytes(reinterpret_cast<const void*>(&val), sizeof(T)).err;
}
template<typename T>
ReaderWriter::SerialError ReaderWriter::write_t_val(T val) {
    return write_bytes(reinterpret_cast<const void*>(&val), sizeof(T)).err;
}
template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::GetSetResult ReaderWriter::get_t_cast(T_NATIVE* val_dst) {
    T_SERIAL val_ser;
    GetSetResult res = get_bytes(reinterpret_cast<void*>(&val_ser), sizeof(T_SERIAL))
    *val_dst = static_cast<T_NATIVE>(val_ser);
    return res;
}
template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::ReadWriteResult ReaderWriter::read_t_cast(T_NATIVE* val_dst) {
    T_SERIAL val_ser;
    ReadWriteResult res = read_bytes(reinterpret_cast<void*>(&val_ser), sizeof(T_SERIAL));
    *val_dst = static_cast<T_NATIVE>(val_ser);
    return res;
}
template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::GetSetResult ReaderWriter::set_t_cast(const T_NATIVE* val_src) {
    T_SERIAL val_ser = static_cast<T_SERIAL>(*val_src);
    return set_bytes(reinterpret_cast<const void*>(&val_ser), sizeof(T_SERIAL));
}
template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::ReadWriteResult ReaderWriter::write_t_cast(const T_NATIVE* val_src) {
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
ReaderWriter::SerialError ReaderWriter::set_t_cast_val(T_NATIVE val_src) {
    T_SERIAL val_ser = static_cast<T_SERIAL>(val_src);
    return set_bytes(reinterpret_cast<const void*>(&val_ser), sizeof(T_SERIAL)).err;
}
template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::SerialError ReaderWriter::write_t_cast_val(T_NATIVE val_src) {
    T_SERIAL val_ser = static_cast<T_SERIAL>(val_src);
    return write_bytes(reinterpret_cast<const void*>(&val_ser), sizeof(T_SERIAL)).err;
}

template<typename T_SERIAL, typename T_NATIVE>
T_NATIVE ReaderWriter::get_gds() {
    int64_t initial_pos = get_read_pos();
    T_NATIVE out = read_gds<T_SERIAL, T_NATIVE>();
    seek_read_pos(initial_pos, SEEK::FROM_START);
    return out;
}

template<typename T_SERIAL, typename T_NATIVE>
T_NATIVE ReaderWriter::read_gds() {
    FIND_S(T_SERIAL)
    DEV_ASSERT_MSG(S != SerialType::_S_INVALID, "invalid serial type");
    FIND_N(T_NATIVE)
    DEV_ASSERT_MSG(N != SerialType::_N_INVALID, "invalid native type for get_gds/read_gds");
    //                                    bool  int      float   vec2    vec2i    vec3    vec3i    vec4    vec4i    rect2   rect2i   color  xfrm2   xfrm3   plane   basis   proj    quat    aabb
    using T_NATIVE_ELEM_LIST = std::tuple<bool, int64_t, double, real_t, int32_t, real_t, int32_t, real_t, int32_t, real_t, int32_t, float, real_t, real_t, real_t, real_t, real_t, real_t, real_t>;
    using T_NATIVE_ELEM = std::tuple_element_t<N, T_NATIVE_ELEM_LIST>;
    const uint32_t SERIAL_ELEM_SIZE = SerialType::serial_elem_size(S);
    const uint32_t NATIVE_ELEM_SIZE = SerialType::native_elem_size(N);
    const uint32_t NATIVE_STRIDE = SerialType::native_stride(N);
    const uint32_t ELEM_COUNT = SerialType::native_elem_count(N);
    DEV_ASSERT_MSG(NATIVE_STRIDE == sizeof(T_NATIVE), "native stride calculated by SerialType does not match sizeof(T_NATIVE)");
    T_NATIVE out;
    if constexpr (SERIAL_ELEM_SIZE == NATIVE_ELEM_SIZE && S == SerialType::native_elem_type(N)) {
        read_bytes(&out, NATIVE_STRIDE);
    } else if constexpr (ELEM_COUNT == 1) {
        read_t_cast<T_SERIAL>(reinterpret_cast<T_NATIVE_ELEM*>(&out));
    } else {
        T_NATIVE_ELEM* elem_ptr = reinterpret_cast<T_NATIVE_ELEM*>(&out);
        for (uint32_t e = 0; e < ELEM_COUNT; e += 1) {
            read_t_cast<T_SERIAL>(elem_ptr);
            elem_ptr += 1;
        }
    }
    return out;
}

template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::SerialError ReaderWriter::set_gds(T_NATIVE val) {
    int64_t initial_pos = get_write_pos();
    SerialError err = write_gds<T_SERIAL, T_NATIVE>(val);
    seek_write_pos(initial_pos, SEEK::FROM_START);
    return err;
}

template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::SerialError ReaderWriter::write_gds(T_NATIVE val) {
    FIND_S(T_SERIAL)
    DEV_ASSERT_MSG(S != SerialType::_S_INVALID, "invalid serial type");
    FIND_N(T_NATIVE)
    DEV_ASSERT_MSG(N != SerialType::_N_INVALID, "invalid native type for get_gds/read_gds");
    using T_NATIVE_ELEM_LIST = std::tuple<bool, int64_t, double, real_t, int32_t, real_t, int32_t, real_t, int32_t, real_t, int32_t, float, real_t, real_t, real_t, real_t, real_t, real_t, real_t>;
    using T_NATIVE_ELEM = std::tuple_element_t<N, T_NATIVE_ELEM_LIST>;
    const uint32_t SERIAL_ELEM_SIZE = SerialType::serial_elem_size(S);
    const uint32_t NATIVE_ELEM_SIZE = SerialType::native_elem_size(N);
    const uint32_t NATIVE_STRIDE = SerialType::native_stride(N);
    const uint32_t ELEM_COUNT = SerialType::native_elem_count(N);
    DEV_ASSERT_MSG(NATIVE_STRIDE == sizeof(T_NATIVE), "native stride calculated by SerialType does not match sizeof(T_NATIVE)");
    if constexpr (SERIAL_ELEM_SIZE == NATIVE_ELEM_SIZE && S == SerialType::native_elem_type(N)) {
        write_bytes(&val, NATIVE_STRIDE);
    } else if constexpr (ELEM_COUNT == 1) {
        write_t_cast<T_SERIAL>(reinterpret_cast<T_NATIVE_ELEM*>(&val));
    } else {
        T_NATIVE_ELEM* elem_ptr = reinterpret_cast<T_NATIVE_ELEM*>(&val);
        for (uint32_t e = 0; e < ELEM_COUNT; e += 1) {
            write_t_cast<T_SERIAL>(elem_ptr);
            elem_ptr += 1;
        }
    }
    return first_error;
}

template<typename T>
ReaderWriter::SerialError ReaderWriter::get_t_array(T* val_dst, uint32_t count) {
    get_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    return first_error;
}
template<typename T>
ReaderWriter::SerialError ReaderWriter::read_t_array(T* val_dst, uint32_t count) {
    read_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    return first_error;
}
template<typename T>
ReaderWriter::SerialError ReaderWriter::set_t_array(const T* val_src, uint32_t count) {
    set_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    return first_error;
}
template<typename T>
ReaderWriter::SerialError ReaderWriter::write_t_array(const T* val_src, uint32_t count) {
    write_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    return first_error;
}
template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::SerialError ReaderWriter::get_t_cast_array(T_NATIVE* val_dst, uint32_t count) {
    int64_t init_pos = get_read_pos();
    for (uint32_t i = 0; i < count; i += 1) {
        read_t_cast<T_SERIAL>(val_dst);
        val_dst += 1;
    }
    seek_read_pos(init_pos, SEEK::FROM_START);
    return first_error;
}
template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::SerialError ReaderWriter::read_t_cast_array(T_NATIVE* val_dst, uint32_t count) {
    for (uint32_t i = 0; i < count; i += 1) {
        read_t_cast<T_SERIAL>(val_dst);
        val_dst += 1;
    }
    return first_error;
}
template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::SerialError ReaderWriter::set_t_cast_array(const T_NATIVE* val_src, uint32_t count) {
    int64_t init_pos = get_write_pos();
    for (uint32_t i = 0; i < count; i += 1) {
        set_t_cast<T_SERIAL>(val_src);
        val_src += 1;
    }
    seek_write_pos(init_pos, SEEK::FROM_START);
    return first_error;
}
template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::SerialError ReaderWriter::write_t_cast_array(const T_NATIVE* val_src, uint32_t count) {
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
ReaderWriter::SerialError ReaderWriter::set_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
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
ReaderWriter::SerialError ReaderWriter::write_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
    uint32_t end = arr_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(arr.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`arr_offset` + `count` is greater than the `size()` of the array provided as the data source");
    for (uint32_t i = arr_offset; i < end; i += 1) {
        write_t_cast<T_SERIAL>((T_NATIVE)arr[i]);
    }
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
T_ARRAY ReaderWriter::get_gds_array(T_ARRAY dest_array, uint32_t array_offset, uint32_t count) {
    int64_t initial_pos = get_read_pos();
    T_ARRAY arr = read_gds_array<T_SERIAL, T_NATIVE, T_ARRAY>(dest_array, array_offset, count);
    seek_read_pos(initial_pos, SEEK::FROM_START);
    return arr;
}
template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
T_ARRAY ReaderWriter::read_gds_array(T_ARRAY dest_array, uint32_t array_offset, uint32_t count) {
    uint32_t end = array_offset + count;
    dest_array.resize(end);
    for (uint32_t i = array_offset; i < end; i += 1) {
        dest_array[i] = read_gds<T_SERIAL, T_NATIVE>();
    }
    return dest_array;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
ReaderWriter::SerialError ReaderWriter::set_gds_array(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    int64_t initial_pos = get_write_pos();
    write_gds_array<T_SERIAL, T_NATIVE, T_ARRAY>(src_array, array_offset, count);
    seek_write_pos(initial_pos, SEEK::FROM_START);
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
ReaderWriter::SerialError ReaderWriter::write_gds_array(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    uint32_t end = array_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(src_array.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`array_offset` + `count` is greater than the `size()` of the array provided as the data source");
    for (uint32_t i = array_offset; i < end; i += 1) {
        write_gds<T_SERIAL, T_NATIVE>(src_array[i]);
    }
    return first_error;
}

template<typename T>
ReaderWriter::SerialError ReaderWriter::get_t_array_len_prefix(T* val_dst) {
    int64_t init_pos = get_read_pos();
    uint32_t count = read_t_val<uint32_t>();
    get_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    seek_read_pos(init_pos, SEEK::FROM_START);
    return first_error;
}
template<typename T>
ReaderWriter::SerialError ReaderWriter::read_t_array_len_prefix(T* val_dst) {
    uint32_t count = read_t_val<uint32_t>();
    read_bytes(reinterpret_cast<void*>(val_dst), count * sizeof(T));
    return first_error;
}
template<typename T>
ReaderWriter::SerialError ReaderWriter::set_t_array_len_prefix(const T* val_src, uint32_t count) {
    int64_t init_pos = get_write_pos();
    write_t_val(count);
    set_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    seek_write_pos(init_pos, SEEK::FROM_START);
    return first_error;
}
template<typename T>
ReaderWriter::SerialError ReaderWriter::write_t_array_len_prefix(const T* val_src, uint32_t count) {
    int64_t init_pos = get_write_pos();
    write_t_val(count);
    set_bytes(reinterpret_cast<const void*>(val_src), count * sizeof(T));
    seek_write_pos(init_pos, SEEK::FROM_START);
    return first_error;
}
template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::SerialError ReaderWriter::get_t_cast_array_len_prefix(T_NATIVE* val_dst) {
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
ReaderWriter::SerialError ReaderWriter::read_t_cast_array_len_prefix(T_NATIVE* val_dst) {
    uint32_t count;
    read_t(&count);
    for (uint32_t i = 0; i < count; i += 1) {
        read_t_cast<T_SERIAL>(val_dst);
        val_dst += 1;
    }
    return first_error;
}
template<typename T_SERIAL, typename T_NATIVE>
ReaderWriter::SerialError ReaderWriter::set_t_cast_array_len_prefix(const T_NATIVE* val_src, uint32_t count) {
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
ReaderWriter::SerialError ReaderWriter::write_t_cast_array_len_prefix(const T_NATIVE* val_src, uint32_t count) {
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
ReaderWriter::ReaderWriter::SerialError ReaderWriter::set_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
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
ReaderWriter::ReaderWriter::SerialError ReaderWriter::write_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset, uint32_t count) {
    uint32_t end = arr_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(arr.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`arr_offset` + `count` is greater than the `size()` of the array provided as the data source");
    write_t(&count);
    for (uint32_t i = arr_offset; i < end; i += 1) {
        write_t_cast<T_SERIAL>((T_NATIVE)arr[i]);
    }
    return first_error;
}


template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
T_ARRAY ReaderWriter::get_gds_array_len_prefix(T_ARRAY dest_array, uint32_t array_offset) {
    int64_t initial_pos = get_read_pos();
    T_ARRAY arr = read_gds_array_len_prefix<T_SERIAL, T_NATIVE, T_ARRAY>(dest_array, array_offset);
    seek_read_pos(initial_pos, SEEK::FROM_START);
    return arr;
}
template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
T_ARRAY ReaderWriter::read_gds_array_len_prefix(T_ARRAY dest_array, uint32_t array_offset) {
    uint32_t count = read_t_val<uint32_t>();
    uint32_t end = array_offset + count;
    dest_array.resize(end);
    for (uint32_t i = array_offset; i < end; i += 1) {
        dest_array[i] = read_gds<T_SERIAL, T_NATIVE>();
    }
    return dest_array;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
ReaderWriter::SerialError ReaderWriter::set_gds_array_len_prefix(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    int64_t initial_pos = get_write_pos();
    write_gds_array_len_prefix<T_SERIAL, T_NATIVE, T_ARRAY>(src_array, array_offset, count);
    seek_write_pos(initial_pos, SEEK::FROM_START);
    return first_error;
}

template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
ReaderWriter::SerialError ReaderWriter::write_gds_array_len_prefix(T_ARRAY src_array, uint32_t array_offset, uint32_t count) {
    uint32_t end = array_offset + count;
    ERR_FAIL_COND_V_MSG(end > static_cast<uint32_t>(src_array.size()), ERROR::ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT, "`array_offset` + `count` is greater than the `size()` of the array provided as the data source");
    write_t_val(count);
    for (uint32_t i = array_offset; i < end; i += 1) {
        write_gds<T_SERIAL, T_NATIVE>(src_array[i]);
    }
    return first_error;
}

/*****************
 *  FileAccess
 *****************/
ReaderWriter::SeekResult file_access_position(Ref<RefCounted> p_object) {
    if (p_object.is_null()) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
    if (fa == nullptr) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    if (!fa->is_open()) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    int64_t pos = static_cast<int64_t>(fa->get_position());
    return ReaderWriter::SeekResult{pos, ReaderWriter::ERROR::NONE};
}
ReaderWriter::SeekResult file_access_seek(Ref<RefCounted> p_object, int64_t delta, ReaderWriter::SEEK from) {
    if (p_object.is_null()) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
    if (fa == nullptr) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    if (!fa->is_open()) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    int64_t pos = static_cast<int64_t>(fa->get_position());
    int64_t new_pos = pos;
    switch (from) {
        case ReaderWriter::SEEK::FROM_START: {
            ERR_FAIL_COND_V_MSG(delta < 0, ReaderWriter::SeekResult::error(ReaderWriter::ERROR::SEEK_BEFORE_DATA_RANGE), "cannot seek to before beginning of data");
            fa->seek(static_cast<uint64_t>(delta));
            int64_t e = fa->get_error();
            ERR_FAIL_COND_V_MSG(e > 0, ReaderWriter::SeekResult::error(ReaderWriter::ERROR::SEEK_AFTER_DATA_RANGE), "cannot seek to after end of data");
            break;
        }
        case ReaderWriter::SEEK::FROM_CURRENT: {
            int64_t spos = pos;
            spos += delta;
            ERR_FAIL_COND_V_MSG(spos < 0, ReaderWriter::SeekResult::error(ReaderWriter::ERROR::SEEK_BEFORE_DATA_RANGE), "cannot seek to before beginning of data");
            fa->seek(static_cast<uint64_t>(spos));
            int64_t e = fa->get_error();
            ERR_FAIL_COND_V_MSG(e > 0, ReaderWriter::SeekResult::error(ReaderWriter::ERROR::SEEK_AFTER_DATA_RANGE), "cannot seek to after end of data");
            break;
        }
        case ReaderWriter::SEEK::FROM_END: {
            ERR_FAIL_COND_V_MSG(delta > 0, ReaderWriter::SeekResult::error(ReaderWriter::ERROR::SEEK_AFTER_DATA_RANGE), "cannot seek to after end of data");
            fa->seek_end(delta);
            int64_t e = fa->get_error();
            ERR_FAIL_COND_V_MSG(e > 0, ReaderWriter::SeekResult::error(ReaderWriter::ERROR::SEEK_BEFORE_DATA_RANGE), "cannot seek to before beginning of data");
            break;
        }
    }
    new_pos = static_cast<int64_t>(fa->get_position());
    return ReaderWriter::SeekResult{new_pos - pos, ReaderWriter::ERROR::NONE};
}
ReaderWriter::ReadWriteResult file_access_read(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
    if (p_object.is_null()) {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
    if (fa == nullptr) {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    if (!fa->is_open()) {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    uint64_t num_read = fa->get_buffer(reinterpret_cast<uint8_t*>(data_dst), static_cast<uint64_t>(num_bytes));
    uint8_t e = ReaderWriter::ERROR::NONE;
    if (num_read < num_bytes) {
        e = ReaderWriter::ERROR::OUT_OF_DATA_TO_READ;
    }
    return ReaderWriter::ReadWriteResult{static_cast<int64_t>(num_read), static_cast<uint32_t>(num_read), e};
}
ReaderWriter::GetSetResult file_access_get(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
    if (p_object.is_null()) {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
    if (fa == nullptr) {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    if (!fa->is_open()) {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    int64_t initial_pos = static_cast<int64_t>(fa->get_position());
    uint64_t num_read = fa->get_buffer(reinterpret_cast<uint8_t*>(data_dst), static_cast<uint64_t>(num_bytes));
    uint8_t e = ReaderWriter::ERROR::NONE;
    if (num_read < num_bytes) {
        e = ReaderWriter::ERROR::OUT_OF_DATA_TO_READ;
    }
    fa->seek(initial_pos);
    if (e == 0) {
        if (fa->get_error()) {
            e = ReaderWriter::ERROR::SEEK_ERROR;
        }
    }
    return ReaderWriter::GetSetResult{static_cast<uint32_t>(num_read), e};
}
ReaderWriter::ReadWriteResult file_access_write(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
    if (p_object.is_null()) {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
    if (fa == nullptr) {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    if (!fa->is_open()) {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    uint64_t num_written = fa->store_buffer(reinterpret_cast<const uint8_t*>(data_src), static_cast<uint64_t>(num_bytes));
    uint8_t e = ReaderWriter::ERROR::NONE;
    if (num_written < num_bytes) {
        e = ReaderWriter::ERROR::OUT_OF_SPACE_TO_WRITE;
    }
    return ReaderWriter::ReadWriteResult{static_cast<int64_t>(num_written), static_cast<uint32_t>(num_written), e};
}
ReaderWriter::GetSetResult file_access_set(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
    if (p_object.is_null()) {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
    if (fa == nullptr) {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    if (!fa->is_open()) {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    int64_t initial_pos = static_cast<int64_t>(fa->get_position());
    uint64_t num_written = fa->store_buffer(reinterpret_cast<const uint8_t*>(data_src), static_cast<uint64_t>(num_bytes));
    uint8_t e = ReaderWriter::ERROR::NONE;
    if (num_written < num_bytes) {
        e = ReaderWriter::ERROR::OUT_OF_SPACE_TO_WRITE;
    }
    fa->seek(initial_pos);
    if (e == 0) {
        if (fa->get_error()) {
            e = ReaderWriter::ERROR::SEEK_ERROR;
        }
    }
    return ReaderWriter::GetSetResult{static_cast<uint32_t>(num_written), e};
}
/*****************
 *  PackedByteArray
 *****************/
ReaderWriter::SeekResult packed_bytes_read_pos(Ref<RefCounted> p_object) {
    if (p_object.is_null()) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
    if (wrapper == nullptr) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    return ReaderWriter::SeekResult{wrapper->rpos, ReaderWriter::ERROR::NONE};
}
ReaderWriter::SeekResult packed_bytes_write_pos(Ref<RefCounted> p_object) {
    if (p_object.is_null()) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
    if (wrapper == nullptr) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    return ReaderWriter::SeekResult{wrapper->wpos, ReaderWriter::ERROR::NONE};
}
ReaderWriter::SeekResult packed_bytes_seek_read(Ref<RefCounted> p_object, int64_t delta, ReaderWriter::SEEK from) {
    if (p_object.is_null()) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
    if (wrapper == nullptr) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    int64_t old_pos = wrapper->rpos;
    int64_t new_pos = old_pos;
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
            new_pos = static_cast<int64_t>(wrapper->arr.size()) + delta;
            break;
        }
    }
    uint8_t e = ReaderWriter::ERROR::NONE;
    if (new_pos < 0) {e = ReaderWriter::ERROR::SEEK_BEFORE_DATA_RANGE;}
    else if (new_pos > wrapper->arr.size()) {e = ReaderWriter::ERROR::SEEK_AFTER_DATA_RANGE;}
    new_pos = MAX((int64_t)0, MIN(new_pos, MIN(wrapper->wpos, wrapper->arr.size())));
    wrapper->rpos = new_pos;
    int64_t true_delta = new_pos - old_pos;
    return ReaderWriter::SeekResult{true_delta, e};
}
ReaderWriter::SeekResult packed_bytes_seek_write(Ref<RefCounted> p_object, int64_t delta, ReaderWriter::SEEK from) {
    if (p_object.is_null()) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
    if (wrapper == nullptr) {return ReaderWriter::SeekResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    int64_t old_pos = wrapper->wpos;
    int64_t new_pos = old_pos;
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
            new_pos = static_cast<int64_t>(wrapper->arr.size()) + delta;
            break;
        }
    }
    uint8_t e = ReaderWriter::ERROR::NONE;
    if (new_pos < 0) {e = ReaderWriter::ERROR::SEEK_BEFORE_DATA_RANGE;}
    else if (new_pos > wrapper->arr.size()) {e = ReaderWriter::ERROR::SEEK_AFTER_DATA_RANGE;}
    new_pos = MAX(MAX((int64_t)0, wrapper->rpos), MIN(new_pos,  wrapper->arr.size()));
    wrapper->wpos = new_pos;
    int64_t true_delta = new_pos - old_pos;
    return ReaderWriter::SeekResult{true_delta, e};
}
ReaderWriter::ReadWriteResult packed_bytes_read(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
    if (p_object.is_null()) {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
    if (wrapper == nullptr) {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    int64_t end = wrapper->rpos + static_cast<int64_t>(num_bytes);
    if (end > static_cast<int64_t>(wrapper->arr.size()))  {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::OUT_OF_DATA_TO_READ);}
    const uint8_t* data_src = wrapper->arr.ptr();
    memcpy(data_dst, data_src, num_bytes);
    wrapper->rpos = end;
    return ReaderWriter::ReadWriteResult{num_bytes, num_bytes, ReaderWriter::ERROR::NONE};
}
ReaderWriter::GetSetResult packed_bytes_get(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
    if (p_object.is_null()) {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
    if (wrapper == nullptr) {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    int64_t end = wrapper->rpos + static_cast<int64_t>(num_bytes);
    if (end > static_cast<int64_t>(wrapper->arr.size()))  {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::OUT_OF_DATA_TO_READ);}
    const uint8_t* data_src = wrapper->arr.ptr();
    memcpy(data_dst, data_src, num_bytes);
    return ReaderWriter::GetSetResult{num_bytes, ReaderWriter::ERROR::NONE};
}
ReaderWriter::ReadWriteResult packed_bytes_write(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
    if (p_object.is_null()) {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
    if (wrapper == nullptr) {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    int64_t end = wrapper->wpos + static_cast<int64_t>(num_bytes);
    if (end > static_cast<int64_t>(wrapper->arr.size()))  {
        if (wrapper->arr.resize(end)) {
            {return ReaderWriter::ReadWriteResult::error(ReaderWriter::ERROR::OUT_OF_SPACE_TO_WRITE);}
        }
    }
    uint8_t* data_dst = wrapper->arr.ptrw();
    memcpy(data_dst, data_src, num_bytes);
    wrapper->wpos = end;
    return ReaderWriter::ReadWriteResult{num_bytes, num_bytes, ReaderWriter::ERROR::NONE};
}
ReaderWriter::GetSetResult packed_bytes_set(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
    if (p_object.is_null()) {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
    if (wrapper == nullptr) {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::INVALID_STATE);}
    int64_t end = wrapper->wpos + static_cast<int64_t>(num_bytes);
    if (end > static_cast<int64_t>(wrapper->arr.size())) {
        if (wrapper->arr.resize(end)) {
            {return ReaderWriter::GetSetResult::error(ReaderWriter::ERROR::OUT_OF_SPACE_TO_WRITE);}
        }
    }
    uint8_t* data_dst = wrapper->arr.ptrw();
    memcpy(data_dst, data_src, num_bytes);
    return ReaderWriter::GetSetResult{num_bytes, ReaderWriter::ERROR::NONE};
}