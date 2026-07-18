
#include "entity_system.h"
#include "core/error/error_macros.h"
#include "core/os/memory.h"
#include "core/typedefs.h"
#include "core/variant/variant.h"
#include <cstdint>

void EntitySystem::_bind_methods() {
    // bind gdscript visible methods here
    // ClassDB::bind_method(D_METHOD(add, a, b), &EntitySystem::add);
}

EntitySystem::EntitySystem() {
    // Initialize class state when created
}
EntitySystem::~EntitySystem() {
    deinitialize();
}

void EntitySystem::ensure_capacity_for_n_entities(TypeIndex p_type_index, Size p_size) {
    ERR_FAIL_COND_MSG(p_type_index >= num_types, "type index is greater than the total number of types");
    Size cap = caps[p_type_index];
    if (p_size > cap) {
        bool at_least_1_sub_bit_field = at_least_1_sub_bit_field_list[p_type_index];
        FieldIndex num_fields = num_fields_list[p_type_index];
        switch (grow_mode) {
            case GROW_QUARTER:
                cap = MAX((uint32_t)2, cap + ((1 + cap) >> 2));
                if (p_size > cap) {
                    cap = p_size;
                }
                break;
            case GROW_HALF:
                cap = MAX((uint32_t)2, cap + ((1 + cap) >> 1));
                if (p_size > cap) {
                    cap = p_size;
                }
                break;
            case GROW_DOUBLE:
                cap = MAX((uint32_t)2, (cap + 1) << 1);
                if (p_size > cap) {
                    cap = p_size;
                }
                break;
            default:
                cap = p_size;
                break;
        }
        if (at_least_1_sub_bit_field) {
            cap = (cap + 7) & ~7;
        }
        for (FieldIndex i = 0; i < num_fields; i++) {
            void* data = field_data_ptrs[i];
            uint64_t t = field_type_list[i];
            uint32_t s = get_total_size(t);
            uint32_t total = cap * s;
            data = memrealloc(data, total);
            CRASH_COND_MSG(!data, "Out of memory");
        }
    }
}

void EntitySystem::deinitialize() {
    if (init_status == DEINITIALIZED) { return; }
    for (TypeIndex t = 0; t < num_types; t += 1) {
        clear_entity_list(t);
        caps[t] = 0;
    }
    for (FieldIndex f = 0; f < total_num_fields; f++) {
        void* data = field_data_ptrs[f];
        if (data) {
            memfree(data);
            data = nullptr;
        }
    }
    memfree(reinterpret_cast<void*>(field_type_list));
    memfree(reinterpret_cast<void*>(field_data_ptrs));
    memfree(reinterpret_cast<void*>(field_allowed_id_list_index));
    memfree(reinterpret_cast<void*>(stride_list));
    memfree(reinterpret_cast<void*>(next_unused_index_list));
    memfree(reinterpret_cast<void*>(first_free_list));
    memfree(reinterpret_cast<void*>(at_least_1_sub_bit_field_list));
    memfree(reinterpret_cast<void*>(lens));
    memfree(reinterpret_cast<void*>(caps));
    memfree(reinterpret_cast<void*>(field_idx_offset_list));
    memfree(reinterpret_cast<void*>(num_fields_list));
    memfree(reinterpret_cast<void*>(free_tracking_field_list));
    memfree(reinterpret_cast<void*>(gen_tracking_field_list));
    memfree(reinterpret_cast<void*>(field_allowed_id_list));
    init_status = DEINITIALIZED;
}

void EntitySystem::clear_entity_list(TypeIndex p_type) {
    ERR_FAIL_COND_MSG(p_type >= num_types, "type index out of bounds for total number of defined types");
    lens[p_type] = 0;
    first_free_list[p_type] = 0;
    next_unused_index_list[p_type] = 0;
}

