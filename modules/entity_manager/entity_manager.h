
#pragma once

#include "core/error/error_macros.h"
#include "core/object/ref_counted.h"
#include "core/typedefs.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include <cstdint>

#define ptrcast(m_type, m_mem) reinterpret_cast<m_type>(m_mem)

typedef uint32_t Index;
typedef uint64_t FieldType;
typedef uint32_t Size;
typedef uint16_t FieldIndex;
typedef uint16_t TypeIndex;
typedef uint16_t Gen;
typedef uint8_t TElem;
typedef uint8_t TStruct;
typedef uint8_t TFixed;
typedef uint8_t TShift;
typedef uint8_t GrowMode;
typedef uint8_t TypeFlags;

class EntityManager : public RefCounted {
    GDCLASS(EntityManager, RefCounted);
private:
    class IdData {
    private:
        

        static const constexpr Index FREE_BIT = ((Index)1 << ((sizeof(Index) * 8) - 1));
        static const constexpr Index IDX_MASK = FREE_BIT - 1;
    public:
        Index index_and_free = 0;
        Index next_free = 0;
        TypeIndex type_idx = 0;
        Gen gen = 1;

        _FORCE_INLINE_ bool is_free() {
            return (index_and_free & FREE_BIT) == FREE_BIT;
        }
        _FORCE_INLINE_ bool is_used() {
            return (index_and_free & FREE_BIT) == 0;
        }
        _FORCE_INLINE_ void set_free(EntityManager* manager) {
            DEV_ASSERT(is_used());
            index_and_free = index_and_free | FREE_BIT;
            gen = MAX((Gen)1, gen + 1);
            next_free = manager->type_data[type_idx].first_free;
            manager->type_data[type_idx].first_free = get_idx();
            manager->type_data[type_idx].num_free += 1;
        }
        _FORCE_INLINE_ void set_used(EntityManager* manager) {
            DEV_ASSERT(is_free())
            index_and_free = index_and_free & IDX_MASK;
            manager->type_data[type_idx].first_free = next_free;
            manager->type_data[type_idx].num_free -= 1;
            next_free = 0;
        }
        _FORCE_INLINE_ Index get_idx() {
            return index_and_free & IDX_MASK;
        }
        _FORCE_INLINE_ void set_idx(Index idx) {
            Index free_bit = index_and_free & FREE_BIT;
            index_and_free = idx | free_bit;
        }
    };
    
    class IdParts {
        public:
        Index index = 0;
        Gen gen = 0;

        _FORCE_INLINE_ bool gen_matches(IdData data) {
            return gen == data.gen;
        }
    };

    class Id {
    private:
        static const constexpr int64_t GEN_SHIFT = 32;
        static const constexpr int64_t GEN_MASK_HI = (int64_t)0xFFFF << 32;
        static const constexpr int64_t GEN_MASK_LO = (int64_t)0xFFFF;
        static const constexpr int64_t IDX_MASK = 0xFFFFFFFF;
    public:
        int64_t raw = 0;
        _FORCE_INLINE_ IdParts to_parts() {
            return IdParts{static_cast<Index>(raw & IDX_MASK), static_cast<Gen>((raw >> GEN_SHIFT) & GEN_MASK_LO)};
        }
        _FORCE_INLINE_ static Id from_parts(IdParts parts) {
            int64_t raw = static_cast<int64_t>(parts.index);
            raw |= static_cast<int64_t>(parts.gen) << GEN_SHIFT;
            return Id {raw};
        }
    };

    class TypeData {
    public:
        Index* id_data_idxs = nullptr;
        Index next_unused_index = 0;
        Size num_free = 0;
        Index first_free = 0;
        Size len = 0;
        Size cap = 0;
        FieldIndex num_fields = 0;
        FieldIndex fields_start = 0;
        FieldIndex fields_limit = 0;
        bool at_least_1_variant_field = false;
        bool at_least_1_sub_byte_field = false;
    };

