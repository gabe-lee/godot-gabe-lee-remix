
#include "entity_manager.h"
#include "core/error/error_macros.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/color.h"
#include "core/math/math_defs.h"
#include "core/math/plane.h"
#include "core/math/projection.h"
#include "core/math/quaternion.h"
#include "core/math/rect2.h"
#include "core/math/rect2i.h"
#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "core/object/class_db.h"
#include "core/os/memory.h"
#include "core/typedefs.h"
#include "core/variant/array.h"
#include "core/variant/variant.h"
#include <cstdint>
#include <cstring>
#include "utils.hpp"

void EntityManager::_bind_methods() {
    
    BIND_ENUM_CONSTANT(BOOL);
    BIND_ENUM_CONSTANT(U8);
    BIND_ENUM_CONSTANT(I8);
    BIND_ENUM_CONSTANT(U16);
    BIND_ENUM_CONSTANT(I16);
    BIND_ENUM_CONSTANT(U32);
    BIND_ENUM_CONSTANT(I32);
    BIND_ENUM_CONSTANT(U64);
    BIND_ENUM_CONSTANT(I64);
    BIND_ENUM_CONSTANT(F16);
    BIND_ENUM_CONSTANT(F32);
    BIND_ENUM_CONSTANT(F64);
    BIND_ENUM_CONSTANT(DEFAULT);

    BIND_ENUM_CONSTANT(BOOLEAN);
    BIND_ENUM_CONSTANT(FLOAT);
    BIND_ENUM_CONSTANT(INTEGER);
    BIND_ENUM_CONSTANT(VEC_2I);
    BIND_ENUM_CONSTANT(VEC_2);
    BIND_ENUM_CONSTANT(VEC_3);
    BIND_ENUM_CONSTANT(VEC_3I);
    BIND_ENUM_CONSTANT(VEC_4);
    BIND_ENUM_CONSTANT(VEC_4I);
    BIND_ENUM_CONSTANT(COLOR);
    BIND_ENUM_CONSTANT(RECT_2);
    BIND_ENUM_CONSTANT(RECT_2I);
    BIND_ENUM_CONSTANT(TRANSFORM_2D);
    BIND_ENUM_CONSTANT(PLANE);
    BIND_ENUM_CONSTANT(QUATERNION);
    BIND_ENUM_CONSTANT(AABB);
    BIND_ENUM_CONSTANT(TRANSFORM_3D);
    BIND_ENUM_CONSTANT(PROJECTION);
    BIND_ENUM_CONSTANT(BASIS);
    BIND_ENUM_CONSTANT(RECT_3);
    BIND_ENUM_CONSTANT(ENTITY_ID);

    BIND_ENUM_CONSTANT(GROW_EXACT);
    BIND_ENUM_CONSTANT(GROW_QUARTER);
    BIND_ENUM_CONSTANT(GROW_HALF);
    BIND_ENUM_CONSTANT(GROW_DOUBLE);

    ClassDB::bind_method(D_METHOD("clear_entity_list", "entity_type"), &EntityManager::clear_entity_list);
    ClassDB::bind_method(D_METHOD("destroy_entity_manager"), &EntityManager::destroy_entity_manager);
    ClassDB::bind_method(D_METHOD("entity_list_is_empty", "entity_type"), &EntityManager::entity_list_is_empty);
    ClassDB::bind_method(D_METHOD("entity_list_not_empty", "entity_type"), &EntityManager::entity_list_not_empty);
    ClassDB::bind_method(D_METHOD("get_entity_list_cap", "entity_type"), &EntityManager::get_entity_list_cap);
    ClassDB::bind_method(D_METHOD("get_entity_list_len", "entity_type"), &EntityManager::get_entity_list_len);
    ClassDB::bind_method(D_METHOD("ensure_capacity_for_n_entities", "entity_type", "count"), &EntityManager::ensure_capacity_for_n_entities);
    ClassDB::bind_method(D_METHOD("get", "entity_id", "entity_field"), &EntityManager::get_gdscript);
    ClassDB::bind_method(D_METHOD("get_one_from_fixed_array", "entity_id", "entity_field", "array_idx"), &EntityManager::get_one_from_array_gdscript);
    ClassDB::bind_method(D_METHOD("set", "entity_id", "entity_field", "value"), &EntityManager::set_gdscript);
    ClassDB::bind_method(D_METHOD("set_one_in_fixed_array", "entity_id", "entity_field", "array_idx", "value"), &EntityManager::set_one_in_array_gdscript);
    ClassDB::bind_method(D_METHOD("create_entity", "entity_type"), &EntityManager::create_gdscript);
    ClassDB::bind_method(D_METHOD("destroy_entity", "entity_id"), &EntityManager::destroy_gdscript);
    ClassDB::bind_method(D_METHOD("entity_exists", "entity_id"), &EntityManager::entity_exists_gdscript);
    ClassDB::bind_method(D_METHOD("define_manager", "total_num_entity_types"), &EntityManager::define_manager);
    ClassDB::bind_method(D_METHOD("define_type", "entity_type", "num_fields_for_entity_type"), &EntityManager::define_type);
    ClassDB::bind_method(D_METHOD("define_field", "entity_type", "field_index", "element_type"), &EntityManager::define_field);
    ClassDB::bind_method(D_METHOD("define_entity_id_field", "entity_type", "field_index", "allowed_entity_types_for_entity_ids"), &EntityManager::define_entity_id_field);
    ClassDB::bind_method(D_METHOD("define_struct_field", "entity_type", "field_index", "struct_type", "element_type"), &EntityManager::define_struct_field);
    ClassDB::bind_method(D_METHOD("define_fixed_length_array_field", "entity_type", "field_index", "element_type", "fixed_length"), &EntityManager::define_fixed_length_array_field);
    ClassDB::bind_method(D_METHOD("define_fixed_length_entity_id_array_field", "entity_type", "field_index", "fixed_length", "allowed_entity_types_for_entity_ids"), &EntityManager::define_fixed_length_entity_id_array_field);
    ClassDB::bind_method(D_METHOD("define_fixed_length_struct_array_field", "entity_type", "field_index", "struct_type", "element_type", "fixed_length"), &EntityManager::define_fixed_length_struct_array_field);
    ClassDB::bind_method(D_METHOD("finalize_entity_manager_layout"), &EntityManager::finalize_entity_manager_layout);
}

EntityManager::EntityManager() {
    // Initialize class state when created
}
EntityManager::~EntityManager() {
    destroy_entity_manager();
}