void EntitySystem::define_system(TypeIndex p_num_types) {
    ERR_FAIL_COND_MSG(init_status != UNINIT, "`define_system()` must be the FIRST step in the EntitySystem initialization process");
    init_status = SET_TOTAL_TYPES;
    num_types = p_num_types;
    lens = reinterpret_cast<Size*>(memalloc_zeroed(p_num_types * sizeof(Size)));
    caps = reinterpret_cast<Size*>(memalloc_zeroed(p_num_types * sizeof(Size)));
    next_unused_index_list = reinterpret_cast<Size*>(memalloc(p_num_types * sizeof(Size)));
    first_free_list = reinterpret_cast<Size*>(memalloc_zeroed(p_num_types * sizeof(Size)));
    at_least_1_sub_bit_field_list = reinterpret_cast<bool*>(memalloc_zeroed(p_num_types * sizeof(bool)));
    field_idx_offset_list = reinterpret_cast<FieldIndex*>(memalloc_zeroed(p_num_types * sizeof(FieldIndex)));
    num_fields_list = reinterpret_cast<FieldIndex*>(memalloc_zeroed(p_num_types * sizeof(FieldIndex)));
    free_tracking_field_list = reinterpret_cast<FieldIndex*>(memalloc_zeroed(p_num_types * sizeof(FieldIndex)));
    gen_tracking_field_list = reinterpret_cast<FieldIndex*>(memalloc_zeroed(p_num_types * sizeof(FieldIndex)));
    num_allowed_id_blocks_per_idx = ((num_types + 63) & ~63) >> 6;
    for (TypeIndex t = 0; t < num_types; t += 1) {
        next_unused_index_list[t] = 1;
    }
}

void EntitySystem::define_type(TypeIndex p_type, FieldIndex p_num_fields) {
    ERR_FAIL_COND_MSG(init_status != SET_TOTAL_TYPES, "`define_type()` calls must be the SECOND step in the EntitySystem initialization process (after `set_total_types()`)");
    ERR_FAIL_COND_MSG(p_type >= num_types, "type index out of bounds for total number of defined types");
    ERR_FAIL_COND_MSG(num_fields_list[p_type] != 0, "type was already defined");
    ERR_FAIL_COND_MSG(p_num_fields != 0, "cannot define a type with zero fields");
    num_fields_list[p_type] = p_num_fields;
}

