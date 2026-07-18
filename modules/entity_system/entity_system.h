
#pragma once

#include "core/object/ref_counted.h"
#include "core/typedefs.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include <cstdint>

typedef uint32_t Index;
typedef uint64_t Type;
typedef uint32_t Size;
typedef uint16_t FieldIndex;
typedef uint16_t TypeIndex;
typedef int64_t Id;
typedef uint8_t GrowMode;

class EntitySystem : public RefCounted {
    GDCLASS(EntitySystem, RefCounted);
public:
    enum ELEM {
        NONE = 0,
        BOOL = 1,
        U1,
        U2,
        U4,
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
        ENTITY_ID,
        VARIANT,
        BYTE = U8,
        INT = I64,
        FLOAT = F64,
    };
    enum STRUCT {
        VEC_2 = 1,
        VEC_3,
        VEC_4,
        COLOR_3,
        COLOR_4,
        RECT_2,
        TRANSFORM_2D,
        PLANE,
        QUATERNION,
        AABB,
        TRANSFORM_3D,
        PROJECTION,
        BASIS,
        RECT_3 = AABB,
    };
    enum GROW {
        GROW_EXACT,
        GROW_QUARTER,
        GROW_HALF,
        GROW_DOUBLE,
    };

protected:
    static void _bind_methods();

private:
    enum LAYOUT {
        ELEM_MAX = VARIANT,
        NUM_ELEM_TYPES = ELEM_MAX + 1,
        ELEM_BITS = 31 - std::__countl_zero((int)ELEM_MAX),
        ELEM_MASK_LO = (1 << ELEM_BITS) - 1,
        ELEM_SHIFT = 0,
        ELEM_MASK_HI = ELEM_MASK_LO << ELEM_SHIFT,
        STRUCT_MAX = BASIS,
        NUM_STRUCT_TYPES = STRUCT_MAX + 1,
        STRUCT_BITS = 31 - std::__countl_zero((int)STRUCT_MAX),
        STRUCT_MASK_LO = (1 << STRUCT_BITS) - 1,
        STRUCT_SHIFT = ELEM_BITS,
        STRUCT_MASK_HI = STRUCT_MASK_LO << STRUCT_SHIFT,
        FIXED_MAX = 31,
        FIXED_MAX_BIASED = FIXED_MAX + 1,
        FIXED_BITS = 31 - std::__countl_zero(FIXED_MAX),
        FIXED_MASK_LO = (1 << FIXED_BITS) - 1,
        FIXED_SHIFT = (int)ELEM_BITS + (int)STRUCT_BITS,
        FIXED_MASK_HI = FIXED_MASK_LO << FIXED_SHIFT,
    };
    constexpr static const Size ELEM_SIZES[NUM_ELEM_TYPES] = {
        0, // NONE
        1, // BOOL
        1, // U1
        1, // U2
        1, // U4
        1, // U8
        1, // I8
        2, // U16
        2, // I16
        4, // U32
        4, // I32
        8, // U64
        8, // I64
        2, // F16
        4, // F32
        8, // F64
        sizeof(Variant), // VARIANT
        sizeof(Id), // ENTITY_ID
    };
    constexpr static const Size ELEM_SIZE_SHIFTS[NUM_ELEM_TYPES] = {
        0, // NONE
        0, // BOOL
        3, // U1
        2, // U2
        1, // U4
        0, // U8
        0, // I8
        0, // U16
        0, // I16
        0, // U32
        0, // I32
        0, // U64
        0, // I64
        0, // F16
        0, // F32
        0, // F64
        0, // VARIANT
        0, // ENTITY_ID
    };
    constexpr static const Size STRUCT_ELEM_COUNTS[NUM_STRUCT_TYPES] = {
        1, // NONE
        2, // VEC_2
        3, // VEC_3
        4, // VEC_4
        3, // COLOR_3
        4, // COLOR_4
        4, // RECT_2
        6, // TRANSFORM_2D
        4, // PLANE
        4, // QUATERNION
        6, // AABB
        12, // TRANSFORM_3D
        16, // PROJECTION
        9, // BASIS
    };
    static const Size VARIANT_SIZE = sizeof(Variant);
    static const Size ENTITY_ID_SIZE = 8;
    _FORCE_INLINE_ static Type get_struct_type(Type p_type) {
        return (p_type & STRUCT_MASK_HI) >> STRUCT_SHIFT;
    }
    _FORCE_INLINE_ static Size get_struct_size(Type p_type) {
        return STRUCT_ELEM_COUNTS[get_struct_type(p_type)];
    }
    _FORCE_INLINE_ static Type get_elem_type(Type p_type) {
        return (p_type & ELEM_MASK_HI) >> ELEM_SHIFT;
    }
    _FORCE_INLINE_ static Size get_elem_size(Type p_type) {
        return ELEM_SIZES[get_elem_type(p_type)];
    }
    _FORCE_INLINE_ static Size get_size_shift(Type p_type) {
        return ELEM_SIZE_SHIFTS[get_elem_type(p_type)];
    }
    _FORCE_INLINE_ static Type get_fixed_size(Type p_type) {
        return ((p_type & FIXED_MASK_HI) >> FIXED_SHIFT) + 1;
    }
    _FORCE_INLINE_ static Size get_total_size(Type p_type) {
        return ((get_elem_size(p_type) * get_struct_size(p_type) * get_fixed_size(p_type)) >> get_size_shift(p_type));
    }
    _FORCE_INLINE_ static bool is_bool(Type p_type) {
        return ((p_type & ELEM_MASK_HI) >> ELEM_SHIFT) == BOOL;
    }
    _FORCE_INLINE_ static bool is_int(Type p_type) {
        Type elem = ((p_type & ELEM_MASK_HI) >> ELEM_SHIFT);
        return elem >= U1 && elem <= I64;
    }
    _FORCE_INLINE_ static bool is_sub_int(Type p_type) {
        Type elem = ((p_type & ELEM_MASK_HI) >> ELEM_SHIFT);
        return elem >= U1 && elem <= U4;
    }
    _FORCE_INLINE_ static bool is_whole_int(Type p_type) {
        Type elem = ((p_type & ELEM_MASK_HI) >> ELEM_SHIFT);
        return elem >= U8 && elem <= I64;
    }
    _FORCE_INLINE_ static bool is_float(Type p_type) {
        Type elem = ((p_type & ELEM_MASK_HI) >> ELEM_SHIFT);
        return elem >= F16 && elem <= F64;
    }
    _FORCE_INLINE_ static bool is_numeric(Type p_type) {
        Type elem = ((p_type & ELEM_MASK_HI) >> ELEM_SHIFT);
        return elem >= U1 && elem <= F64;
    }
    _FORCE_INLINE_ static bool is_entity_id(Type p_type) {
        return ((p_type & ELEM_MASK_HI) >> ELEM_SHIFT) == ENTITY_ID;
    }
    _FORCE_INLINE_ static bool is_variant(Type p_type) {
        return ((p_type & ELEM_MASK_HI) >> ELEM_SHIFT) == VARIANT;
    }
    _FORCE_INLINE_ static Type make_type(Type p_elem, Type p_struct, Type p_fixed_len) {
        ERR_FAIL_COND_V_MSG(p_elem >= NUM_ELEM_TYPES, 0, "element type is invalid");
        ERR_FAIL_COND_V_MSG(p_struct >= NUM_STRUCT_TYPES, 0, "struct type is invalid");
        ERR_FAIL_COND_V_MSG(p_fixed_len <= 0 || p_fixed_len > FIXED_MAX_BIASED, 0, "fixed len is either zero, or greater than max fixed len");
        p_fixed_len -= 1;
        p_elem <<= ELEM_SHIFT;
        p_struct <<= STRUCT_SHIFT;
        p_fixed_len <<= FIXED_SHIFT;
        return p_fixed_len | p_struct | p_elem;
    }
    void define_field_internal(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_field_type, bool p_has_allowed_ids, PackedInt32Array p_allowed_ids);
    