void EntityManager::ensure_capacity_for_n_entities_internal(TypeData* p_type_data, Size p_count) {
    TypeData td = *p_type_data;
    if (p_count > td.cap) {
        switch (elem_grow_mode) {
            case GROW_QUARTER:
                td.cap = MAX(p_count, td.cap + ((1 + td.cap) >> 2));
                break;
            case GROW_HALF:
                td.cap = MAX(p_count, td.cap + ((1 + td.cap) >> 1));
                break;
            case GROW_DOUBLE:
                td.cap = MAX(p_count, (td.cap + 1) << 1);
                break;
            default:
                td.cap = p_count;
                break;
        }
        td.cap = (td.cap + 7) & ~7;
        for (FieldIndex f = td.fields_start; f < td.fields_limit; f++) {
            FieldData fd = field_data[f];
            fd.realloc(td.cap);
            field_data[f] = fd;
        }
        *p_type_data = td;
    }
}
void EntityManager::ensure_capacity_for_n_entities(TypeIndex p_type_idx, Size p_count) {
    ERR_FAIL_COND_MSG(p_type_idx >= num_types, "type index out of bounds for total number of defined types");
    TypeData* p_type_data = &type_data[p_type_idx];
    ensure_capacity_for_n_entities_internal(p_type_data, p_count);
}
void EntityManager::ensure_capacity_for_n_ids_internal(Size p_count) {
    if (p_count > cap_ids) {
        switch (id_grow_mode) {
            case GROW_QUARTER:
                cap_ids = MAX(p_count, cap_ids + ((1 + cap_ids) >> 2));
                break;
            case GROW_HALF:
                cap_ids = MAX(p_count, cap_ids + ((1 + cap_ids) >> 1));
                break;
            case GROW_DOUBLE:
                cap_ids = MAX(p_count, (cap_ids + 1) << 1);
                break;
            default:
                cap_ids = p_count;
                break;
        }
        id_data = memrealloc_t(IdData*, id_data, cap_ids);
    }
}

void EntityManager::destroy_entity_manager() {
    if (init_status == DEINITIALIZED) { return; }
    for (TypeIndex t = 0; t < num_types; t++) {
        destroy_entity_list_memory(t);
        TypeData p_type_data = type_data[t];
        if (p_type_data.id_data_idxs) {
            memfree_t(p_type_data.id_data_idxs);
            p_type_data.id_data_idxs = nullptr;
        }
    }
    for (FieldIndex f = 0; f < total_num_fields; f++) {
        FieldData p_field_data = field_data[f];
        if (p_field_data.data_ptr) {
            memfree(p_field_data.data_ptr);
            p_field_data.data_ptr = nullptr;
        }
    }
    memfree_t(id_data);
    memfree_t(type_data);
    memfree_t(field_data);
    memfree_t(field_allowed_id_list);
    init_status = DEINITIALIZED;
}


void EntityManager::destroy_entity_list_memory(TypeIndex p_type_idx) {
    DEV_ASSERT_MSG(p_type_idx >= num_types, "type index out of bounds for total number of defined types");
    TypeData p_type_data = type_data[p_type_idx];
    Size idx_limit = p_type_data.next_unused_index;
    if (type_data->at_least_1_variant_field) {
        for (Size i = 0; i < idx_limit; i += 1) {
            Index p_id_data_idx = p_type_data.id_data_idxs[i];
            IdData p_id_data = id_data[p_id_data_idx];
            if (p_id_data.is_used()) {
                destroy_internal(p_id_data_idx, p_id_data, p_type_data);
            }
        }
    }
    p_type_data.len = 0;
    p_type_data.cap = 0;
    p_type_data.next_unused_index = 1;
    p_type_data.first_free = 0;
    type_data[p_type_idx] = p_type_data;
}

void EntityManager::clear_entity_list(TypeIndex p_type_idx) {
    ERR_FAIL_COND_MSG(p_type_idx >= num_types, "type index out of bounds for total number of defined types");
    TypeData p_type_data = type_data[p_type_idx];
    Size idx_limit = p_type_data.next_unused_index;
    for (Size i = 0; i < idx_limit; i += 1) {
        Index p_id_data_idx = p_type_data.id_data_idxs[i];
        IdData p_id_data = id_data[p_id_data_idx];
        if (p_id_data.is_used()) {
            destroy_internal(p_id_data_idx, p_id_data, p_type_data);
        }
    }
    p_type_data.len = 0;
    p_type_data.next_unused_index = 1;
    p_type_data.first_free = 0;
    type_data[p_type_idx] = p_type_data;
}

EntityManager::ExistsAndIdData EntityManager::entity_exists_internal(IdParts p_id_parts, bool ignore_gen) const {
    if (p_id_parts.index >= num_ids) {return ExistsAndIdData{};}
    IdData p_id_data = id_data[p_id_parts.index];
    if (p_id_data.is_free()) {return ExistsAndIdData{};}
    if (!ignore_gen) {
        if (p_id_data.gen != p_id_parts.gen) {return ExistsAndIdData{};}
    }
    return ExistsAndIdData{p_id_data, true};
}

bool EntityManager::entity_exists(Id p_id) const {
    ERR_FAIL_COND_V_MSG(init_status != FINALIZED, false, "Cannot access entities before the entity manager is finalized.");
    IdParts parts = p_id.to_parts();
    return entity_exists_internal(parts, false).exists;
}

bool EntityManager::entity_exists_gdscript(int64_t p_id) const {
    return entity_exists(Id{p_id});
}
//CHECKPOINT
void EntityManager::destroy_internal(Index p_id_index, IdData p_id_data, TypeData p_type_data) {
    Index p_serial_type_index = p_id_data.get_idx();
    if (p_type_data.at_least_1_variant_field) {
        for (FieldIndex f = p_type_data.fields_start; f < p_type_data.fields_limit; f += 1) {
            FieldData p_field_data = field_data[f];
            if (p_field_data.elem_t == VARIANT) {
                Variant* var_ptr = p_field_data.get_elem_ptr_cast<Variant>(p_serial_type_index);
                var_ptr->~Variant();
                memset((void*)var_ptr, 0, sizeof(Variant));
            }
        }
    }
    p_id_data.set_free(this);
    id_data[p_id_index] = p_id_data;
}

bool EntityManager::destroy(Id p_id) {
    ERR_FAIL_COND_V_MSG(init_status != FINALIZED, false, "Cannot access entities before the entity manager is finalized.");
    IdParts parts = p_id.to_parts();
    ExistsAndIdData id_if_exists = entity_exists_internal(parts, false);
    if (!id_if_exists.exists) { return false; }
    TypeData p_type_data = type_data[parts.index];
    destroy_internal(parts.index, id_if_exists.data, p_type_data);
    return true;
}

bool EntityManager::destroy_gdscript(int64_t p_id) {
    return destroy(Id{p_id});
}

