
#pragma once

#include "core/error/error_macros.h"
#include "core/typedefs.h"
#include "core/variant/array.h"
#include "core/variant/variant.h"
#include <sys/types.h>
#include <cstdint>
#include <cstring>

#define ptrcast(m_type, m_mem) reinterpret_cast<m_type>(m_mem)

#if __cplusplus >= 202002L
#include <bit>
#endif

inline uint32_t float_as_uint32(float f) {
    uint32_t bits;
#if __cplusplus >= 202002L
    bits = std::bit_cast<uint32_t>(f);
#else
    memcpy(&bits, &f, sizeof(bits));
#endif
    return bits;
}


uint16_t float_to_half(float f) {
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

inline float uint32_as_float(uint32_t bits) {
    float f;
#if __cplusplus >= 202002L
    f = std::bit_cast<float>(bits);
#else
    std::memcpy(&f, &bits, sizeof(f));
#endif
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

#if __cplusplus >= 202002L
    bits = std::bit_cast<uint32_t>(f);
#else
    std::memcpy(&bits, &f, sizeof(bits));
#endif

    if (f >= was_infnan) {
        bits |= 255 << 23;
    }

    bits |= (h & 0x8000) << 16;

    return uint32_as_float(bits);
}

struct SrcDstOpq {
    uint8_t* src = nullptr;
    uint8_t* dst = nullptr;

    SrcDstOpq(void* s, void* d) {
        this->src = reinterpret_cast<uint8_t*>(s);
        this->dst = reinterpret_cast<uint8_t*>(d);
    }
};

struct SrcDstOpqBitRead {
    uint8_t* src_base = nullptr;
    uint8_t* dst = nullptr;
    uint32_t src_idx = 0;

    SrcDstOpqBitRead(void* s_base, uint32_t s_idx, void* d) {
        this->src_base = reinterpret_cast<uint8_t*>(s_base);
        this->dst = reinterpret_cast<uint8_t*>(d);
        this->src_idx = s_idx;
    }
};

struct SrcDstOpqBitWrite {
    uint8_t* src = nullptr;
    uint8_t* dst_base = nullptr;
    uint32_t dst_idx = 0;

    SrcDstOpqBitWrite(void* s, void* d_base, uint32_t d_idx) {
        this->src = reinterpret_cast<uint8_t*>(s);
        this->dst_base = reinterpret_cast<uint8_t*>(d_base);
        this->dst_idx = d_idx;
    }
};

template<uint8_t NUM, typename READ_T, uint8_t READ_SIZE, bool READ_HALF, typename WRITE_T, uint8_t WRITE_SIZE, bool WRITE_HALF>
_FORCE_INLINE_ SrcDstOpq _move_t(SrcDstOpq ptr) {
    if constexpr (READ_HALF) {
        for (uint16_t i = 0; i < NUM; i += 1) {
            *reinterpret_cast<WRITE_T*>(ptr.dst) = half_u16_to_float(*reinterpret_cast<READ_T*>(ptr.src));
            ptr.dst += WRITE_SIZE;
            ptr.src += READ_SIZE;
        }
    } else if constexpr (WRITE_HALF) {
        for (uint16_t i = 0; i < NUM; i += 1) {
            *reinterpret_cast<WRITE_T*>(ptr.dst) = float_to_half(*reinterpret_cast<READ_T*>(ptr.src));
            ptr.dst += WRITE_SIZE;
            ptr.src += READ_SIZE;
        }
    } else {
        for (uint16_t i = 0; i < NUM; i += 1) {
            *reinterpret_cast<WRITE_T*>(ptr.dst) = static_cast<WRITE_T>(*reinterpret_cast<READ_T*>(ptr.src));
            ptr.dst += WRITE_SIZE;
            ptr.src += READ_SIZE;
        }
    }
    return ptr;
}

template<bool IS_VAR, typename SRC_T, uint8_t ELEM_BIT_MASK, uint8_t BITS_PER_ELEM, uint8_t BLOCK_IDX_SHIFT, uint8_t SUB_IDX_MASK, uint8_t ELEM_PER_BYTE>
void _write_t_bits_multi(uint32_t num, std::conditional_t<IS_VAR, Array*, SRC_T*> src, uint8_t* base_dst_ptr, uint32_t dst_idx) {
    uint32_t block_idx = dst_idx >> BLOCK_IDX_SHIFT;
    uint32_t sub_idx = dst_idx & SUB_IDX_MASK;
    uint32_t num_in_curr_block = ELEM_PER_BYTE - sub_idx;
    uint8_t block = base_dst_ptr[block_idx];
    uint32_t a = 0;
    if (num_in_curr_block >= num) {
        uint8_t shift = (sub_idx * BITS_PER_ELEM);
        uint8_t bitmask = ELEM_BIT_MASK << shift;
        for (uint32_t n = 0; n < num; n += 1) {
            uint8_t bits;
            if constexpr (IS_VAR) {
                bits = static_cast<uint8_t>((SRC_T)src->operator[](a)) & ELEM_BIT_MASK;
            } else {
                bits = static_cast<uint8_t>(*src) & ELEM_BIT_MASK;
            }
            block &= ~bitmask;
            block |= (bits << shift);
            bitmask <<= BITS_PER_ELEM;
            shift += BITS_PER_ELEM;
            if constexpr (IS_VAR) {
                a += 1;
            } else {
                src += 1;
            }
        }
        base_dst_ptr[block_idx] = block;
        return;
    }
    uint32_t num_in_prefix_section = num_in_curr_block & SUB_IDX_MASK;
    uint32_t num_after_prefix_section = num - num_in_prefix_section;
    uint32_t num_in_suffix_section = num_after_prefix_section % ELEM_PER_BYTE;
    uint8_t num_in_full_blocks = num_after_prefix_section - num_in_suffix_section;
    // these bits vvvvv
    //         000xxxxx xxxxxxxx xx000000
    if (num_in_prefix_section > 0) {
        uint32_t delete_bits = num_in_prefix_section * BITS_PER_ELEM;
        block <<= delete_bits;
        block >>= delete_bits;
        uint8_t shift = 0;
        for (uint32_t n = 0; n < num_in_prefix_section; n += 1) {
            uint8_t bits;
            if constexpr (IS_VAR) {
                bits = static_cast<uint8_t>((SRC_T)src->operator[](a)) & ELEM_BIT_MASK;
            } else {
                bits = static_cast<uint8_t>(*src) & ELEM_BIT_MASK;
            }
            block |= (bits << shift);
            shift += BITS_PER_ELEM;
            if constexpr (IS_VAR) {
                a += 1;
            } else {
                src += 1;
            }
        }
        base_dst_ptr[block_idx] = block;
        block_idx += 1;
    }
    // these bits vvvvvvvv
    //   000xxxxx xxxxxxxx xx000000
    for (uint32_t n = 0; n < num_in_full_blocks; n += ELEM_PER_BYTE) {
        block = 0;
        uint8_t shift = 0;
        for (uint32_t nn = 0; nn < ELEM_PER_BYTE; nn += 1) {
            uint8_t bits;
            if constexpr (IS_VAR) {
                bits = static_cast<uint8_t>((SRC_T)src->operator[](a)) & ELEM_BIT_MASK;
            } else {
                bits = static_cast<uint8_t>(*src) & ELEM_BIT_MASK;
            }
            block |= (bits << shift);
            shift += BITS_PER_ELEM;
            if constexpr (IS_VAR) {
                a += 1;
            } else {
                src += 1;
            }
        }
        base_dst_ptr[block_idx] = block;
        block_idx += 1;
    }
    //        these bits vv
    // 000xxxxx xxxxxxxx xx000000
    if (num_in_suffix_section > 0) {
        block = base_dst_ptr[block_idx];
        uint32_t delete_bits = num_in_suffix_section * BITS_PER_ELEM;
        block >>= delete_bits;
        block <<= delete_bits;
        uint8_t shift = 0;
        for (uint32_t n = 0; n < num_in_suffix_section; n += 1) {
            uint8_t bits;
            if constexpr (IS_VAR) {
                bits = static_cast<uint8_t>((SRC_T)src->operator[](a)) & ELEM_BIT_MASK;
            } else {
                bits = static_cast<uint8_t>(*src) & ELEM_BIT_MASK;
            }
            block |= (bits << shift);
            shift += BITS_PER_ELEM;
            if constexpr (IS_VAR) {
                a += 1;
            } else {
                src += 1;
            }
        }
        base_dst_ptr[block_idx] = block;
    }
}
template<bool IS_VAR, typename DST_T, uint8_t ELEM_BIT_MASK, uint8_t BITS_PER_ELEM, uint8_t BLOCK_IDX_SHIFT, uint8_t SUB_IDX_MASK, uint8_t ELEM_PER_BYTE>
void _read_t_bits_multi(uint32_t num, uint8_t* base_src_ptr, uint32_t src_idx, std::conditional_t<IS_VAR, Array*, DST_T*> dst) {
    uint32_t block_idx = src_idx >> BLOCK_IDX_SHIFT;
    uint32_t sub_idx = src_idx & SUB_IDX_MASK;
    uint8_t block = base_src_ptr[block_idx];
    uint8_t shift = (sub_idx * BITS_PER_ELEM);
    uint8_t bitmask = ELEM_BIT_MASK << shift;
    uint32_t n = 0;
    while (n < num) {
        uint8_t bits = (block & bitmask) >> shift;
        if constexpr (IS_VAR) {
            dst->operator[](n) = Variant(static_cast<DST_T>(bits));
        } else {
            *dst = static_cast<DST_T>(bits);
        }
        bitmask <<= BITS_PER_ELEM;
        shift += BITS_PER_ELEM;
        sub_idx += 1;
        n += 1;
        if constexpr (!IS_VAR) {
            dst += 1;
        }
        if (sub_idx == ELEM_PER_BYTE) {
            sub_idx = 0;
            block_idx += 1;
            shift = 0;
            bitmask = ELEM_BIT_MASK;
            if (n <= num) {
                block = base_src_ptr[block_idx];
            }
        }
    }
}
template<typename READ_T, uint8_t READ_SIZE, uint8_t WRITE_BIT_MASK, uint8_t WRITE_NUM_BITS, uint8_t WRITE_BLOCK_IDX_SHIFT, uint8_t WRITE_SUB_IDX_MASK>
void _write_t_bits_single(READ_T* src_ptr, uint8_t* base_dst_ptr, uint32_t dst_idx) {
    uint32_t block_idx = dst_idx >> WRITE_BLOCK_IDX_SHIFT;
    uint32_t sub_idx = dst_idx & WRITE_SUB_IDX_MASK;
    uint8_t block = base_dst_ptr[block_idx];
    uint8_t bitmask = WRITE_BIT_MASK << (WRITE_NUM_BITS * sub_idx);
    uint8_t bits = static_cast<uint8_t>(*src_ptr & WRITE_BIT_MASK);
    block &= ~bitmask;
    block |= bits;
    base_dst_ptr[block_idx] = block;
}
template<uint8_t READ_BIT_MASK, uint8_t READ_NUM_BITS, uint8_t READ_BLOCK_IDX_SHIFT, uint8_t READ_SUB_IDX_MASK, typename WRITE_T>
void _read_t_bits_single(uint8_t* src_base_ptr, uint32_t src_idx, WRITE_T* dst_ptr) {
    uint32_t block_idx = src_idx >> READ_BLOCK_IDX_SHIFT;
    uint32_t sub_idx = src_idx & READ_SUB_IDX_MASK;
    uint8_t block = src_base_ptr[block_idx];
    uint8_t bits = (block >> (READ_NUM_BITS * sub_idx)) & READ_BIT_MASK;
    *dst_ptr = static_cast<WRITE_T>(bits);
}

#define invalid_struct_elem_case(m_struct, m_elem) \
case m_elem: { \
    ERR_FAIL_V_MSG(Variant(), "element type " #m_elem " is not valid for struct type " #m_struct);\
}

#define read_array_struct_case(m_case, m_arr, m_obj, m_vec_num, m_read_t, m_read_size, m_read_half, m_write_t, m_write_size, m_write_half) \
case m_case: { \
    m_arr arr; \
    arr.reserve(p_field_data.fixed_len); \
    m_obj obj = m_obj(); \
    SrcDstOpq ptrs = SrcDstOpq(p_field_data.get_elem_ptr_opaque(p_id_parts.get_idx()), &obj); \
    for (int i = 0; i < p_field_data.fixed_len; i += 1) { \
        ptrs = _move_t<m_vec_num, m_read_t, m_read_size, m_read_half, m_write_t, m_write_size, m_write_half>(ptrs); \
        arr.append(obj); \
        obj = {}; \
    } \
    out = Variant(arr); \
}

#define read_array_struct_bits_case(m_case, m_arr, m_obj, m_vec_num, m_read_t, m_bit_mask, m_bits_per_elem, m_block_idx_shift, m_sub_idx_mask, m_elem_per_byte) \
case m_case: { \
    m_arr arr; \
    arr.reserve(p_field_data.fixed_len); \
    m_obj obj = m_obj(); \
    uint8_t* base_src_ptr = p_field_data.get_elem_base_ptr_u8(); \
    m_read_t* dest_ptr = reinterpret_cast<m_read_t*>(&obj); \
    uint32_t num_elems = p_field_data.fixed_len * m_vec_num; \
    for (int i = 0; i < p_field_data.fixed_len; i += 1) { \
        _read_t_bits_multi<false, m_read_t, m_bit_mask, m_bits_per_elem, m_block_idx_shift, m_sub_idx_mask, m_elem_per_byte>(num_elems, base_src_ptr, p_idx, dest_ptr); \
        arr.append(obj); \
        obj = {}; \
    } \
    out = Variant(arr); \
}

#define read_array_struct_all_elem_cases(m_case, m_vec_num, m_arr_int, m_obj_int, m_read_t_int, m_read_size_int, m_arr_float, m_obj_float, m_read_t_float, m_read_size_float) \
case m_case: { \
    switch (p_field_data.elem_t) { \
        read_array_struct_bits_case(U1, m_arr_int, m_obj_int, m_vec_num, uint8_t, 0b1, 1, 3, 7, 8); \
        read_array_struct_bits_case(U2, m_arr_int, m_obj_int, m_vec_num, uint8_t, 0b11, 2, 2, 3, 4); \
        read_array_struct_bits_case(U4, m_arr_int, m_obj_int, m_vec_num, uint8_t, 0b1111, 4, 1, 1, 2); \
        read_array_struct_case(BOOL, m_arr_int, m_obj_int, m_vec_num, bool, 1, false, m_read_t_int, m_read_size_int, false); \
        read_array_struct_case(U8, m_arr_int, m_obj_int, m_vec_num, uint8_t, 1, false, m_read_t_int, m_read_size_int, false); \
        read_array_struct_case(I8, m_arr_int, m_obj_int, m_vec_num, int8_t, 1, false, m_read_t_int, m_read_size_int, false); \
        read_array_struct_case(U16, m_arr_int, m_obj_int, m_vec_num, uint16_t, 2, false, m_read_t_int, m_read_size_int, false); \
        read_array_struct_case(I16, m_arr_int, m_obj_int, m_vec_num, int16_t, 2, false, m_read_t_int, m_read_size_int, false); \
        read_array_struct_case(U32, m_arr_int, m_obj_int, m_vec_num, uint32_t, 4, false, m_read_t_int, m_read_size_int, false); \
        read_array_struct_case(I32, m_arr_int, m_obj_int, m_vec_num, int32_t, 4, false, m_read_t_int, m_read_size_int, false); \
        read_array_struct_case(U64, m_arr_int, m_obj_int, m_vec_num, uint64_t, 8, false, m_read_t_int, m_read_size_int, false); \
        read_array_struct_case(I64, m_arr_int, m_obj_int, m_vec_num, int64_t, 8, false, m_read_t_int, m_read_size_int, false); \
        read_array_struct_case(F16, m_arr_float, m_obj_float, m_vec_num, uint16_t, 2, true, m_read_t_float, m_read_size_float, false); \
        read_array_struct_case(F32, m_arr_float, m_obj_float, m_vec_num, float, 4, false, m_read_t_float, m_read_size_float, false); \
        read_array_struct_case(F64, m_arr_float, m_obj_float, m_vec_num, double, 8, false, m_read_t_float, m_read_size_float, false); \
        invalid_struct_elem_case(m_case, VARIANT); \
        invalid_struct_elem_case(m_case, ENTITY_ID); \
        default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid element type in entity manager"); \
    } \
}

#define read_single_bits_case(m_case, m_obj, m_bit_mask, m_num_bits, m_block_shift, m_sub_mask) \
case m_case: { \
    m_obj obj; \
    uint8_t* base_src_ptr = p_field_data.get_elem_ptr_cast<uint8_t>(0); \
    _read_t_bits_single<m_bit_mask, m_num_bits, m_block_shift, m_sub_mask, m_obj>(base_src_ptr, p_idx, &obj); \
    out = Variant(obj); \
    break; \
}

#define read_array_bits_nonpacked_case(m_case, m_dst_t, m_bit_mask, m_bits_per_elem, m_block_idx_shift, m_sub_idx_mask, m_elem_per_byte) \
case m_case: { \
    Array arr; \
    arr.reserve(p_field_data.fixed_len); \
    uint8_t* base_src_ptr = p_field_data.get_elem_ptr_cast<uint8_t>(0); \
    _read_t_bits_multi<true, m_dst_t, m_bit_mask, m_bits_per_elem, m_block_idx_shift, m_sub_idx_mask, m_elem_per_byte>(p_field_data.fixed_len, base_src_ptr, p_idx, &arr); \
    out = Variant(arr); \
    break; \
}

#define read_array_bits_packed_case(m_case, m_arr, m_dst_t, m_bit_mask, m_bits_per_elem, m_block_idx_shift, m_sub_idx_mask, m_elem_per_byte) \
case m_case: { \
    m_arr arr; \
    arr.reserve(p_field_data.fixed_len); \
    m_dst_t* dst_ptr = arr.ptrw(); \
    uint8_t* base_src_ptr = p_field_data.get_elem_ptr_cast<uint8_t>(0); \
    _read_t_bits_multi<false, m_dst_t, m_bit_mask, m_bits_per_elem, m_block_idx_shift, m_sub_idx_mask, m_elem_per_byte>(p_field_data.fixed_len, base_src_ptr, p_idx, dst_ptr); \
    out = Variant(arr); \
    break; \
}