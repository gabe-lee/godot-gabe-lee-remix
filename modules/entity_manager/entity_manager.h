
#pragma once

#include "core/error/error_macros.h"
#include "core/object/ref_counted.h"
#include "core/typedefs.h"
#include "core/variant/dictionary.h"
#include "core/variant/type_info.h"
#include "core/variant/variant.h"
#include "modules/serial/serial.h"
#include <cstdint>

#define ptrcast(m_type, m_mem) reinterpret_cast<m_type>(m_mem)

typedef uint32_t Index;
typedef uint64_t FieldType;
typedef uint32_t Size;
typedef uint16_t FieldIndex;
typedef uint16_t TypeIndex;
typedef uint16_t Gen;
typedef uint8_t TFixed;
typedef uint8_t TShift;
typedef uint8_t GrowMode;
typedef uint8_t TypeFlags;

class EntityManager : public RefCounted {
    GDCLASS(EntityManager, RefCounted);
private:
    using RW = ReaderWriter;
    using TGodot = RW::GODOT_TYPE;
    using TSerial = RW::SERIAL_TYPE;
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
        Size full_stride = 0;
        Size struct_stride = 0;
        Index allowed_id_list_index = 0;
        TSerial elem_t = (TSerial)0;
        TGodot struct_t = (TGodot)0;
        TFixed struct_elem_count = 0;
        TFixed fixed_len = 0;

        _FORCE_INLINE_ void set_field_type(TSerial p_serial_t, TGodot p_godot_t, TFixed p_fixed_len) {
            elem_t = p_serial_t;
            struct_t = p_godot_t;
            struct_elem_count = RW::GODOT_ELEM_COUNT[p_godot_t];
            fixed_len = MAX((TFixed)1, p_fixed_len);
            struct_stride = RW::godot_type_size(p_godot_t);
            full_stride = struct_stride * static_cast<Size>(fixed_len);
        }