EntityManager::IdAndIdData EntityManager::create_internal(TypeIndex p_type_idx, TypeData p_type_data) {
    IdData p_id_data = {};
    Id p_id = {};
    if (p_type_data.num_free > 0 || p_type_data.first_free > 0) {
        DEV_ASSERT_MSG(p_type_data.num_free > 0 && p_type_data.first_free > 0, "`num_free` state did not match `first_free` state");
        Index id_data_idx = p_type_data.id_data_idxs[p_type_data.first_free];
        p_id_data = id_data[id_data_idx];
        DEV_ASSERT_MSG(p_id_data.type_idx == p_type_idx, "type_idx on IdData does not match p_type_idx passed to function");
        p_id = Id::from_parts(IdParts{id_data_idx, p_id_data.gen});
        p_id_data.set_used(this);
        id_data[id_data_idx] = p_id_data;
    } else {
        p_id = Id::from_parts(IdParts{num_ids, 1});
        ensure_capacity_for_n_ids_internal(num_ids + 1);
        p_id_data = IdData{0, 0, p_type_idx, 1};
        p_id_data.set_idx(num_ids);
        id_data[num_ids] = p_id_data;
        num_ids += 1;
        if (p_type_data.next_unused_index >= p_type_data.cap) {
            ensure_capacity_for_n_entities_internal(&p_type_data, p_type_data.next_unused_index + 1);
        }
    }
    return IdAndIdData{p_id, p_id_data};
}

EntityManager::Id EntityManager::create(TypeIndex p_type_idx) {
    ERR_FAIL_COND_V_MSG(init_status != FINALIZED, Id{}, "Cannot access entities before the entity manager is finalized.");
    ERR_FAIL_COND_V_MSG(p_type_idx >= num_types, Id{}, "type index is greater then the total number of types");
    TypeData p_type_data = type_data[p_type_idx];
    return create_internal(p_type_idx, p_type_data).id;
}

int64_t EntityManager::create_gdscript(TypeIndex p_type_idx) {
    return create(p_type_idx).raw;
}

Variant EntityManager::get_internal(IdData p_id_data, TypeData p_type_data, FieldIndex p_field_index, FieldData p_field_data) const {
    Variant out;
    Index p_idx = p_id_data.get_idx();
    if (p_field_data.fixed_len > 1) {
        if (p_field_data.struct_t == NONE) { // array of vals
            switch (p_field_data.elem_t) {
                case BOOL: [[fallthrough]];
                read_array_of_vals_direct_packed_case(U8, PackedByteArray, uint8_t)
                read_array_of_vals_packed_case(I8, PackedInt32Array, int8_t, false, int32_t, false)
                read_array_of_vals_packed_case(U16, PackedInt32Array, uint16_t, false, int32_t, false)
                read_array_of_vals_packed_case(I16, PackedInt32Array, int16_t, false, int32_t, false)
                read_array_of_vals_packed_case(U32, PackedInt64Array, uint32_t, false, int64_t, false)
                read_array_of_vals_direct_packed_case(I32, PackedInt32Array, int32_t)
                case ENTITY_ID: [[fallthrough]];
                case U64: [[fallthrough]]; // Variants can never hold integers greater than INT64_MAX, even if using U64 mode
                read_array_of_vals_direct_packed_case(I64, PackedInt64Array, int64_t)
                read_array_of_vals_packed_case(F16, PackedFloat32Array, uint16_t, true, float, false)
                read_array_of_vals_direct_packed_case(F32, PackedFloat32Array, float)
                read_array_of_vals_direct_packed_case(F64, PackedFloat64Array, double)
                case VARIANT: {
                    Array arr;
                    arr.reserve(p_field_data.fixed_len);
                    Variant* src_ptr = p_field_data.get_elem_ptr_cast<Variant>(p_id_data.get_idx());
                    for (int i = 0; i < p_field_data.fixed_len; i += 1, src_ptr += 1) {
                        arr.append(*src_ptr);
                    }
                    out = Variant(arr);
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "Invalid `elem_t` type in entity manager");
            }
        } else { // array of structs
            switch (p_field_data.struct_t) {
                read_array_of_structs_all_elem_cases(VEC_2, STRUCT_ELEM_COUNTS[VEC_2], Array, Vector2i, int32_t, PackedVector2Array, Vector2, real_t)
                read_array_of_structs_all_elem_cases(VEC_3, STRUCT_ELEM_COUNTS[VEC_3], Array, Vector3i, int32_t, PackedVector3Array, Vector3, real_t)
                read_array_of_structs_all_elem_cases(VEC_4, STRUCT_ELEM_COUNTS[VEC_4], Array, Vector4i, int32_t, PackedVector4Array, Vector4, real_t)
                read_array_of_structs_all_elem_cases(COLOR_3, STRUCT_ELEM_COUNTS[COLOR_3], PackedColorArray, Color, float, PackedColorArray, Color, float)
                read_array_of_structs_all_elem_cases(COLOR_4, STRUCT_ELEM_COUNTS[COLOR_4], PackedColorArray, Color, float, PackedColorArray, Color, float)
                read_array_of_structs_all_elem_cases(RECT_2, STRUCT_ELEM_COUNTS[RECT_2], Array, Rect2i, int32_t, Array, Rect2, real_t)
                read_array_of_structs_all_elem_cases(TRANSFORM_2D, STRUCT_ELEM_COUNTS[TRANSFORM_2D], Array, Transform2D, real_t, Array, Transform2D, real_t)
                read_array_of_structs_all_elem_cases(PLANE, STRUCT_ELEM_COUNTS[PLANE], Array, Plane, real_t, Array, Plane, real_t)
                read_array_of_structs_all_elem_cases(QUATERNION, STRUCT_ELEM_COUNTS[QUATERNION], Array, Quaternion, real_t, Array, Quaternion, real_t)
                read_array_of_structs_all_elem_cases(AABB, STRUCT_ELEM_COUNTS[AABB], Array, ::AABB, real_t, Array, ::AABB, real_t)
                read_array_of_structs_all_elem_cases(TRANSFORM_3D, STRUCT_ELEM_COUNTS[TRANSFORM_3D], Array, Transform3D, real_t, Array, Transform3D, real_t)
                read_array_of_structs_all_elem_cases(PROJECTION, STRUCT_ELEM_COUNTS[PROJECTION], Array, Projection, real_t, Array, Projection, real_t)
                read_array_of_structs_all_elem_cases(BASIS, STRUCT_ELEM_COUNTS[BASIS], Array, Basis, real_t, Array, Basis, real_t)
                default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid struct type in entity manager");
            }
        }
    } else {
        if (p_field_data.struct_t == NONE) { // single val
            switch (p_field_data.elem_t) {
                case BOOL: [[fallthrough]];
                read_single_val_case(U8, uint8_t, int64_t);
                read_single_val_case(I8, int8_t, int64_t);
                read_single_val_case(U16, uint16_t, int64_t);
                read_single_val_case(I16, int16_t, int64_t);
                read_single_val_case(U32, uint32_t, int64_t);
                read_single_val_case(I32, int32_t, int64_t);
                case ENTITY_ID: [[fallthrough]];
                case U64: [[fallthrough]];
                read_single_val_case(I64, int64_t, int64_t);
                case F16: {
                    out = Variant(static_cast<double>(half_u16_to_float(*p_field_data.get_elem_ptr_cast<uint16_t>(p_idx))));
                    break; 
                }
                read_single_val_case(F32, float, double);
                read_single_val_case(F64, double, double);
                case VARIANT: {
                    out = *p_field_data.get_elem_ptr_cast<Variant>(p_idx);
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "Invalid `elem_t` type in entity manager");
            }
        } else { // single struct
            switch (p_field_data.struct_t) {
                read_single_struct_all_elem_cases(VEC_2, STRUCT_ELEM_COUNTS[VEC_2], Vector2i, int32_t, Vector2, real_t)
                read_single_struct_all_elem_cases(VEC_3, STRUCT_ELEM_COUNTS[VEC_3], Vector3i, int32_t, Vector3, real_t)
                read_single_struct_all_elem_cases(VEC_4, STRUCT_ELEM_COUNTS[VEC_4], Vector4i, int32_t, Vector4, real_t)
                read_single_struct_all_elem_cases(COLOR_3, STRUCT_ELEM_COUNTS[COLOR_3], Color, real_t, Color, real_t)
                read_single_struct_all_elem_cases(COLOR_4, STRUCT_ELEM_COUNTS[COLOR_4], Color, real_t, Color, real_t)
                read_single_struct_all_elem_cases(RECT_2, STRUCT_ELEM_COUNTS[RECT_2], Rect2i, int32_t, Rect2, real_t)
                read_single_struct_all_elem_cases(TRANSFORM_2D, STRUCT_ELEM_COUNTS[TRANSFORM_2D], Transform2D, real_t, Transform2D, real_t)
                read_single_struct_all_elem_cases(PLANE, STRUCT_ELEM_COUNTS[PLANE], Plane, real_t, Plane, real_t)
                read_single_struct_all_elem_cases(QUATERNION, STRUCT_ELEM_COUNTS[QUATERNION], Quaternion, real_t, Quaternion, real_t)
                read_single_struct_all_elem_cases(AABB, STRUCT_ELEM_COUNTS[AABB], ::AABB, real_t, ::AABB, real_t)
                read_single_struct_all_elem_cases(TRANSFORM_3D, STRUCT_ELEM_COUNTS[TRANSFORM_3D], Transform3D, real_t, Transform3D, real_t)
                read_single_struct_all_elem_cases(PROJECTION, STRUCT_ELEM_COUNTS[PROJECTION], Projection, real_t, Projection, real_t)
                read_single_struct_all_elem_cases(BASIS, STRUCT_ELEM_COUNTS[BASIS], Basis, real_t, Basis, real_t)
                default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid struct type in entity manager");
            }
        }
    }
    return out;
}

