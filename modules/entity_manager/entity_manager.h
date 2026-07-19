
#pragma once

#include "core/object/ref_counted.h"
#include "core/typedefs.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "entity_ref.h"
#include <cstdint>

#define ptrcast(m_type, m_mem) reinterpret_cast<m_type>(m_mem)

typedef uint32_t Index;
typedef uint64_t Type;
typedef uint32_t Size;
typedef uint16_t FieldIndex;
typedef uint16_t TypeIndex;
typedef uint16_t Gen;
typedef int64_t Id;
typedef uint8_t GrowMode;
typedef uint8_t TypeFlags;

class EntityManager : public RefCounted {
    GDCLASS(EntityManager, RefCounted);
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
        ENTITY_REF,
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
        ELEM_MAX = ENTITY_REF,
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
    enum META_FIELD {
        NORMAL,
        NEXT_FREE_TRACK,
        IS_FREE_TRACK,
        GEN_TRACK,
    };
    enum FLAG {
        AT_LEAST_1_SUB_BYTE_FIELD = 1 << 0,
        AT_LEAST_1_VARIANT_OR_FIELD__FIELD = 1 << 1,
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
        sizeof(Ref<EntityRef>), // ENTITY_REF
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
        0, // ENTITY_REF
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
    _FORCE_INLINE_ static bool is_variant_or_ref(Type p_type) {
        Type elem = ((p_type & ELEM_MASK_HI) >> ELEM_SHIFT);
        return elem == VARIANT || elem == ENTITY_REF;
    }
    _FORCE_INLINE_ static bool is_entity_ref(Type p_type) {
        return ((p_type & ELEM_MASK_HI) >> ELEM_SHIFT) == ENTITY_REF;
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
    _FORCE_INLINE_ static bool has_at_least_1_sub_bit_field(TypeFlags p_flags) {
        return (p_flags & AT_LEAST_1_SUB_BYTE_FIELD) == AT_LEAST_1_SUB_BYTE_FIELD;
    }
    _FORCE_INLINE_ static bool has_at_least_1_variant_or_ref_field(TypeFlags p_flags) {
        return (p_flags & AT_LEAST_1_VARIANT_OR_FIELD__FIELD) == AT_LEAST_1_VARIANT_OR_FIELD__FIELD;
    }
    void define_field_internal(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_field_type, bool p_has_allowed_ids, PackedInt32Array p_allowed_ids);
    
    constexpr static const uint64_t GEN_MASK_LO = 0xFFFF;
    constexpr static const uint64_t TYPE_INDEX_MASK_LO = 0xFFFF;
    constexpr static const uint64_t TYPE_INDEX_SHIFT = 48;
    constexpr static const uint64_t GEN_SHIFT = 32;
    constexpr static const uint64_t INDEX_MASK_LO = 0xFFFFFFFF;
    
    // these pointer lens = num_types
    Size* lens = nullptr;
    Size* caps = nullptr;
    FieldIndex* field_idx_offset_list = nullptr;
    FieldIndex* num_fields_list = nullptr;
    FieldIndex* next_free_tracking_field_list = nullptr;
    Size* first_free_list = nullptr;
    Size* next_unused_index_list = nullptr;
    TypeFlags* type_flags_list = nullptr;
    // these pointer lens = total_num_fields
    Type* field_type_list = nullptr;
    void** field_data_ptrs = nullptr;
    Type* field_allowed_id_list_index = nullptr;
    Size* field_stride_list = nullptr;
    // this pointer len = total_num_id_fields
    uint64_t* field_allowed_id_list = nullptr;
    FieldIndex total_num_id_fields = 0;
    FieldIndex total_num_fields = 0;
    TypeIndex num_types = 0;
    TypeIndex next_expected_type_define = 0;
    FieldIndex num_allowed_id_blocks_per_idx = 0;
    uint8_t grow_mode = GROW_QUARTER;
    bool enable_entity_refs = false;

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
    _FORCE_INLINE_ void set_at_least_1_sub_bit_field(TypeIndex p_type_index) {
        type_flags_list[p_type_index] |= AT_LEAST_1_SUB_BYTE_FIELD;
    }
    _FORCE_INLINE_ void set_at_least_1_variant_field(TypeIndex p_type_index) {
        type_flags_list[p_type_index] |= AT_LEAST_1_VARIANT_OR_FIELD__FIELD;
    }
    _FORCE_INLINE_ Size field_stride(TypeIndex p_type_idx, FieldIndex p_field_idx) {
        return field_stride_list[p_field_idx + field_idx_offset_list[p_type_idx]];
    }
    _FORCE_INLINE_ Size field_stride_abs(FieldIndex p_field_idx) {
        return field_stride_list[p_field_idx];
    }
    _FORCE_INLINE_ bool is_free_abs(FieldIndex p_free_list_idx, Size p_ent_idx) {
        Size block_idx = p_ent_idx >> 3;
        Size bit_shift = p_ent_idx & 7;
        uint8_t bit = (uint8_t)1 << bit_shift;
        uint8_t* list = ptrcast(uint8_t*, field_data_ptrs[p_free_list_idx]);
        uint8_t block = list[block_idx];
        return (block & bit) == bit;
    }
    _FORCE_INLINE_ void* get_elem_ptr_abs(FieldIndex p_field_idx, Size stride, Size p_ent_idx) {
        uint8_t* list = ptrcast(uint8_t*, field_data_ptrs[p_field_idx]);
        return list + (stride * p_ent_idx);
    }
    _FORCE_INLINE_ void set_free_abs(FieldIndex p_free_list_idx, Size p_ent_idx) {
        Size block_idx = p_ent_idx >> 3;
        Size bit_shift = p_ent_idx & 7;
        uint8_t bit = (uint8_t)1 << bit_shift;
        uint8_t* list = ptrcast(uint8_t*, field_data_ptrs[p_free_list_idx]);
        list[block_idx] |= bit;
    }
    _FORCE_INLINE_ void set_used_abs(FieldIndex p_free_list_idx, Size p_ent_idx) {
        Size block_idx = p_ent_idx >> 3;
        Size bit_shift = p_ent_idx & 7;
        uint8_t bit = (uint8_t)1 << bit_shift;
        bit = ~bit;
        uint8_t* list = ptrcast(uint8_t*, field_data_ptrs[p_free_list_idx]);
        list[block_idx] &= bit;
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

    struct FieldRanges {
        FieldIndex num_fields = 0;
        FieldIndex num_user_fields = 0;
        FieldIndex start = 0;
        FieldIndex end = 0;
        FieldIndex next_free = 0;
        FieldIndex gen = 0;
        FieldIndex free = 0;
        FieldIndex ref = 0;
    };
    struct IdParts {
        TypeIndex type_idx = 0;
        Gen  gen = 0;
        Size index = 0;
    };
    FieldRanges get_field_ranges(TypeIndex p_type_index);
    
    _FORCE_INLINE_ static IdParts get_id_parts(Id p_id) {
        uint64_t u_id = static_cast<uint64_t>(p_id);
        IdParts parts = {};
        parts.type_idx = static_cast<TypeIndex>((u_id >> TYPE_INDEX_SHIFT) & TYPE_INDEX_MASK_LO);
        parts.gen = static_cast<Gen>((u_id >> GEN_SHIFT) & GEN_MASK_LO);
        parts.index = static_cast<Gen>(u_id & INDEX_MASK_LO);
        return parts;
    }
    _FORCE_INLINE_ static Id make_id(TypeIndex type_idx, Gen gen, Size index) {
        uint64_t u_id = static_cast<uint64_t>(index);
        u_id |= static_cast<uint64_t>(gen) << GEN_SHIFT;
        u_id |= static_cast<uint64_t>(type_idx) << TYPE_INDEX_SHIFT;
        return static_cast<Id>(u_id);
    }
    _FORCE_INLINE_ static Id make_id(IdParts parts) {
        return make_id(parts.type_idx, parts.gen, parts.index);
    }
    void free_entity_list_memory(TypeIndex p_type);

public:
    void clear_entity_list(TypeIndex p_type);
	void deinitialize();
	_FORCE_INLINE_ bool entity_list_is_empty(TypeIndex p_type) const { return lens[p_type] == 0; }
	_FORCE_INLINE_ bool entity_list_not_empty(TypeIndex p_type) const { return lens[p_type] != 0; }
	_FORCE_INLINE_ Size get_entity_list_cap(TypeIndex p_type) const { return caps[p_type]; }
	_FORCE_INLINE_ uint32_t get_entity_list_len(TypeIndex p_type) const { return lens[p_type]; }
	void ensure_capacity_for_n_entities(TypeIndex p_type_index, Size p_size);
    Variant get(Id p_id, FieldIndex p_field_index) const;
    Ref<EntityRef> get_entity_ref(Id p_id);
    bool set(Id p_id, FieldIndex p_field_index, Variant p_val);
    Id create(FieldIndex p_type);
    bool destroy(Id p_id);
    bool entity_exists(Id p_id);
    // Init process
    void define_system(TypeIndex p_total_num_types, bool p_enable_entity_refs = true);
    void define_type(TypeIndex p_type_idx, FieldIndex p_num_fields);
    void define_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem);
    void define_entity_id_field(TypeIndex p_type_idx, FieldIndex p_field_idx, PackedInt32Array p_allowed_fields);
    void define_struct_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_struct);
    void define_fixed_length_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_fixed);
    void define_fixed_length_entity_id_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_fixed, PackedInt32Array p_allowed_fields);
    void define_fixed_length_struct_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_struct, Type p_fixed);
    void finalize_entity_system_layout();

    EntityManager();
    ~EntityManager();
};