    class FieldData {
    public:
        void* data_ptr = nullptr;
        Size stride = 0;
        Index allowed_id_list_index = 0;
        TElem elem_t = 0;
        TStruct struct_t = 0;
        TFixed fixed_len = 0;
        TShift sub_shift = 0;

        _FORCE_INLINE_ void set_field_type(TElem p_elem_t, TStruct p_struct_t, TFixed p_fixed_len) {
            elem_t = p_elem_t;
            struct_t = p_struct_t;
            fixed_len = MAX((TFixed)1, p_fixed_len);
            stride = static_cast<Size>(ELEM_SIZES[p_elem_t]) * static_cast<Size>(STRUCT_ELEM_COUNTS[p_struct_t]) * static_cast<Size>(fixed_len);
            switch (p_elem_t) {
                case U1: {
                    stride = (stride + 7) & ~7;
                    stride <<= 3;
                }
                case U2: {
                    stride = (stride + 7) & ~7;
                    stride <<= 2;
                }
                case U4: {
                    stride = (stride + 7) & ~7;
                    stride <<= 1;
                }
                default: break;
            }
            sub_shift = ELEM_SUB_SHIFTS[elem_t];
        }

        template <typename T>
        _FORCE_INLINE_ T* get_elem_ptr_cast(Index index) {
            return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(data_ptr) + (index * stride));
        }
        _FORCE_INLINE_ void* get_elem_ptr_opaque(Index index) {
            return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(data_ptr) + (index * stride));
        }

        _FORCE_INLINE_ void realloc(Size new_cap) {
            uint32_t total = new_cap * stride;
            data_ptr = memrealloc(data_ptr, total);
            CRASH_COND_MSG(!data_ptr, "Out of memory");
        }
    };
    class IdAndIdData {
    public:
        Id id = {};
        IdData data = {};
    };
    class ExistsAndIdData {
    public:
        IdData data = {};
        bool exists = false;
    };
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
        VARIANT,
        ENTITY_ID,
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
        ELEM_MAX = ENTITY_ID,
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
        AT_LEAST_1_VARIANT_FIELD = 1 << 1,
    };
    enum INIT {
        UNINIT,
        SET_TOTAL_TYPES,
        ADDING_ALL_TYPES,
        DEFINING_ALL_FIELDS,
        FINALIZED,
        DEINITIALIZED,
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
        sizeof(int64_t), // ENTITY_ID
    };
    constexpr static const Size ELEM_SUB_SHIFTS[NUM_ELEM_TYPES] = {
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
    constexpr static const Size MAX_STRUCT_ELEM_COUNT = 16;
    static const Size VARIANT_SIZE = sizeof(Variant);
    static const Size ENTITY_ID_SIZE = 8;
    _FORCE_INLINE_ static bool is_bool(TElem p_elem_t) {
        return p_elem_t == BOOL;
    }
    _FORCE_INLINE_ static bool is_int(TElem p_elem_t) {
        return p_elem_t >= U1 && p_elem_t <= I64;
    }
    _FORCE_INLINE_ static bool is_sub_int(TElem p_elem_t) {
        return p_elem_t >= U1 && p_elem_t <= U4;
    }
    _FORCE_INLINE_ static bool is_whole_int(TElem p_elem_t) {
        return p_elem_t >= U8 && p_elem_t <= I64;
    }
    _FORCE_INLINE_ static bool is_float(TElem p_elem_t) {
        return p_elem_t >= F16 && p_elem_t <= F64;
    }
    _FORCE_INLINE_ static bool elem_is_numeric(TElem p_elem_t) {
        return p_elem_t >= U1 && p_elem_t <= F64;
    }
    
    IdData* id_data = nullptr;
    TypeData* type_data = nullptr;
    FieldData* field_data = nullptr;
    FieldType* field_allowed_id_list = nullptr;
    Size num_ids = 0;
    Size cap_ids = 0;
    FieldIndex total_num_id_fields = 0;
    FieldIndex total_num_fields = 0;
    TypeIndex num_types = 0;
    FieldIndex num_allowed_id_blocks_per_idx = 0;
    uint8_t id_grow_mode = GROW_QUARTER;
    uint8_t elem_grow_mode = GROW_QUARTER;
    uint8_t init_status = UNINIT;

    _FORCE_INLINE_ void set_allowed_type_id(FieldIndex p_field_idx, int64_t p_allowed);
    _FORCE_INLINE_ bool type_id_is_allowed_in_field(FieldIndex p_field_idx, TypeIndex p_type_idx);
    _FORCE_INLINE_ bool invalid_id_data(IdData p_id_data);
    IdData claim_first_free(TypeIndex p_type_idx, TypeData p_field_ranges);
    IdData claim_next_unused(TypeIndex p_type_idx, TypeData p_field_ranges);
    void destroy_entity_list_memory(TypeIndex p_type);
    void destroy_internal(Index p_id_index, IdData p_id_data, TypeData p_type_data);
    IdAndIdData create_internal(TypeIndex p_type_idx, TypeData p_type_data);
    ExistsAndIdData entity_exists_internal(IdParts p_id_parts, bool ignore_gen = false) const;
    Variant get_internal(IdData p_id_parts, TypeData p_type_data,FieldIndex p_field_index, FieldData p_field_data);
    bool set_internal(IdData p_id_parts, TypeData p_type_data, FieldIndex p_field_index, FieldData p_field_data, Variant val);
    void ensure_capacity_for_n_entities_internal(TypeData* p_type_data, Size p_count);
    void ensure_capacity_for_n_ids_internal(Size p_count);
    void define_field_internal(TypeIndex p_type_idx, FieldIndex p_field_idx, TElem p_elem_t, TStruct p_struct_t, TFixed p_fixed_len, bool p_has_allowed_ids, PackedInt32Array p_allowed_ids);
public:
    void clear_entity_list(TypeIndex p_type);
	void destroy_entity_manager();
	_FORCE_INLINE_ bool entity_list_is_empty(TypeIndex p_type) const { return type_data[p_type].len == 0; }
	_FORCE_INLINE_ bool entity_list_not_empty(TypeIndex p_type) const { return type_data[p_type].len != 0; }
	_FORCE_INLINE_ Size get_entity_list_cap(TypeIndex p_type) const { return type_data[p_type].cap; }
	_FORCE_INLINE_ uint32_t get_entity_list_len(TypeIndex p_type) const { return type_data[p_type].len; }
	void ensure_capacity_for_n_entities(TypeIndex p_type_index, Size p_count);
    Variant get(Id p_id, FieldIndex p_field_index) const;
    Variant get_gdscript(int64_t p_id_gdscript, FieldIndex p_field_index) const;
    bool set(Id p_id, FieldIndex p_field_index, Variant p_val);
    bool set_gdscript(int64_t p_id_gdscript, FieldIndex p_field_index, Variant p_val);
    Id create(TypeIndex p_type);
    int64_t create_gdscript(TypeIndex p_type);
    bool destroy(Id p_id);
    bool destroy_gdscript(int64_t p_id_gdscript);
    bool entity_exists(Id p_id) const;
    bool entity_exists_gdscript(int64_t p_id_gdscript) const;
    // Init process
    void define_system(TypeIndex p_total_num_types);
    void define_type(TypeIndex p_type_idx, FieldIndex p_num_fields);
    void define_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TElem p_elem);
    void define_entity_id_field(TypeIndex p_type_idx, FieldIndex p_field_idx, PackedInt32Array p_allowed_fields);
    void define_struct_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TElem p_elem, TStruct p_struct);
    void define_fixed_length_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TElem p_elem, TFixed p_fixed);
    void define_fixed_length_entity_id_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TFixed p_fixed, PackedInt32Array p_allowed_fields);
    void define_fixed_length_struct_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TElem p_elem, TStruct p_struct, TFixed p_fixed);
    void finalize_entity_system_layout();

    EntityManager();
    ~EntityManager();
};