Variant EntityManager::get(Id p_id, FieldIndex p_field_index) const {
    IdParts parts = p_id.to_parts();
    ExistsAndIdData p_id_data_if_exists = entity_exists_internal(parts);
    if (!p_id_data_if_exists.exists) { return Variant();}
    IdData p_id_data = p_id_data_if_exists.data;
    DEV_ASSERT_MSG(p_id_data.type_idx < num_types, "internal error: entity system IdData entry had a type_idx beyond maximum");
    TypeData p_type_data = type_data[p_id_data.type_idx];
    ERR_FAIL_COND_V_MSG(p_field_index >= p_type_data.num_fields, Variant(), "invalid field for id, must be one of the fields originally defined on the type during initialization");
    FieldData p_field_data = field_data[p_field_index + p_type_data.fields_start];
    return get_internal(p_id_data, p_type_data, p_field_index, p_field_data);
}

Variant EntityManager::get_gdscript(int64_t p_id_gdscript, FieldIndex p_field_index) const {
    return get(Id{p_id_gdscript}, p_field_index);
}


Variant EntityManager::get_internal_from_array(IdData p_id_data, TypeData p_type_data, FieldIndex p_field_index, FieldData p_field_data, Index p_sub_idx) const {
    Variant out;
    Index p_idx = p_id_data.get_idx();
    ERR_FAIL_COND_V_MSG(p_field_data.fixed_len <= 1, Variant(), "field is not a `fixed-len-array` type, cannot get a single value indexed from a `single-value` type");
    if (p_field_data.struct_t == NONE) { // array of vals
        switch (p_field_data.elem_t) {
            case BOOL: [[fallthrough]];
            read_single_from_array_of_vals_case(U8, uint8_t, false, int64_t, false)
            read_single_from_array_of_vals_case(I8, int8_t, false, int64_t, false)
            read_single_from_array_of_vals_case(U16, uint16_t, false, int64_t, false)
            read_single_from_array_of_vals_case(I16, int16_t, false, int64_t, false)
            read_single_from_array_of_vals_case(U32, uint32_t, false, int64_t, false)
            read_single_from_array_of_vals_case(I32, int32_t, false, int64_t, false)
            case ENTITY_ID: [[fallthrough]];
            case U64: [[fallthrough]]; // Variants can never hold integers greater than INT64_MAX, even if using U64 mode
            read_single_from_array_of_vals_case(I64, int64_t, false, int64_t, false)
            read_single_from_array_of_vals_case(F16, uint16_t, true, double, false)
            read_single_from_array_of_vals_case(F32, float, false, double, false)
            read_single_from_array_of_vals_case(F64, double, false, double, false)
            case VARIANT: {
                Variant* src_ptr = p_field_data.get_elem_ptr_cast<Variant>(p_idx);
                src_ptr += p_sub_idx;
                out = *src_ptr;
                break;
            }
            default: ERR_FAIL_V_MSG(Variant(), "Invalid `elem_t` type in entity manager");
        }
    } else { // array of structs
        switch (p_field_data.struct_t) {
            read_single_from_array_of_structs_all_elem_cases(VEC_2, STRUCT_ELEM_COUNTS[VEC_2], Vector2i, int32_t, Vector2, real_t)
            read_single_from_array_of_structs_all_elem_cases(VEC_3, STRUCT_ELEM_COUNTS[VEC_3], Vector3i, int32_t, Vector3, real_t)
            read_single_from_array_of_structs_all_elem_cases(VEC_4, STRUCT_ELEM_COUNTS[VEC_4], Vector4i, int32_t, Vector4, real_t)
            read_single_from_array_of_structs_all_elem_cases(COLOR_3, STRUCT_ELEM_COUNTS[COLOR_3], Color, float, Color, float)
            read_single_from_array_of_structs_all_elem_cases(COLOR_4, STRUCT_ELEM_COUNTS[COLOR_4], Color, float, Color, float)
            read_single_from_array_of_structs_all_elem_cases(RECT_2, STRUCT_ELEM_COUNTS[RECT_2], Rect2i, int32_t, Rect2, real_t)
            read_single_from_array_of_structs_all_elem_cases(TRANSFORM_2D, STRUCT_ELEM_COUNTS[TRANSFORM_2D], Transform2D, real_t, Transform2D, real_t)
            read_single_from_array_of_structs_all_elem_cases(PLANE, STRUCT_ELEM_COUNTS[PLANE], Plane, real_t, Plane, real_t)
            read_single_from_array_of_structs_all_elem_cases(QUATERNION, STRUCT_ELEM_COUNTS[QUATERNION], Quaternion, real_t, Quaternion, real_t)
            read_single_from_array_of_structs_all_elem_cases(AABB, STRUCT_ELEM_COUNTS[AABB], ::AABB, real_t, ::AABB, real_t)
            read_single_from_array_of_structs_all_elem_cases(TRANSFORM_3D, STRUCT_ELEM_COUNTS[TRANSFORM_3D], Transform3D, real_t, Transform3D, real_t)
            read_single_from_array_of_structs_all_elem_cases(PROJECTION, STRUCT_ELEM_COUNTS[PROJECTION], Projection, real_t, Projection, real_t)
            read_single_from_array_of_structs_all_elem_cases(BASIS, STRUCT_ELEM_COUNTS[BASIS], Basis, real_t, Basis, real_t)
            default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid struct type in entity manager");
        }
    }
    return out;
}