void EntitySystem::define_field_internal(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_type, bool p_has_allowed_ids, PackedInt32Array p_allowed_ids) {
    if (init_status == SET_TOTAL_TYPES) {
        FieldIndex f = 0;
        for (TypeIndex i = 0; i < num_types; i += 1) {
            ERR_FAIL_COND_MSG(num_fields_list[i] == 0, "you must call `define_type()` for EVERY type, as defined by `set_total_types()`");
            field_idx_offset_list[i] = f;
            f += num_fields_list[i];
        }
        total_num_fields = f;
        field_type_list = reinterpret_cast<Type*>(memalloc_zeroed(total_num_fields * sizeof(Type)));
        field_allowed_id_list_index = reinterpret_cast<Type*>(memalloc_zeroed(total_num_fields * sizeof(Type)));
        stride_list = reinterpret_cast<Size*>(memalloc_zeroed(total_num_fields * sizeof(Size)));
        field_data_ptrs = reinterpret_cast<void**>(memalloc_zeroed(total_num_fields * sizeof(void*)));
        init_status = DEFINING_ALL_FIELDS;
    }
    ERR_FAIL_COND_MSG(init_status != DEFINING_ALL_FIELDS, "`define_xxxxx_field()` calls must be the THRID step in the EntitySystem initialization process (after `set_total_types()` and all `define_type()` calls)");
    ERR_FAIL_COND_MSG(p_type_idx >= num_types, "type index out of bounds for total number of defined types");
    ERR_FAIL_COND_MSG(p_field_idx >= num_fields_list[p_type_idx], "field index out of bounds for defined number of fields on this type");
    FieldIndex fidx = field_idx_offset_list[p_type_idx] + p_field_idx;
    ERR_FAIL_COND_MSG(field_type_list[fidx] != 0, "field was already defined");
    ERR_FAIL_COND_MSG(p_type == 0, "no type data defined at all (resolves to NONE)");
    if (is_sub_int(p_type)) {
        at_least_1_sub_bit_field_list[p_type_idx] = true;
    }
    field_type_list[fidx] = p_type;
    stride_list[fidx] = get_total_size(p_type);
    if (p_has_allowed_ids) {
        field_allowed_id_list_index[fidx] = total_num_id_fields;
        total_num_id_fields += 1;
        if (total_num_id_fields <= 1) {
            field_allowed_id_list = reinterpret_cast<uint64_t*>(memalloc(num_allowed_id_blocks_per_idx * total_num_id_fields * sizeof(uint64_t)));
        } else {
            field_allowed_id_list = reinterpret_cast<uint64_t*>(memrealloc(reinterpret_cast<void*>(field_allowed_id_list), num_allowed_id_blocks_per_idx * total_num_id_fields * sizeof(uint64_t)));
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
            ERR_FAIL_COND_MSG(a > num_types, "allowed entity id type index is greater than the largest entity type");
            set_allowed_type_id(fidx, a);
        }
    } else {
        field_allowed_id_list_index[fidx] = 0;
    }
}
void EntitySystem::define_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem) {
    ERR_FAIL_COND_MSG(is_entity_id(p_elem), "the `ENTITY_ID` type must be defined with `add_entity_id_field()` or `add_fixed_length_entity_id_array_field()`");
    define_field_internal(p_type_idx, p_field_idx, make_type(p_elem, NONE, 1), false, PackedInt32Array());
}
void EntitySystem::define_entity_id_field(TypeIndex p_type_idx, FieldIndex p_field_idx, PackedInt32Array p_allowed_ids = PackedInt32Array()) {
    define_field_internal(p_type_idx, p_field_idx, make_type(ENTITY_ID, NONE, 1), true, p_allowed_ids);
}
void EntitySystem::define_struct_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_struct) {
    ERR_FAIL_COND_MSG(!is_numeric(p_elem), "only numeric types (INT, FLOAT, U8, I32, I64, etc...) area allowed in `struct` types (VEC_2, VEC_3, COLOR_4, RECT_2, etc...)");
    define_field_internal(p_type_idx, p_field_idx, make_type(p_elem, p_struct, 1), false, PackedInt32Array());
}
void EntitySystem::define_fixed_length_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_fixed) {
    ERR_FAIL_COND_MSG(is_entity_id(p_elem), "the `ENTITY_ID` type must be defined with `add_entity_id_field()` or `add_fixed_length_entity_id_array_field()`");
    define_field_internal(p_type_idx, p_field_idx, make_type(p_elem, NONE, MAX((Type)1, p_fixed)), false, PackedInt32Array());
}
void EntitySystem::define_fixed_length_entity_id_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_fixed, PackedInt32Array p_allowed_ids = PackedInt32Array()) {
    define_field_internal(p_type_idx, p_field_idx, make_type(ENTITY_ID, NONE, MAX((Type)1, p_fixed)), true, p_allowed_ids);
}
void EntitySystem::define_fixed_length_struct_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_struct, Type p_fixed) {
    ERR_FAIL_COND_MSG(!is_numeric(p_elem), "only numeric types (INT, FLOAT, U8, I32, I64, etc...) area allowed in `struct` types (VEC_2, VEC_3, COLOR_4, RECT_2, etc...)");
    define_field_internal(p_type_idx, p_field_idx, make_type(p_elem, p_struct, MAX((Type)1, p_fixed)), false, PackedInt32Array());
}
void EntitySystem::finalize_entity_system_layout() {
    ERR_FAIL_COND_MSG(init_status != DEFINING_ALL_FIELDS, "`finalize_entity_system_layout()` must be the FOURTH and final step in the EntitySystem initialization process (after `set_total_types()`, all `define_type()` calls, and all `define_xxxxx_field()` calls)");
    for (FieldIndex i = 0; i <= total_num_fields; i += 1) {
        ERR_FAIL_COND_MSG(field_type_list[i] == 0, "one of the declared fields was not defined");
    }
    init_status = FINALIZED;
}