    constexpr static const Id gen_mask_lo = 0xFFFF;
    constexpr static const Id type_mask_lo = 0xFFFF;
    constexpr static const Id type_index_shift = 48;
    constexpr static const Id gen_shift = 32;
    
    // these pointer lens = num_types
    Size* lens = nullptr;
    Size* caps = nullptr;
    FieldIndex* field_idx_offset_list = nullptr;
    FieldIndex* num_fields_list = nullptr;
    FieldIndex* free_tracking_field_list = nullptr;
    FieldIndex* gen_tracking_field_list = nullptr;
    bool* at_least_1_sub_bit_field_list = nullptr;
    Size* first_free_list = nullptr;
    Size* next_unused_index_list = nullptr;
    // these pointer lens = total_num_fields
    Type* field_type_list = nullptr;
    void** field_data_ptrs = nullptr;
    Type* field_allowed_id_list_index = nullptr;
    Size* stride_list = nullptr;
    // this pointer len = total_num_id_fields
    uint64_t* field_allowed_id_list = nullptr;
    FieldIndex total_num_id_fields = 0;
    FieldIndex total_num_fields = 0;
    TypeIndex num_types = 0;
    TypeIndex next_expected_type_define = 0;
    FieldIndex num_allowed_id_blocks_per_idx = 0;
    uint8_t grow_mode = GROW_QUARTER;