Variant EntityManager::get_one_from_array(Id p_id, FieldIndex p_field_index, Index p_sub_idx) const {
    IdParts parts = p_id.to_parts();
    ExistsAndIdData p_id_data_if_exists = entity_exists_internal(parts);
    if (!p_id_data_if_exists.exists) { return Variant();}
    IdData p_id_data = p_id_data_if_exists.data;
    DEV_ASSERT_MSG(p_id_data.type_idx < num_types, "internal error: entity system IdData entry had a type_idx beyond maximum");
    TypeData p_type_data = type_data[p_id_data.type_idx];
    ERR_FAIL_COND_V_MSG(p_field_index >= p_type_data.num_fields, Variant(), "invalid field for id, must be one of the fields originally defined on the type during initialization");
    FieldData p_field_data = field_data[p_field_index + p_type_data.fields_start];
    ERR_FAIL_COND_V_MSG(p_sub_idx >= p_field_data.fixed_len, Variant(), "array index is larger than the defined array fixed length for this field during initialization");
    return get_internal_from_array(p_id_data, p_type_data, p_field_index, p_field_data, p_sub_idx);
}

Variant EntityManager::get_one_from_array_gdscript(int64_t p_id_gdscript, FieldIndex p_field_index, Index p_sub_idx) const {
    return get_one_from_array(Id{p_id_gdscript}, p_field_index, p_sub_idx);
}

