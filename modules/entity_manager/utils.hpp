
#pragma once

#include "core/error/error_macros.h"
#include "core/typedefs.h"
#include "core/variant/array.h"
#include "core/variant/variant.h"
#include <sys/types.h>
#include <cstdint>
#include <cstring>

#define ptrcast(m_type, m_mem) reinterpret_cast<m_type>(m_mem)

_FORCE_INLINE_ uint32_t float_as_uint32(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

uint16_t float_to_half_u16(float f) {
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

_FORCE_INLINE_ float uint32_as_float(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

float half_u16_to_float(uint16_t h) {
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


template<uint8_t ELEM_COUNT, typename SRC_T, bool SRC_HALF, typename DST_T, bool DST_HALF>
_FORCE_INLINE_ void _move_t_single(const SRC_T* src_ptr, DST_T* dst_ptr) {
    if constexpr (SRC_HALF && DST_HALF) {
        DEV_ASSERT_MSG(false, "invalid template settings: both source and dest are half-precision floats");
    }
    if constexpr (ELEM_COUNT > 1) {
        for (uint16_t i = 0; i < ELEM_COUNT; i += 1) {
            if constexpr (SRC_HALF) {
                *reinterpret_cast<DST_T*>(dst_ptr) = half_u16_to_float(*src_ptr);
            } else if (DST_HALF) {
                *reinterpret_cast<DST_T*>(dst_ptr) = float_to_half_u16(*src_ptr);
            } else {
                *reinterpret_cast<DST_T*>(dst_ptr) = static_cast<DST_T>(*src_ptr);
            }
            dst_ptr += 1;
            src_ptr += 1;
        }
    } else {
        if constexpr (SRC_HALF) {
            *reinterpret_cast<DST_T*>(dst_ptr) = half_u16_to_float(*src_ptr);
        } else if (DST_HALF) {
            *reinterpret_cast<DST_T*>(dst_ptr) = float_to_half_u16(*src_ptr);
        } else {
            *reinterpret_cast<DST_T*>(dst_ptr) = static_cast<DST_T>(*src_ptr);
        }
    }
}
template<bool SRC_ARR, typename SRC_T, bool SRC_HALF, bool DST_ARR, typename DST_T, bool DST_HALF>
_FORCE_INLINE_ void _move_t_multi(uint32_t elem_count, std::conditional_t<SRC_ARR, const Array*, const SRC_T*> src, std::conditional_t<DST_ARR, Array*, DST_T*> dst) {
    if constexpr (SRC_HALF && DST_HALF) {
        DEV_ASSERT_MSG(false, "invalid template settings: both source and dest are half-precision floats");
    }
    if constexpr (SRC_ARR && DST_ARR) {
        DEV_ASSERT_MSG(false, "invalid template settings: both source and dest are array targets");
    }
    for (uint32_t i = 0; i < elem_count; i += 1) {
        DST_T val;
        if constexpr (SRC_HALF) {
            if constexpr (SRC_ARR) {
                val = half_u16_to_float((SRC_T)src->operator[](i));
            } else {
                val = half_u16_to_float(*src);
                src += 1;
            }
        } else if (DST_HALF) {
            if constexpr (SRC_ARR) {
                val = float_to_half_u16((SRC_T)src->operator[](i));
            } else {
                val = float_to_half_u16(*src);
                src += 1;
            }
        } else {
            if constexpr (SRC_ARR) {
                val = static_cast<DST_T>((SRC_T)src->operator[](i));
            } else {
                val = static_cast<DST_T>(*src);
                src += 1;
            }
        }
        if constexpr (DST_ARR) {
            dst->operator[](i) = Variant(val);
        } else {
            *dst = val;
            dst += 1;
        }
    }
}

#define invalid_struct_elem_case(m_struct, m_elem, m_return) \
case m_elem: { \
    ERR_FAIL_V_MSG(m_return, "element type " #m_elem " is not valid for struct type " #m_struct);\
}
// READS
#define read_array_of_structs_case(m_case, m_arr, m_obj, m_vec_num, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    m_arr arr; \
    arr.reserve(p_field_data.fixed_len); \
    m_obj obj = m_obj(); \
    m_read_t* src_ptr = p_field_data.get_elem_ptr_cast<m_read_t>(p_idx); \
    m_write_t* dst_ptr = reinterpret_cast<m_write_t*>(&obj); \
    for (int i = 0; i < p_field_data.fixed_len; i += 1) { \
        _move_t_single<m_vec_num, m_read_t, m_read_half, m_write_t, m_write_half>(src_ptr, dst_ptr); \
        arr.append(obj); \
        obj = m_obj(); \
        src_ptr += m_vec_num; \
    } \
    out = Variant(arr); \
    break; \
}
#define read_array_of_vals_direct_packed_case(m_case, m_arr, m_t) \
case m_case: { \
    m_arr arr; \
    arr.resize(p_field_data.fixed_len); \
    m_t* dst_ptr = arr.ptrw(); \
    m_t* src_ptr = p_field_data.get_elem_ptr_cast<m_t>(p_idx); \
    memcpy(dst_ptr, src_ptr, p_field_data.fixed_len * sizeof(m_t)); \
    out = Variant(arr); \
    break; \
}
#define read_array_of_vals_packed_case(m_case, m_arr, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    m_arr arr; \
    arr.reserve(p_field_data.fixed_len); \
    m_read_t* src_ptr = p_field_data.get_elem_ptr_cast<m_read_t>(p_idx); \
    m_write_t* dst_ptr = arr.ptrw(); \
    _move_t_multi<false, m_read_t, m_read_half, false, m_write_t, m_write_half>(p_field_data.fixed_len, src_ptr, dst_ptr); \
    out = Variant(arr); \
    break; \
}
#define read_array_of_vals_unpacked_case(m_case, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    Array arr; \
    arr.reserve(p_field_data.fixed_len); \
    m_read_t* src_ptr = p_field_data.get_elem_ptr_cast<m_read_t>(p_idx); \
    _move_t_multi<false, m_read_t, m_read_half, true, m_write_t, m_write_half>(p_field_data.fixed_len, src_ptr, &arr); \
    out = Variant(arr); \
    break; \
}
#define read_single_from_array_of_structs_case(m_case, m_obj, m_elem_count, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    m_obj obj = m_obj(); \
    m_read_t* src_ptr = p_field_data.get_elem_ptr_cast<m_read_t>(p_idx); \
    src_ptr += p_sub_idx * m_elem_count; \
    m_write_t* dst_ptr = reinterpret_cast<m_write_t*>(&obj); \
    _move_t_single<m_elem_count, m_read_t, m_read_half, m_write_t, m_write_half>(src_ptr, dst_ptr); \
    out = Variant(obj); \
    break; \
}
#define read_single_from_array_of_vals_case(m_case, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    m_write_t val; \
    m_read_t* src_ptr = p_field_data.get_elem_ptr_cast<m_read_t>(p_idx); \
    src_ptr += p_sub_idx; \
    _move_t_single<1, m_read_t, m_read_half, m_write_t, m_write_half>(src_ptr, &val); \
    out = Variant(val); \
    break; \
}