    _FORCE_INLINE_ void set_allowed_type_id(FieldIndex p_field_idx, int64_t p_allowed) {
        int64_t block = p_allowed >> 6;
        int64_t bit_shift = p_allowed & 63;
        uint64_t bit = (uint64_t)1 << bit_shift;
        FieldIndex allowed_block_offset = field_allowed_id_list_index[p_field_idx];
        field_allowed_id_list[(allowed_block_offset * num_allowed_id_blocks_per_idx) + block] |= bit;
    }
    _FORCE_INLINE_ bool type_id_is_allowed_in_field(FieldIndex p_field_idx, TypeIndex p_type_idx) {
        uint64_t block = (uint64_t)p_type_idx >> 6;
        uint64_t bit_shift = (uint64_t)p_type_idx & 63;
        uint64_t bit = (uint64_t)1 << bit_shift;
        FieldIndex allowed_block_offset = field_allowed_id_list_index[p_field_idx];
        return (field_allowed_id_list[(allowed_block_offset * num_allowed_id_blocks_per_idx) + block] & bit) == bit;
    }

    enum INIT {
        UNINIT,
        SET_TOTAL_TYPES,
        ADDING_ALL_TYPES,
        DEFINING_ALL_FIELDS,
        FINALIZED,
        DEINITIALIZED,
    };
    uint8_t init_status = UNINIT;

public:
    void clear_entity_list(TypeIndex p_type);
	void deinitialize();
	_FORCE_INLINE_ bool entity_list_is_empty(TypeIndex p_type) const { return lens[p_type] == 0; }
	_FORCE_INLINE_ bool entity_list_not_empty(TypeIndex p_type) const { return lens[p_type] != 0; }
	_FORCE_INLINE_ Size get_entity_list_cap(TypeIndex p_type) const { return caps[p_type]; }
	_FORCE_INLINE_ uint32_t get_entity_list_len(TypeIndex p_type) const { return lens[p_type]; }
	void ensure_capacity_for_n_entities(TypeIndex p_type_index, Size p_size);
    Variant get(Id p_id) const;
    bool set(Id p_id, Variant p_val);
    void create(FieldIndex p_type);
    void destroy(Id p_id);
    // Init process
    void define_system(TypeIndex p_total_num_types);
    void define_type(TypeIndex p_type_idx, FieldIndex p_num_fields);
    void define_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem);
    void define_entity_id_field(TypeIndex p_type_idx, FieldIndex p_field_idx, PackedInt32Array p_allowed_fields);
    void define_struct_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_struct);
    void define_fixed_length_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_fixed);
    void define_fixed_length_entity_id_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_fixed, PackedInt32Array p_allowed_fields);
    void define_fixed_length_struct_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_struct, Type p_fixed);
    void finalize_entity_system_layout();

    EntitySystem();
    ~EntitySystem();
};