bool EntityManager::set_internal(IdData p_id_data, TypeData p_type_data, FieldIndex p_field_index, FieldData p_field_data, Variant value) {
    Index p_idx = p_id_data.get_idx();
    if (p_field_data.fixed_len > 1) {
        if (p_field_data.struct_t == NONE) { // array of vals
            switch (p_field_data.elem_t) {
                case BOOL: [[fallthrough]];
                write_array_of_vals_direct_packed_case(U8, PackedByteArray, uint8_t)
                write_array_of_vals_packed_case(I8, PackedInt32Array, int32_t, false, int8_t, false)
                write_array_of_vals_packed_case(U16, PackedInt32Array, int32_t, false, uint16_t, false)
                write_array_of_vals_packed_case(I16, PackedInt32Array, int32_t, false, int16_t, false)
                write_array_of_vals_packed_case(U32, PackedInt64Array, int64_t, false, uint32_t, false)
                write_array_of_vals_direct_packed_case(I32, PackedInt32Array, int32_t)
                case ENTITY_ID: [[fallthrough]];
                case U64: [[fallthrough]]; // Variants can never hold integers greater than INT64_MAX, even if using U64 mode
                write_array_of_vals_direct_packed_case(I64, PackedInt64Array, int64_t)
                write_array_of_vals_packed_case(F16, PackedFloat32Array, float, false, uint16_t, true)
                write_array_of_vals_direct_packed_case(F32, PackedFloat32Array, float)
                write_array_of_vals_direct_packed_case(F64, PackedFloat64Array, double)
                case VARIANT: {
                    Array arr = (Array)value;
                    Size size = arr.size(); \
                    ERR_FAIL_COND_V_MSG(size > p_field_data.fixed_len, false, "provided array was larger than the defined fixed-len length for this field"); \
                    Variant* dst_ptr = p_field_data.get_elem_ptr_cast<Variant>(p_id_data.get_idx());
                    for (Size i = 0; i < size; i += 1, dst_ptr += 1) {
                        *dst_ptr = arr[i];
                    }
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "Invalid `elem_t` type in entity manager");
            }
        } else { // array of structs
            switch (p_field_data.struct_t) {
                write_array_of_structs_all_elem_cases(VEC_2, STRUCT_ELEM_COUNTS[VEC_2], Array, Vector2i, int32_t, PackedVector2Array, Vector2, real_t)
                write_array_of_structs_all_elem_cases(VEC_3, STRUCT_ELEM_COUNTS[VEC_3], Array, Vector3i, int32_t, PackedVector3Array, Vector3, real_t)
                write_array_of_structs_all_elem_cases(VEC_4, STRUCT_ELEM_COUNTS[VEC_4], Array, Vector4i, int32_t, PackedVector4Array, Vector4, real_t)
                write_array_of_structs_all_elem_cases(COLOR_3, STRUCT_ELEM_COUNTS[COLOR_3], PackedColorArray, Color, float, PackedColorArray, Color, float)
                write_array_of_structs_all_elem_cases(COLOR_4, STRUCT_ELEM_COUNTS[COLOR_4], PackedColorArray, Color, float, PackedColorArray, Color, float)
                write_array_of_structs_all_elem_cases(RECT_2, STRUCT_ELEM_COUNTS[RECT_2], Array, Rect2i, int32_t, Array, Rect2, real_t)
                write_array_of_structs_all_elem_cases(TRANSFORM_2D, STRUCT_ELEM_COUNTS[TRANSFORM_2D], Array, Transform2D, real_t, Array, Transform2D, real_t)
                write_array_of_structs_all_elem_cases(PLANE, STRUCT_ELEM_COUNTS[PLANE], Array, Plane, real_t, Array, Plane, real_t)
                write_array_of_structs_all_elem_cases(QUATERNION, STRUCT_ELEM_COUNTS[QUATERNION], Array, Quaternion, real_t, Array, Quaternion, real_t)
                write_array_of_structs_all_elem_cases(AABB, STRUCT_ELEM_COUNTS[AABB], Array, ::AABB, real_t, Array, ::AABB, real_t)
                write_array_of_structs_all_elem_cases(TRANSFORM_3D, STRUCT_ELEM_COUNTS[TRANSFORM_3D], Array, Transform3D, real_t, Array, Transform3D, real_t)
                write_array_of_structs_all_elem_cases(PROJECTION, STRUCT_ELEM_COUNTS[PROJECTION], Array, Projection, real_t, Array, Projection, real_t)
                write_array_of_structs_all_elem_cases(BASIS, STRUCT_ELEM_COUNTS[BASIS], Array, Basis, real_t, Array, Basis, real_t)
                default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid struct type in entity manager");
            }
        }
    } else {
        if (p_field_data.struct_t == NONE) { // single val
            switch (p_field_data.elem_t) {
                case BOOL: [[fallthrough]];
                write_single_val_case(U8, uint8_t, int64_t);
                write_single_val_case(I8, int8_t, int64_t);
                write_single_val_case(U16, uint16_t, int64_t);
                write_single_val_case(I16, int16_t, int64_t);
                write_single_val_case(U32, uint32_t, int64_t);
                write_single_val_case(I32, int32_t, int64_t);
                case ENTITY_ID: [[fallthrough]];
                case U64: [[fallthrough]];
                write_single_val_case(I64, int64_t, int64_t);
                case F16: {
                    *p_field_data.get_elem_ptr_cast<uint16_t>(p_idx) = float_to_half_u16((double)value);
                    break; 
                }
                write_single_val_case(F32, float, double);
                write_single_val_case(F64, double, double);
                case VARIANT: {
                    *p_field_data.get_elem_ptr_cast<Variant>(p_idx) = value;
                    break;
                }
                default: ERR_FAIL_V_MSG(Variant(), "Invalid `elem_t` type in entity manager");
            }
        } else { // single struct
            switch (p_field_data.struct_t) {
                write_single_struct_all_elem_cases(VEC_2, STRUCT_ELEM_COUNTS[VEC_2], Vector2i, int32_t, Vector2, real_t)
                write_single_struct_all_elem_cases(VEC_3, STRUCT_ELEM_COUNTS[VEC_3], Vector3i, int32_t, Vector3, real_t)
                write_single_struct_all_elem_cases(VEC_4, STRUCT_ELEM_COUNTS[VEC_4], Vector4i, int32_t, Vector4, real_t)
                write_single_struct_all_elem_cases(COLOR_3, STRUCT_ELEM_COUNTS[COLOR_3], Color, real_t, Color, real_t)
                write_single_struct_all_elem_cases(COLOR_4, STRUCT_ELEM_COUNTS[COLOR_4], Color, real_t, Color, real_t)
                write_single_struct_all_elem_cases(RECT_2, STRUCT_ELEM_COUNTS[RECT_2], Rect2i, int32_t, Rect2, real_t)
                write_single_struct_all_elem_cases(TRANSFORM_2D, STRUCT_ELEM_COUNTS[TRANSFORM_2D], Transform2D, real_t, Transform2D, real_t)
                write_single_struct_all_elem_cases(PLANE, STRUCT_ELEM_COUNTS[PLANE], Plane, real_t, Plane, real_t)
                write_single_struct_all_elem_cases(QUATERNION, STRUCT_ELEM_COUNTS[QUATERNION], Quaternion, real_t, Quaternion, real_t)
                write_single_struct_all_elem_cases(AABB, STRUCT_ELEM_COUNTS[AABB], ::AABB, real_t, ::AABB, real_t)
                write_single_struct_all_elem_cases(TRANSFORM_3D, STRUCT_ELEM_COUNTS[TRANSFORM_3D], Transform3D, real_t, Transform3D, real_t)
                write_single_struct_all_elem_cases(PROJECTION, STRUCT_ELEM_COUNTS[PROJECTION], Projection, real_t, Projection, real_t)
                write_single_struct_all_elem_cases(BASIS, STRUCT_ELEM_COUNTS[BASIS], Basis, real_t, Basis, real_t)
                default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid struct type in entity manager");
            }
        }
    }
    return false;
}

bool EntityManager::set(Id p_id, FieldIndex p_field_index, Variant value) {
    IdParts parts = p_id.to_parts();
    ExistsAndIdData p_id_data_if_exists = entity_exists_internal(parts);
    if (!p_id_data_if_exists.exists) { return Variant();}
    IdData p_id_data = p_id_data_if_exists.data;
    DEV_ASSERT_MSG(p_id_data.type_idx < num_types, "internal error: entity system IdData entry had a type_idx beyond maximum");
    TypeData p_type_data = type_data[p_id_data.type_idx];
    ERR_FAIL_COND_V_MSG(p_field_index >= p_type_data.num_fields, Variant(), "invalid field for id, must be one of the fields originally defined on the type during initialization");
    FieldData p_field_data = field_data[p_field_index + p_type_data.fields_start];
    return set_internal(p_id_data, p_type_data, p_field_index, p_field_data, value);
}

bool EntityManager::set_gdscript(int64_t p_id_gdscript, FieldIndex p_field_index, Variant value) {
    return set(Id{p_id_gdscript}, p_field_index, value);
}