#define read_single_struct_case(m_case, m_obj, m_vec_num, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    m_obj obj = m_obj(); \
    m_read_t* src_ptr = p_field_data.get_elem_ptr_cast<m_read_t>(p_idx); \
    m_write_t* dst_ptr = reinterpret_cast<m_write_t*>(&obj); \
    _move_t_single<m_vec_num, m_read_t, m_read_half, m_write_t, m_write_half>(src_ptr, dst_ptr); \
    out = Variant(obj); \
    break; \
}


#define read_single_val_case(m_case, m_read_t, m_write_t) \
case m_case: { \
    out = Variant(static_cast<m_write_t>(*p_field_data.get_elem_ptr_cast<m_read_t>(p_idx))); \
    break; \
}

#define read_array_of_structs_all_elem_cases(m_case, m_vec_num, m_arr_int, m_obj_int, m_gd_t_int, m_arr_float, m_obj_float, m_gd_t_float) \
case m_case: { \
    switch (p_field_data.elem_t) { \
        read_array_of_structs_case(BOOL, m_arr_int, m_obj_int, m_vec_num, bool, false, m_gd_t_int, false); \
        read_array_of_structs_case(U8, m_arr_int, m_obj_int, m_vec_num, uint8_t, false, m_gd_t_int, false); \
        read_array_of_structs_case(I8, m_arr_int, m_obj_int, m_vec_num, int8_t, false, m_gd_t_int, false); \
        read_array_of_structs_case(U16, m_arr_int, m_obj_int, m_vec_num, uint16_t, false, m_gd_t_int, false); \
        read_array_of_structs_case(I16, m_arr_int, m_obj_int, m_vec_num, int16_t, false, m_gd_t_int, false); \
        read_array_of_structs_case(U32, m_arr_int, m_obj_int, m_vec_num, uint32_t, false, m_gd_t_int, false); \
        read_array_of_structs_case(I32, m_arr_int, m_obj_int, m_vec_num, int32_t, false, m_gd_t_int, false); \
        read_array_of_structs_case(U64, m_arr_int, m_obj_int, m_vec_num, uint64_t, false, m_gd_t_int, false); \
        read_array_of_structs_case(I64, m_arr_int, m_obj_int, m_vec_num, int64_t, false, m_gd_t_int, false); \
        read_array_of_structs_case(F16, m_arr_float, m_obj_float, m_vec_num, uint16_t, true, m_gd_t_float, false); \
        read_array_of_structs_case(F32, m_arr_float, m_obj_float, m_vec_num, float, false, m_gd_t_float, false); \
        read_array_of_structs_case(F64, m_arr_float, m_obj_float, m_vec_num, double, false, m_gd_t_float, false); \
        invalid_struct_elem_case(m_case, VARIANT, Variant()); \
        invalid_struct_elem_case(m_case, ENTITY_ID, Variant()); \
        default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid element type in entity manager"); \
    } \
    break; \
}

#define read_single_struct_all_elem_cases(m_case, m_vec_num, m_obj_int, m_gd_t_int, m_obj_float, m_gd_t_float) \
case m_case: { \
    switch (p_field_data.elem_t) { \
        read_single_struct_case(BOOL, m_obj_int, m_vec_num, bool, false, m_gd_t_int, false); \
        read_single_struct_case(U8, m_obj_int, m_vec_num, uint8_t, false, m_gd_t_int, false); \
        read_single_struct_case(I8, m_obj_int, m_vec_num, int8_t, false, m_gd_t_int, false); \
        read_single_struct_case(U16, m_obj_int, m_vec_num, uint16_t, false, m_gd_t_int, false); \
        read_single_struct_case(I16, m_obj_int, m_vec_num, int16_t, false, m_gd_t_int, false); \
        read_single_struct_case(U32, m_obj_int, m_vec_num, uint32_t, false, m_gd_t_int, false); \
        read_single_struct_case(I32, m_obj_int, m_vec_num, int32_t, false, m_gd_t_int, false); \
        read_single_struct_case(U64, m_obj_int, m_vec_num, uint64_t, false, m_gd_t_int, false); \
        read_single_struct_case(I64, m_obj_int, m_vec_num, int64_t, false, m_gd_t_int, false); \
        read_single_struct_case(F16, m_obj_float, m_vec_num, uint16_t, true, m_gd_t_float, false); \
        read_single_struct_case(F32, m_obj_float, m_vec_num, float, false, m_gd_t_float, false); \
        read_single_struct_case(F64, m_obj_float, m_vec_num, double, false, m_gd_t_float, false); \
        invalid_struct_elem_case(m_case, VARIANT, Variant()); \
        invalid_struct_elem_case(m_case, ENTITY_ID, Variant()); \
        default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid element type in entity manager"); \
    } \
    break; \
}


#define read_single_from_array_of_structs_all_elem_cases(m_case, m_vec_num, m_obj_int, m_gd_t_int, m_obj_float, m_gd_t_float) \
case m_case: { \
    switch (p_field_data.elem_t) { \
        read_single_from_array_of_structs_case(BOOL, m_obj_int, m_vec_num, bool, false, m_gd_t_int, false); \
        read_single_from_array_of_structs_case(U8, m_obj_int, m_vec_num, uint8_t, false, m_gd_t_int, false); \
        read_single_from_array_of_structs_case(I8, m_obj_int, m_vec_num, int8_t, false, m_gd_t_int, false); \
        read_single_from_array_of_structs_case(U16, m_obj_int, m_vec_num, uint16_t, false, m_gd_t_int, false); \
        read_single_from_array_of_structs_case(I16, m_obj_int, m_vec_num, int16_t, false, m_gd_t_int, false); \
        read_single_from_array_of_structs_case(U32, m_obj_int, m_vec_num, uint32_t, false, m_gd_t_int, false); \
        read_single_from_array_of_structs_case(I32, m_obj_int, m_vec_num, int32_t, false, m_gd_t_int, false); \
        read_single_from_array_of_structs_case(U64, m_obj_int, m_vec_num, uint64_t, false, m_gd_t_int, false); \
        read_single_from_array_of_structs_case(I64, m_obj_int, m_vec_num, int64_t, false, m_gd_t_int, false); \
        read_single_from_array_of_structs_case(F16, m_obj_float, m_vec_num, uint16_t, true, m_gd_t_float, false); \
        read_single_from_array_of_structs_case(F32, m_obj_float, m_vec_num, float, false, m_gd_t_float, false); \
        read_single_from_array_of_structs_case(F64, m_obj_float, m_vec_num, double, false, m_gd_t_float, false); \
        invalid_struct_elem_case(m_case, VARIANT, Variant()); \
        invalid_struct_elem_case(m_case, ENTITY_ID, Variant()); \
        default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid element type in entity manager"); \
    } \
    break; \
}

// WRITES
#define write_array_of_structs_case(m_case, m_arr, m_obj, m_vec_num, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    m_arr arr = (m_arr)value; \
    Size size = arr.size(); \
    ERR_FAIL_COND_V_MSG(size > p_field_data.fixed_len, false, "provided array was larger than the defined fixed-len length for this field"); \
    m_obj obj; \
    const m_read_t* src_ptr = reinterpret_cast<const m_read_t*>(&obj); \
    m_write_t* dst_ptr = p_field_data.get_elem_ptr_cast<m_write_t>(p_idx); \
    for (Size i = 0; i < size; i += 1) { \
        obj = arr[i]; \
        _move_t_single<m_vec_num, m_read_t, m_read_half, m_write_t, m_write_half>(src_ptr, dst_ptr); \
        dst_ptr += m_vec_num; \
    } \
    break; \
}
#define write_array_of_vals_direct_packed_case(m_case, m_arr, m_t) \
case m_case: { \
    m_arr arr = (m_arr)value; \
    Size size = arr.size(); \
    ERR_FAIL_COND_V_MSG(size > p_field_data.fixed_len, false, "provided array was larger than the defined fixed-len length for this field"); \
    m_t* dst_ptr = p_field_data.get_elem_ptr_cast<m_t>(p_idx);\
    const m_t* src_ptr = arr.ptr(); \
    memcpy(dst_ptr, src_ptr, size * sizeof(m_t)); \
    break; \
}
#define write_array_of_vals_packed_case(m_case, m_arr, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    m_arr arr = (m_arr)value;\
    Size size = arr.size(); \
    ERR_FAIL_COND_V_MSG(size > p_field_data.fixed_len, false, "provided array was larger than the defined fixed-len length for this field"); \
    const m_read_t* src_ptr = arr.ptr(); \
    m_write_t* dst_ptr = p_field_data.get_elem_ptr_cast<m_write_t>(p_idx);\
    _move_t_multi<false, m_read_t, m_read_half, false, m_write_t, m_write_half>(size, src_ptr, dst_ptr); \
    break; \
}
#define write_array_of_vals_unpacked_case(m_case, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    Array arr = (m_arr)value;\
    Size size = arr.size(); \
    ERR_FAIL_COND_V_MSG(size > p_field_data.fixed_len, false, "provided array was larger than the defined fixed-len length for this field"); \
    m_write_t* dst_ptr = p_field_data.get_elem_ptr_cast<m_write_t>(p_idx); \
    _move_t_multi<true, m_read_t, m_read_half, false, m_write_t, m_write_half>(size, &arr, dst_ptr); \
    break; \
}
#define write_single_from_array_of_structs_case(m_case, m_obj, m_elem_count, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    m_obj obj = (m_obj)value;\
    m_write_t* dst_ptr = p_field_data.get_elem_ptr_cast<m_write_t>(p_idx); \
    dst_ptr += p_sub_idx * m_elem_count; \
    const m_read_t* src_ptr = reinterpret_cast<const m_read_t*>(&obj); \
    _move_t_single<m_elem_count, m_read_t, m_read_half, m_write_t, m_write_half>(src_ptr, dst_ptr); \
    break; \
}

#define write_single_from_array_of_vals_case(m_case, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    m_read_t val = (m_read_t)value;\
    m_write_t* dst_ptr = p_field_data.get_elem_ptr_cast<m_write_t>(p_idx); \
    dst_ptr += p_sub_idx; \
    _move_t_single<1, m_read_t, m_read_half, m_write_t, m_write_half>(&val, dst_ptr); \
    break; \
}

#define write_single_struct_case(m_case, m_obj, m_vec_num, m_read_t, m_read_half, m_write_t, m_write_half) \
case m_case: { \
    m_obj obj = (m_obj)value;\
    const m_read_t* src_ptr = reinterpret_cast<const m_read_t*>(&obj); \
    m_write_t* dst_ptr = p_field_data.get_elem_ptr_cast<m_write_t>(p_idx); \
    _move_t_single<m_vec_num, m_read_t, m_read_half, m_write_t, m_write_half>(src_ptr, dst_ptr); \
    break; \
}

#define write_single_val_case(m_case, m_read_t, m_write_t) \
case m_case: { \
    *p_field_data.get_elem_ptr_cast<m_write_t>(p_idx) = static_cast<m_write_t>((m_read_t)value); \
    break; \
}

#define write_array_of_structs_all_elem_cases(m_case, m_vec_num, m_arr_int, m_obj_int, m_gd_t_int, m_arr_float, m_obj_float, m_gd_t_float) \
case m_case: { \
    switch (p_field_data.elem_t) { \
        write_array_of_structs_case(BOOL, m_arr_int, m_obj_int, m_vec_num, m_gd_t_int, false, bool, false); \
        write_array_of_structs_case(U8, m_arr_int, m_obj_int, m_vec_num, m_gd_t_int, false, uint8_t, false); \
        write_array_of_structs_case(I8, m_arr_int, m_obj_int, m_vec_num, m_gd_t_int, false, int8_t, false); \
        write_array_of_structs_case(U16, m_arr_int, m_obj_int, m_vec_num, m_gd_t_int, false, uint16_t, false); \
        write_array_of_structs_case(I16, m_arr_int, m_obj_int, m_vec_num, m_gd_t_int, false, int16_t, false); \
        write_array_of_structs_case(U32, m_arr_int, m_obj_int, m_vec_num, m_gd_t_int, false, uint32_t, false); \
        write_array_of_structs_case(I32, m_arr_int, m_obj_int, m_vec_num, m_gd_t_int, false, int32_t, false); \
        write_array_of_structs_case(U64, m_arr_int, m_obj_int, m_vec_num, m_gd_t_int, false, uint64_t, false); \
        write_array_of_structs_case(I64, m_arr_int, m_obj_int, m_vec_num, m_gd_t_int, false, int64_t, false); \
        write_array_of_structs_case(F16, m_arr_float, m_obj_float, m_vec_num, m_gd_t_float, false, uint16_t, true); \
        write_array_of_structs_case(F32, m_arr_float, m_obj_float, m_vec_num, m_gd_t_float, false, float, false); \
        write_array_of_structs_case(F64, m_arr_float, m_obj_float, m_vec_num, m_gd_t_float, false, double, false); \
        invalid_struct_elem_case(m_case, VARIANT, false); \
        invalid_struct_elem_case(m_case, ENTITY_ID, false); \
        default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid element type in entity manager"); \
    } \
    break; \
}

#define write_single_struct_all_elem_cases(m_case, m_vec_num, m_obj_int, m_gd_t_int, m_obj_float, m_gd_t_float) \
case m_case: { \
    switch (p_field_data.elem_t) { \
        write_single_struct_case(BOOL, m_obj_int, m_vec_num, m_gd_t_int, false, bool, false); \
        write_single_struct_case(U8, m_obj_int, m_vec_num, m_gd_t_int, false, uint8_t, false); \
        write_single_struct_case(I8, m_obj_int, m_vec_num, m_gd_t_int, false, int8_t, false); \
        write_single_struct_case(U16, m_obj_int, m_vec_num, m_gd_t_int, false, uint16_t, false); \
        write_single_struct_case(I16, m_obj_int, m_vec_num, m_gd_t_int, false, int16_t, false); \
        write_single_struct_case(U32, m_obj_int, m_vec_num, m_gd_t_int, false, uint32_t, false); \
        write_single_struct_case(I32, m_obj_int, m_vec_num, m_gd_t_int, false, int32_t, false); \
        write_single_struct_case(U64, m_obj_int, m_vec_num, m_gd_t_int, false, uint64_t, false); \
        write_single_struct_case(I64, m_obj_int, m_vec_num, m_gd_t_int, false, int64_t, false); \
        write_single_struct_case(F16, m_obj_float, m_vec_num, m_gd_t_float, false, uint16_t, true); \
        write_single_struct_case(F32, m_obj_float, m_vec_num, m_gd_t_float, false, float, false); \
        write_single_struct_case(F64, m_obj_float, m_vec_num, m_gd_t_float, false, double, false); \
        invalid_struct_elem_case(m_case, VARIANT, false); \
        invalid_struct_elem_case(m_case, ENTITY_ID, false); \
        default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid element type in entity manager"); \
    } \
    break; \
}

#define write_single_from_array_of_structs_all_elem_cases(m_case, m_vec_num, m_obj_int, m_gd_t_int, m_obj_float, m_gd_t_float) \
case m_case: { \
    switch (p_field_data.elem_t) { \
        write_single_from_array_of_structs_case(BOOL, m_obj_int, m_vec_num, m_gd_t_int, false, bool, false); \
        write_single_from_array_of_structs_case(U8, m_obj_int, m_vec_num, m_gd_t_int, false, uint8_t, false); \
        write_single_from_array_of_structs_case(I8, m_obj_int, m_vec_num, m_gd_t_int, false, int8_t, false); \
        write_single_from_array_of_structs_case(U16, m_obj_int, m_vec_num, m_gd_t_int, false, uint16_t, false); \
        write_single_from_array_of_structs_case(I16, m_obj_int, m_vec_num, m_gd_t_int, false, int16_t, false); \
        write_single_from_array_of_structs_case(U32, m_obj_int, m_vec_num, m_gd_t_int, false, uint32_t, false); \
        write_single_from_array_of_structs_case(I32, m_obj_int, m_vec_num, m_gd_t_int, false, int32_t, false); \
        write_single_from_array_of_structs_case(U64, m_obj_int, m_vec_num, m_gd_t_int, false, uint64_t, false); \
        write_single_from_array_of_structs_case(I64, m_obj_int, m_vec_num, m_gd_t_int, false, int64_t, false); \
        write_single_from_array_of_structs_case(F16, m_obj_float, m_vec_num, m_gd_t_float, false, uint16_t, true); \
        write_single_from_array_of_structs_case(F32, m_obj_float, m_vec_num, m_gd_t_float, false, float, false); \
        write_single_from_array_of_structs_case(F64, m_obj_float, m_vec_num, m_gd_t_float, false, double, false); \
        invalid_struct_elem_case(m_case, VARIANT, false); \
        invalid_struct_elem_case(m_case, ENTITY_ID, false); \
        default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid element type in entity manager"); \
    } \
    break; \
}
//EOF