
#pragma once

#include "core/object/ref_counted.h"
#include "core/os/memory.h"
#include "core/templates/local_vector.h"
#include "core/typedefs.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include <cstdint>

typedef uint32_t Index;
typedef uint64_t Type;
typedef uint32_t Size;
typedef uint16_t TypeIndex;
typedef int64_t Id;
typedef uint8_t GrowMode;

class EntitySystem : public RefCounted {
    GDCLASS(EntitySystem, RefCounted);
private:
    enum TYPE_LAYOUT {
        ELEM_SHIFT = 0,
        ELEM_MASK = 0b1111,
        STRUCT_SHIFT = ELEM_SHIFT + 4,
        STRUCT_MASK = 0b11111 << STRUCT_SHIFT,
        FIXED_LEN_SHIFT = STRUCT_SHIFT + 5,
        FIXED_LEN_MASK = 0b11111 << FIXED_LEN_SHIFT,
        MISC_SHIFT = FIXED_LEN_SHIFT + 5,
        MISC_MASK = 0b1111 << MISC_SHIFT,
        NUM_ELEM_TYPES = 16,
        NUM_STRUCT_TYPES = 14,
        NUM_MISC_TYPES = 3,
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
    };
    constexpr static const Size STRUCT_SIZES[NUM_STRUCT_TYPES] = {
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
    constexpr static const Size MISC_SIZES[NUM_MISC_TYPES] = {
        0, // NONE
        sizeof(Variant), // VARIANT
        8, // ENTITY_ID
    };
    static const Size VARIANT_SIZE = sizeof(Variant);
    static const Size ENTITY_ID_SIZE = 8;
    _FORCE_INLINE_ static Type get_struct_type(Type p_type) {
        return (p_type & STRUCT_MASK) >> STRUCT_SHIFT;
    }
    _FORCE_INLINE_ static Size get_struct_size(Type p_type) {
        return STRUCT_SIZES[get_struct_type(p_type)];
    }
    _FORCE_INLINE_ static Type get_elem_type(Type p_type) {
        return (p_type & ELEM_MASK) >> ELEM_SHIFT;
    }
    _FORCE_INLINE_ static Size get_elem_size(Type p_type) {
        return ELEM_SIZES[get_elem_type(p_type)];
    }
    _FORCE_INLINE_ static Size get_size_shift(Type p_type) {
        return ELEM_SIZE_SHIFTS[get_elem_type(p_type)];
    }
    _FORCE_INLINE_ static Type get_misc_type(Type p_type) {
        return (p_type & MISC_MASK) >> MISC_SHIFT;
    }
    _FORCE_INLINE_ static Size get_misc_size(Type p_type) {
        return MISC_SIZES[get_misc_type(p_type)];
    }
    _FORCE_INLINE_ static Type get_fixed_size(Type p_type) {
        return MAX((Size)1, (p_type & FIXED_LEN_MASK) >> FIXED_LEN_SHIFT);
    }
    _FORCE_INLINE_ static Size get_total_size(Type p_type) {
        return ((get_elem_size(p_type) * get_struct_size(p_type) * get_fixed_size(p_type)) >> get_size_shift(p_type)) + get_misc_size(p_type);
    }
    _FORCE_INLINE_ static bool is_variant(Type p_type) {
        return (p_type & VARIANT) != 0;
    }
public:
    enum TYPES {
        NONE = 0,
        SCALAR = NONE,
        // ELEMENTS
        BOOL = 1 << ELEM_SHIFT,
        U1 = 2 << ELEM_SHIFT,
        U2 = 3 << ELEM_SHIFT,
        U4 = 4 << ELEM_SHIFT,
        U8 = 5 << ELEM_SHIFT,
        I8 = 6 << ELEM_SHIFT,
        U16 = 7 << ELEM_SHIFT,
        I16 = 8 << ELEM_SHIFT,
        U32 = 9 << ELEM_SHIFT,
        I32 = 10 << ELEM_SHIFT,
        U64 = 11 << ELEM_SHIFT,
        I64 = 12 << ELEM_SHIFT,
        F16 = 13 << ELEM_SHIFT,
        F32 = 14 << ELEM_SHIFT,
        F64 = 15 << ELEM_SHIFT,
        INT = I64,
        FLOAT = F64,
        // Structs
        VEC_2 = 1 << STRUCT_SHIFT,
        VEC_3 = 2 << STRUCT_SHIFT,
        VEC_4 = 3 << STRUCT_SHIFT,
        COLOR_3 = 4 << STRUCT_SHIFT,
        COLOR_4 = 5 << STRUCT_SHIFT,
        RECT_2 = 6 << STRUCT_SHIFT,
        TRANSFORM_2D = 7 << STRUCT_SHIFT,
        PLANE = 8 << STRUCT_SHIFT,
        QUATERNION = 9 << STRUCT_SHIFT,
        AABB = 10 << STRUCT_SHIFT,
        TRANSFORM_3D = 11 << STRUCT_SHIFT,
        PROJECTION = 12 << STRUCT_SHIFT,
        BASIS = 13 << STRUCT_SHIFT,
        RECT_3 = AABB,
        // Other types
        VARIANT = 1 << MISC_SHIFT,
        ENTITY_ID = 2 << MISC_SHIFT,
    };
    enum GROW {
        GROW_EXACT,
        GROW_QUARTER,
        GROW_HALF,
        GROW_DOUBLE,
    };

    Variant get(Id p_id);
    void set(Id p_id, Variant p_val);
    void create(TypeIndex p_type);
    void set_schema(Dictionary p_schema);
    
    // Public vars/methods
    // int add(int a, int b) // example

    EntitySystem();
    ~EntitySystem();

protected:
    static void _bind_methods();
    // Protected vars/methods

private:
    // Private vars/methods
    Id type_index_mask = 0;
    Id type_index_shift = 0;
    Type* type_list = nullptr;
    void** data_ptrs = nullptr;
    Type* allowed_id_list = nullptr;
    Size len = 0;
    Size cap = 0;
    TypeIndex num_types = 0;
    TypeIndex free_tracking_field = 0;
    TypeIndex gen_tracking_field = 0;
    uint8_t grow_mode = GROW_QUARTER;
    bool is_init = false;
    bool at_least_1_ent_id_field = false;
    bool at_least_1_sub_bit_field = false;
    
    enum TYPE_RANGE {
        MIN_BITS_ELEM = BOOL,
        MAX_BITS_ELEM = U4,
        MIN_INT_ELEM = U8,
        MAX_INT_ELEM = I64,
        MIN_FLOAT_ELEM = F16,
        MAX_FLOAT_ELEM = F64,
        MIN_STRUCT = VEC_2,
        MAX_STRUCT = BASIS,
        MIN_FIXED_LEN = 0,
        MAX_FIXED_LEN = 32,
        MIN_MISC = VARIANT,
        MAX_MISC = ENTITY_ID,
    };
public:
    void clear();
	void reset();
    void resize(uint32_t p_size);
	_FORCE_INLINE_ bool is_empty() const { return len == 0; }
	_FORCE_INLINE_ bool not_empty() const { return len != 0; }
	_FORCE_INLINE_ Size get_cap() const { return cap; }
	_FORCE_INLINE_ uint32_t get_len() const { return len; }
	void ensure_capacity(uint32_t p_size);
};