bool EntityManager::set_internal_in_array(IdData p_id_data, TypeData p_type_data, FieldIndex p_field_index, FieldData p_field_data, Index p_sub_idx, Variant value) {
    Index p_idx = p_id_data.get_idx();
    ERR_FAIL_COND_V_MSG(p_field_data.fixed_len <= 1, false, "field is not a `fixed-len-array` type, cannot get a single value indexed from a `single-value` type");
    if (p_field_data.struct_t == NONE) { // array of vals
        switch (p_field_data.elem_t) {
            case BOOL: [[fallthrough]];
            write_single_from_array_of_vals_case(U8, uint8_t, false, int64_t, false)
            write_single_from_array_of_vals_case(I8, int8_t, false, int64_t, false)
            write_single_from_array_of_vals_case(U16, uint16_t, false, int64_t, false)
            write_single_from_array_of_vals_case(I16, int16_t, false, int64_t, false)
            write_single_from_array_of_vals_case(U32, uint32_t, false, int64_t, false)
            write_single_from_array_of_vals_case(I32, int32_t, false, int64_t, false)
            case ENTITY_ID: [[fallthrough]];
            case U64: [[fallthrough]]; // Variants can never hold integers greater than INT64_MAX, even if using U64 mode
            write_single_from_array_of_vals_case(I64, int64_t, false, int64_t, false)
            write_single_from_array_of_vals_case(F16, uint16_t, true, double, false)
            write_single_from_array_of_vals_case(F32, float, false, double, false)
            write_single_from_array_of_vals_case(F64, double, false, double, false)
            case VARIANT: {
                Variant* dst_ptr = p_field_data.get_elem_ptr_cast<Variant>(p_idx);
                dst_ptr += p_sub_idx;
                *dst_ptr = value;
                break;
            }
            default: ERR_FAIL_V_MSG(Variant(), "Invalid `elem_t` type in entity manager");
        }
    } else { // array of structs
        switch (p_field_data.struct_t) {
            write_single_from_array_of_structs_all_elem_cases(VEC_2, STRUCT_ELEM_COUNTS[VEC_2], Vector2i, int32_t, Vector2, real_t)
            write_single_from_array_of_structs_all_elem_cases(VEC_3, STRUCT_ELEM_COUNTS[VEC_3], Vector3i, int32_t, Vector3, real_t)
            write_single_from_array_of_structs_all_elem_cases(VEC_4, STRUCT_ELEM_COUNTS[VEC_4], Vector4i, int32_t, Vector4, real_t)
            write_single_from_array_of_structs_all_elem_cases(COLOR_3, STRUCT_ELEM_COUNTS[COLOR_3], Color, float, Color, float)
            write_single_from_array_of_structs_all_elem_cases(COLOR_4, STRUCT_ELEM_COUNTS[COLOR_4], Color, float, Color, float)
            write_single_from_array_of_structs_all_elem_cases(RECT_2, STRUCT_ELEM_COUNTS[RECT_2], Rect2i, int32_t, Rect2, real_t)
            write_single_from_array_of_structs_all_elem_cases(TRANSFORM_2D, STRUCT_ELEM_COUNTS[TRANSFORM_2D], Transform2D, real_t, Transform2D, real_t)
            write_single_from_array_of_structs_all_elem_cases(PLANE, STRUCT_ELEM_COUNTS[PLANE], Plane, real_t, Plane, real_t)
            write_single_from_array_of_structs_all_elem_cases(QUATERNION, STRUCT_ELEM_COUNTS[QUATERNION], Quaternion, real_t, Quaternion, real_t)
            write_single_from_array_of_structs_all_elem_cases(AABB, STRUCT_ELEM_COUNTS[AABB], ::AABB, real_t, ::AABB, real_t)
            write_single_from_array_of_structs_all_elem_cases(TRANSFORM_3D, STRUCT_ELEM_COUNTS[TRANSFORM_3D], Transform3D, real_t, Transform3D, real_t)
            write_single_from_array_of_structs_all_elem_cases(PROJECTION, STRUCT_ELEM_COUNTS[PROJECTION], Projection, real_t, Projection, real_t)
            write_single_from_array_of_structs_all_elem_cases(BASIS, STRUCT_ELEM_COUNTS[BASIS], Basis, real_t, Basis, real_t)
            default: ERR_FAIL_V_MSG(Variant(), "internal error: invalid struct type in entity manager");
        }
    }
    return false;
}

bool EntityManager::set_one_in_array(Id p_id, FieldIndex p_field_index, Index p_sub_idx, Variant value) {
    IdParts parts = p_id.to_parts();
    ExistsAndIdData p_id_data_if_exists = entity_exists_internal(parts);
    if (!p_id_data_if_exists.exists) { return Variant();}
    IdData p_id_data = p_id_data_if_exists.data;
    DEV_ASSERT_MSG(p_id_data.type_idx < num_types, "internal error: entity system IdData entry had a type_idx beyond maximum");
    TypeData p_type_data = type_data[p_id_data.type_idx];
    ERR_FAIL_COND_V_MSG(p_field_index >= p_type_data.num_fields, Variant(), "invalid field for id, must be one of the fields originally defined on the type during initialization");
    FieldData p_field_data = field_data[p_field_index + p_type_data.fields_start];
    ERR_FAIL_COND_V_MSG(p_sub_idx >= p_field_data.fixed_len, Variant(), "array index is larger than the defined array fixed length for this field during initialization");
    return set_internal_in_array(p_id_data, p_type_data, p_field_index, p_field_data, p_sub_idx, value);
}

bool EntityManager::set_one_in_array_gdscript(int64_t p_id_gdscript, FieldIndex p_field_index, Index p_sub_idx, Variant value) {
    return set_one_in_array(Id{p_id_gdscript}, p_field_index, p_sub_idx, value);
}

void EntityManager::define_manager(TypeIndex p_num_types) {
    ERR_FAIL_COND_MSG(init_status != UNINIT, "`define_system()` must be the FIRST step in the EntitySystem initialization process");
    init_status = SET_TOTAL_TYPES;
    num_types = p_num_types;
    type_data = memalloc_zeroed_t(TypeData*, num_types);
    num_allowed_id_blocks_per_idx = ((num_types + 63) & ~63) >> 6;
    for (TypeIndex t = 0; t < num_types; t += 1) {
        type_data[t].next_unused_index = 1;
    }
}

void EntityManager::define_type(TypeIndex p_type_idx, FieldIndex p_num_fields) {
    ERR_FAIL_COND_MSG(init_status != SET_TOTAL_TYPES, "`define_type()` calls must be the SECOND step in the EntitySystem initialization process (after `set_total_types()`)");
    ERR_FAIL_COND_MSG(p_type_idx >= num_types, "type index out of bounds for total number of defined types");
    ERR_FAIL_COND_MSG(type_data[p_type_idx].num_fields != 0, "type was already defined");
    ERR_FAIL_COND_MSG(p_num_fields != 0, "cannot define a type with zero fields");
    type_data[p_type_idx].num_fields = p_num_fields;
}