        template <typename T>
        _FORCE_INLINE_ T* get_elem_ptr_cast(Index index) {
            return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(data_ptr) + (index * full_stride));
        }
        _FORCE_INLINE_ void* get_elem_ptr_opaque(Index index) {
            return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(data_ptr) + (index * full_stride));
        }
        _FORCE_INLINE_ uint8_t* get_elem_base_ptr_u8() {
            return reinterpret_cast<uint8_t*>(data_ptr);
        }

        _FORCE_INLINE_ void realloc(Size new_cap) {
            uint32_t total = new_cap * full_stride;
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
    enum SERIAL_TYPE {
        BOOL = RW::SERIAL_TYPE::BOOL,
        U8 = RW::SERIAL_TYPE::U8,
        I8 = RW::SERIAL_TYPE::I8,
        U16 = RW::SERIAL_TYPE::U16,
        I16 = RW::SERIAL_TYPE::I16,
        U32 = RW::SERIAL_TYPE::U32,
        I32 = RW::SERIAL_TYPE::I32,
        I64 = RW::SERIAL_TYPE::I64,
        U64 = RW::SERIAL_TYPE::U64,
        F16 = RW::SERIAL_TYPE::F16,
        F32 = RW::SERIAL_TYPE::F32,
        F64 = RW::SERIAL_TYPE::F64,
        DEFAULT = RW::SERIAL_TYPE::DEFAULT,
        _SERIAL_TYPE_LIMIT,
        _SERIAL_TYPE_MIN = BOOL,
        _SERIAL_TYPE_MAX = DEFAULT,
        _SERIAL_TYPE_NUM = _SERIAL_TYPE_LIMIT - _SERIAL_TYPE_MIN,
        _SERIAL_TYPE_BITS = RW::SERIAL_TYPE::_SERIAL_TYPE_BITS,
        _SERIAL_TYPE_SHIFT = RW::SERIAL_TYPE::_SERIAL_TYPE_SHIFT,
        _SERIAL_TYPE_MASK_LO = RW::SERIAL_TYPE::_SERIAL_TYPE_MASK,
        _SERIAL_TYPE_MASK_HI = _SERIAL_TYPE_MASK_LO << _SERIAL_TYPE_SHIFT,
        REAL = RW::SERIAL_TYPE::REAL,
    };
    enum GODOT_TYPE {
        BOOLEAN = RW::GODOT_TYPE::BOOLEAN,
        INTEGER = RW::GODOT_TYPE::INTEGER,
        FLOAT = RW::GODOT_TYPE::FLOAT,
        VEC_2 = RW::GODOT_TYPE::VEC_2,
        VEC_2I = RW::GODOT_TYPE::VEC_2I,
        VEC_3 = RW::GODOT_TYPE::VEC_3,
        VEC_3I = RW::GODOT_TYPE::VEC_3I,
        VEC_4 = RW::GODOT_TYPE::VEC_4,
        VEC_4I = RW::GODOT_TYPE::VEC_4I,
        RECT_2 = RW::GODOT_TYPE::RECT_2,
        RECT_2I = RW::GODOT_TYPE::RECT_2I,
        COLOR = RW::GODOT_TYPE::COLOR,
        AABB = RW::GODOT_TYPE::AABB,
        PLANE = RW::GODOT_TYPE::PLANE,
        BASIS = RW::GODOT_TYPE::BASIS,
        TRANSFORM_2D = RW::GODOT_TYPE::TRANSFORM_2D,
        TRANSFORM_3D = RW::GODOT_TYPE::TRANSFORM_3D,
        QUATERNION = RW::GODOT_TYPE::QUATERNION,
        PROJECTION = RW::GODOT_TYPE::PROJECTION,
        ENTITY_ID = RW::GODOT_TYPE::_GD_TYPE_CUSTOM,
        _GD_TYPE_LIMIT,
        _GD_TYPE_MIN = BOOLEAN,
        _GD_TYPE_MAX = PROJECTION,
        _GD_TYPE_NUM = _GD_TYPE_LIMIT - _GD_TYPE_MIN,
        _GD_TYPE_BITS = RW::GODOT_TYPE::_GD_TYPE_BITS,
        _GD_TYPE_SHIFT = RW::GODOT_TYPE::_GD_TYPE_SHIFT,
        _GD_TYPE_MASK_LO = RW::GODOT_TYPE::_GD_TYPE_MASK,
        _GD_TYPE_MASK_HI = _GD_TYPE_MASK_LO << _GD_TYPE_SHIFT,
        RECT_3 = RW::GODOT_TYPE::RECT_3,
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
        FIXED_MAX = 63,
        FIXED_MAX_BIASED = FIXED_MAX + 1,
        FIXED_BITS = 6,
        FIXED_MASK_LO = (1 << FIXED_BITS) - 1,
        FIXED_SHIFT = (unsigned int)_SERIAL_TYPE_BITS + (unsigned int)_GD_TYPE_BITS,
        FIXED_MASK_HI = FIXED_MASK_LO << FIXED_SHIFT,
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
    constexpr static const Size MAX_STRUCT_ELEM_COUNT = 16;
    static const Size VARIANT_SIZE = sizeof(Variant);
    static const Size ENTITY_ID_SIZE = 8;
    // _FORCE_INLINE_ static bool is_bool(TSerial p_serial_t) {
    //     return p_serial_t == BOOL;
    // }
    // _FORCE_INLINE_ static bool is_int(TSerial p_serial_t) {
    //     return p_serial_t >= U8 && p_serial_t <= I64;
    // }
    // _FORCE_INLINE_ static bool is_float(TSerial p_serial_t) {
    //     return p_serial_t >= F16 && p_serial_t <= F64;
    // }
    // _FORCE_INLINE_ static bool elem_is_numeric(TSerial p_serial_t) {
    //     return p_serial_t >= U8 && p_serial_t <= F64;
    // }
    
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

    _FORCE_INLINE_ void set_allowed_type_id(FieldIndex p_allowed_block_offset, TypeIndex p_type_idx);
    _FORCE_INLINE_ bool type_id_is_allowed_in_field(FieldIndex p_allowed_block_offset, TypeIndex p_type_idx);
    _FORCE_INLINE_ bool invalid_id_data(IdData p_id_data);
    IdData claim_first_free(TypeIndex p_type_idx, TypeData p_field_ranges);
    IdData claim_next_unused(TypeIndex p_type_idx, TypeData p_field_ranges);
    void destroy_entity_list_memory(TypeIndex p_type);
    void destroy_internal(Index p_id_index, IdData p_id_data, TypeData p_type_data);
    IdAndIdData create_internal(TypeIndex p_type_idx, TypeData p_type_data);
    ExistsAndIdData entity_exists_internal(IdParts p_id_parts, bool ignore_gen = false) const;
    Variant get_internal(IdData p_id_parts, TypeData p_type_data,FieldIndex p_field_index, FieldData p_field_data) const;
    Variant get_internal_from_array(IdData p_id_parts, TypeData p_type_data,FieldIndex p_field_index, FieldData p_field_data, Index p_sub_idx) const;
    bool set_internal(IdData p_id_parts, TypeData p_type_data, FieldIndex p_field_index, FieldData p_field_data, Variant val);
    bool set_internal_in_array(IdData p_id_parts, TypeData p_type_data, FieldIndex p_field_index, FieldData p_field_data, Index p_sub_idx, Variant val);
    void ensure_capacity_for_n_entities_internal(TypeData* p_type_data, Size p_count);
    void ensure_capacity_for_n_ids_internal(Size p_count);
    void define_field_internal(TypeIndex p_type_idx, FieldIndex p_field_idx, TSerial p_serial_t, TGodot p_godot_t, TFixed p_fixed_len, bool p_has_allowed_ids, PackedInt32Array p_allowed_ids);
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
    Variant get_one_from_array(Id p_id, FieldIndex p_field_index, Index p_sub_idx) const;
    Variant get_one_from_array_gdscript(int64_t p_id_gdscript, FieldIndex p_field_index, Index p_sub_idx) const;
    bool set(Id p_id, FieldIndex p_field_index, Variant p_val);
    bool set_gdscript(int64_t p_id_gdscript, FieldIndex p_field_index, Variant p_val);
    bool set_one_in_array(Id p_id, FieldIndex p_field_index, Index p_sub_idx, Variant p_val);
    bool set_one_in_array_gdscript(int64_t p_id_gdscript, FieldIndex p_field_index, Index p_sub_idx, Variant p_val);
    Id create(TypeIndex p_type);
    int64_t create_gdscript(TypeIndex p_type);
    bool destroy(Id p_id);
    bool destroy_gdscript(int64_t p_id_gdscript);
    bool entity_exists(Id p_id) const;
    bool entity_exists_gdscript(int64_t p_id_gdscript) const;
    // Init process
    void define_manager(TypeIndex p_total_num_types);
    void define_type(TypeIndex p_type_idx, FieldIndex p_num_fields);
    void define_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TSerial p_elem);
    void define_entity_id_field(TypeIndex p_type_idx, FieldIndex p_field_idx, PackedInt32Array p_allowed_fields);
    void define_struct_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TGodot p_struct, TSerial p_elem);
    void define_fixed_length_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TSerial p_elem, TFixed p_fixed);
    void define_fixed_length_entity_id_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TFixed p_fixed, PackedInt32Array p_allowed_fields);
    void define_fixed_length_struct_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TGodot p_struct, TSerial p_elem, TFixed p_fixed);
    void finalize_entity_manager_layout();

    EntityManager();
    ~EntityManager();
};

VARIANT_ENUM_CAST(EntityManager::SERIAL_TYPE);
VARIANT_ENUM_CAST(EntityManager::GODOT_TYPE);
VARIANT_ENUM_CAST(EntityManager::GROW);