void EntityManager::define_field_internal(TypeIndex p_type_idx, FieldIndex p_field_idx, TSerial p_serial_t, TGodot p_godot_t, TFixed p_fixed_len, bool p_has_allowed_ids, PackedInt32Array p_allowed_ids) {
    if (init_status == SET_TOTAL_TYPES) {
        FieldIndex f = 0;
        for (TypeIndex t = 0; t < num_types; t += 1) {
            ERR_FAIL_COND_MSG(type_data[t].num_fields == 0, "you must call `define_type()` for EVERY type, as defined by `set_total_types()`, before you call `define_xxxxx_field()`");
            type_data[t].fields_start = f;
            f += type_data[t].num_fields;
            type_data[t].fields_limit = f;
        }
        total_num_fields = f;
        field_data = memalloc_zeroed_t(FieldData*, total_num_fields);
        init_status = DEFINING_ALL_FIELDS;
    }
    ERR_FAIL_COND_MSG(init_status != DEFINING_ALL_FIELDS, "`define_xxxxx_field()` calls must be the THRID step in the EntitySystem initialization process (after `set_total_types()` and all `define_type()` calls)");
    ERR_FAIL_COND_MSG(p_type_idx >= num_types, "type index out of bounds for total number of defined types");
    TypeData t_data = type_data[p_type_idx];
    ERR_FAIL_COND_MSG(p_field_idx >= t_data.num_fields, "field index out of bounds for defined number of fields on this type");
    FieldIndex fidx = t_data.fields_start + p_field_idx;
    FieldData p_field_data = field_data[fidx];
    ERR_FAIL_COND_MSG(p_field_data.elem_t != 0, "field was already defined");
    ERR_FAIL_COND_MSG(p_serial_t == 0, "no type data defined at all (resolves to NONE)");
    if (p_has_allowed_ids) {
        p_field_data.allowed_id_list_index = total_num_id_fields;
        total_num_id_fields += 1;
        if (total_num_id_fields <= 1) {
            field_allowed_id_list = memalloc_t(uint64_t*, num_allowed_id_blocks_per_idx * total_num_id_fields * sizeof(uint64_t));
        } else {
            field_allowed_id_list = memrealloc_t(uint64_t*, field_allowed_id_list, num_allowed_id_blocks_per_idx * total_num_id_fields * sizeof(uint64_t));
        }
        int64_t num_allowed = p_allowed_ids.size();
        for (int64_t a = 0; a < num_allowed_id_blocks_per_idx; a += 1) {
            if (num_allowed == 0) {
                field_allowed_id_list[((total_num_id_fields - 1) * num_allowed_id_blocks_per_idx) + a] = UINT64_MAX;
            } else {
                field_allowed_id_list[((total_num_id_fields - 1) * num_allowed_id_blocks_per_idx) + a] = 0;
            }
        }
        for (int64_t a = 0; a < num_allowed; a += 1) {
            TypeIndex p_allowed_type_idx = p_allowed_ids[a];
            ERR_FAIL_COND_MSG(p_allowed_type_idx > num_types, "allowed entity id type index is greater than the largest entity type");
            set_allowed_type_id(p_field_data.allowed_id_list_index, p_allowed_type_idx);
        }
    } else {
        p_field_data.allowed_id_list_index = 0;
    }
    p_field_data.set_field_type(p_serial_t, p_godot_t, p_fixed_len);
    field_data[fidx] = p_field_data;
}
void EntityManager::define_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TSerial p_serial_type) {
    ERR_FAIL_COND_MSG(p_serial_type == ENTITY_ID, "the `ENTITY_ID` type must be defined with `add_entity_id_field()` or `add_fixed_length_entity_id_array_field()`");
    define_field_internal(p_type_idx, p_field_idx, p_serial_type, NONE, 1, false, PackedInt32Array());
}
void EntityManager::define_entity_id_field(TypeIndex p_type_idx, FieldIndex p_field_idx, PackedInt32Array p_allowed_ids = PackedInt32Array()) {
    define_field_internal(p_type_idx, p_field_idx, ENTITY_ID, NONE, 1, true, p_allowed_ids);
}
void EntityManager::define_struct_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TGodot p_struct, TSerial p_serial_type) {
    ERR_FAIL_COND_MSG(!elem_is_numeric(p_serial_type), "only numeric types (INT, FLOAT, U8, I32, I64, etc...) area allowed in `struct` types (VEC_2, VEC_3, COLOR_4, RECT_2, etc...)");
    define_field_internal(p_type_idx, p_field_idx, p_serial_type, p_struct, 1, false, PackedInt32Array());
}
void EntityManager::define_fixed_length_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TSerial p_serial_type, TFixed p_fixed) {
    ERR_FAIL_COND_MSG(p_serial_type == ENTITY_ID, "the `ENTITY_ID` type must be defined with `add_entity_id_field()` or `add_fixed_length_entity_id_array_field()`");
    define_field_internal(p_type_idx, p_field_idx, p_serial_type, NONE, MAX((FieldType)1, p_fixed), false, PackedInt32Array());
}
void EntityManager::define_fixed_length_entity_id_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TFixed p_fixed, PackedInt32Array p_allowed_ids = PackedInt32Array()) {
    define_field_internal(p_type_idx, p_field_idx, ENTITY_ID, NONE, MAX((FieldType)1, p_fixed), true, p_allowed_ids);
}
void EntityManager::define_fixed_length_struct_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, TGodot p_struct, TSerial p_serial_type, TFixed p_fixed) {
    ERR_FAIL_COND_MSG(!elem_is_numeric(p_serial_type), "only numeric types (INT, FLOAT, U8, I32, I64, etc...) area allowed in `struct` types (VEC_2, VEC_3, COLOR_4, RECT_2, etc...)");
    define_field_internal(p_type_idx, p_field_idx, p_serial_type, p_struct, MAX((FieldType)1, p_fixed), false, PackedInt32Array());
}
void EntityManager::finalize_entity_manager_layout() {
    ERR_FAIL_COND_MSG(init_status != DEFINING_ALL_FIELDS, "`finalize_entity_system_layout()` must be the FOURTH and final step in the EntitySystem initialization process (after `set_total_types()`, all `define_type()` calls, and all `define_xxxxx_field()` calls)");
    for (TypeIndex t = 0; t < num_types; t += 1) {
        ERR_FAIL_COND_MSG(type_data[t].num_fields == 0, "you must call `define_type()` for EVERY type, as defined by `set_total_types()`, before you call `define_xxxxx_field()`");
    }
    for (FieldIndex f = 0; f <= total_num_fields; f += 1) {
        ERR_FAIL_COND_MSG(field_data[f].elem_t == 0, "one of the declared fields was not defined");
    }
    init_status = FINALIZED;
}

bool EntityManager::type_id_is_allowed_in_field(FieldIndex p_allowed_block_offset, TypeIndex p_type_idx) {
    uint64_t block = (uint64_t)p_type_idx >> 6;
    uint64_t bit_shift = (uint64_t)p_type_idx & 63;
    uint64_t bit = (uint64_t)1 << bit_shift;
    return (field_allowed_id_list[(p_allowed_block_offset * num_allowed_id_blocks_per_idx) + block] & bit) == bit;
}

void EntityManager::set_allowed_type_id(FieldIndex p_allowed_block_offset, TypeIndex p_type_idx) {
    uint64_t block = (uint64_t)p_type_idx >> 6;
    uint64_t bit_shift = (uint64_t)p_type_idx & 63;
    uint64_t bit = (uint64_t)1 << bit_shift;
    field_allowed_id_list[(p_allowed_block_offset * num_allowed_id_blocks_per_idx) + block] |= bit;
}