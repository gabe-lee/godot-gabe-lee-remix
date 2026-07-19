
#include "entity_manager.h"
#include "core/error/error_macros.h"
#include "core/object/ref_counted.h"
#include "core/os/memory.h"
#include "core/typedefs.h"
#include "core/variant/variant.h"
#include "modules/entity_manager/entity_ref.h"
#include <cstdint>
#include <cstring>

void EntityManager::_bind_methods() {
    // bind gdscript visible methods here
    // ClassDB::bind_method(D_METHOD(add, a, b), &EntitySystem::add);
}

EntityManager::EntityManager() {
    // Initialize class state when created
}
EntityManager::~EntityManager() {
    deinitialize();
}

void EntityManager::ensure_capacity_for_n_entities(TypeIndex p_type_index, Size p_size) {
    ERR_FAIL_COND_MSG(p_type_index >= num_types, "type index is greater than the total number of types");
    Size cap = caps[p_type_index];
    if (p_size > cap) {
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
        cap = (cap + 7) & ~7;
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

void EntityManager::deinitialize() {
    if (init_status == DEINITIALIZED) { return; }
    for (TypeIndex t = 0; t < num_types; t++) {
        free_entity_list_memory(t);
    }
    for (FieldIndex f = 0; f < total_num_fields; f++) {
        void* data = field_data_ptrs[f];
        if (data) {
            memfree(data);
            data = nullptr;
        }
    }
    // Len = total_num_fields
    memfree_t(field_type_list);
    memfree_t(field_data_ptrs);
    memfree_t(field_allowed_id_list_index);
    memfree_t(field_stride_list);
    // Len = num_types
    memfree_t(next_unused_index_list);
    memfree_t(first_free_list);
    memfree_t(type_flags_list);
    memfree_t(lens);
    memfree_t(caps);
    memfree_t(field_idx_offset_list);
    memfree_t(num_fields_list);
    memfree_t(next_free_tracking_field_list);
    memfree_t(num_free_list);
    // Len = total_num_id_fields
    memfree_t(field_allowed_id_list);
    init_status = DEINITIALIZED;
}


void EntityManager::free_entity_list_memory(TypeIndex p_type) {
    TypeFlags flags = type_flags_list[p_type];
    FieldRanges ranges = get_field_ranges(p_type);
    Size idx_limit = next_unused_index_list[p_type];
    for (FieldIndex f = ranges.start; f < ranges.end; f += 1) {
        void* data_raw = field_data_ptrs[f];
        if (has_at_least_1_variant_or_ref_field(flags)) {
            Size stride = field_stride_list[f];
            Type type = field_type_list[f];
            Type elem = get_elem_type(type);
            switch (elem) {
                case VARIANT: {
                    for (Size i = 0; i < idx_limit; i += 1) {
                        if (!is_free_abs(ranges.free, i)) {
                            Variant* var_ptr = get_elem_ptr_known_stride_cast(Variant*, f, stride, i);
                            var_ptr->~Variant();
                        }
                    }
                    break;
                }
                case ENTITY_REF: {
                    for (Size i = 0; i < idx_limit; i += 1) {
                        if (!is_free_abs(ranges.free, i)) {
                            Ref<EntityRef>* ref_ptr = get_elem_ptr_known_stride_cast(Ref<EntityRef>*, f, stride, i);
                            if (ref_ptr->ptr()) {
                                ref_ptr->ptr()->invalidate();
                            }
                            ref_ptr->unref();
                        }
                    }
                    break;
                }
                default: break;
            }
        }
        memfree(data_raw);
    }
    if (enable_entity_refs) {
        DEV_ASSERT(field_stride_list[ranges.ref] == sizeof(Ref<EntityRef>))
        for (Size i = 0; i < idx_limit; i += 1) {
            if (!is_free_abs(ranges.free, i)) {
                Ref<EntityRef>* ref_ptr = get_ent_ref_ptr(ranges.ref, i);
                if (ref_ptr->ptr()) {
                    ref_ptr->ptr()->invalidate();
                }
                ref_ptr->unref();
            }
        }
    }
    lens[p_type] = 0;
    caps[p_type] = 0;
    first_free_list[p_type] = 0;
    next_unused_index_list[p_type] = 1;
}

void EntityManager::clear_entity_list(TypeIndex p_type) {
    ERR_FAIL_COND_MSG(p_type >= num_types, "type index out of bounds for total number of defined types");
    FieldRanges ranges = get_field_ranges(p_type);
    TypeFlags flags = type_flags_list[p_type];
    Size idx_limit = next_unused_index_list[p_type];
    if (has_at_least_1_variant_or_ref_field(flags)) {
        for (FieldIndex f = ranges.start; f < ranges.end; f += 1) {
            Size stride = field_stride_list[f];
            Type type = field_type_list[f];
            Type elem = get_elem_type(type);
            switch (elem) {
                case VARIANT: {
                    for (Size i = 0; i < idx_limit; i += 1) {
                        if (!is_free_abs(ranges.free, i)) {
                            Variant* var_ptr = get_elem_ptr_known_stride_cast(Variant*, f, stride, i);
                            var_ptr->~Variant();
                        }
                    }
                    break;
                }
                case ENTITY_REF: {
                    for (Size i = 0; i < idx_limit; i += 1) {
                        if (!is_free_abs(ranges.free, i)) {
                            Ref<EntityRef>* ref_ptr = get_elem_ptr_known_stride_cast(Ref<EntityRef>*, f, stride, i);
                            if (ref_ptr->ptr()) {
                                ref_ptr->ptr()->invalidate();
                            }
                            ref_ptr->unref();
                        }
                    }
                    break;
                }
                default: break;
            }
        }
    }
    //FIXME get ref list if needed
    lens[p_type] = 0;
    first_free_list[p_type] = 0;
    next_unused_index_list[p_type] = 1;
}

bool EntityManager::entity_exists_internal(IdParts p_parts, FieldRanges p_field_ranges, bool ignore_gen) {
    if (is_free_abs(p_field_ranges.free, p_parts.index)) {return false;}
    if (!ignore_gen) {
        Gen* gen_data = ptrcast(Gen*, field_data_ptrs[p_field_ranges.gen]);
        Gen gen = gen_data[p_parts.index];
        if (gen != p_parts.gen) {return false;}
    }
    return true;
}

bool EntityManager::entity_exists(Id p_id) {
    IdParts parts = get_id_parts(p_id);
    if (invalid_id_parts(parts)) {return false;}
    FieldRanges ranges = get_field_ranges(parts.type_idx);
    return entity_exists_internal(parts, ranges);
}

bool EntityManager::destroy_internal(IdParts p_parts, FieldRanges p_field_ranges, TypeFlags p_flags) {
    if (!entity_exists_internal(p_parts, p_field_ranges)) { return false; }
    if (has_at_least_1_variant_or_ref_field(p_flags)) {
        for (FieldIndex f = p_field_ranges.start; f < p_field_ranges.end; f += 1) {
            Size stride = field_stride_list[f];
            Type type = field_type_list[f];
            Type elem = get_elem_type(type);
            switch (elem) {
                case VARIANT: {
                    Variant* var_ptr = get_elem_ptr_known_stride_cast(Variant*, f, stride, p_parts.index);
                    var_ptr->~Variant();
                    *var_ptr = Variant();
                    break;
                }
                case ENTITY_REF: {
                    Ref<EntityRef>* ref_ptr = get_elem_ptr_known_stride_cast(Ref<EntityRef>*, f, stride, p_parts.index);
                    if (ref_ptr->ptr()) {
                        ref_ptr->ptr()->invalidate();
                    }
                    ref_ptr->unref();
                    *ref_ptr = Ref<EntityRef>();
                    break;
                }
                default: break;
            }
        }
    }
    if (enable_entity_refs) {
        Ref<EntityRef>* ent_ref_ptr = get_elem_ptr_cast(Ref<EntityRef>*, p_field_ranges.ref, p_parts.index);
        if (ent_ref_ptr->ptr()) {
            ent_ref_ptr->ptr()->invalidate();
        }
        ent_ref_ptr->unref();
        *ent_ref_ptr = Ref<EntityRef>();
    }
    DEV_ASSERT(field_stride_list[p_field_ranges.gen] == sizeof(Gen))
    Gen* gen_ptr = get_gen_ptr(p_field_ranges.gen, p_parts.index);
    Gen gen = *gen_ptr;
    gen += 1;
    *gen_ptr = gen;
    set_free_flag_internal(p_field_ranges.free, p_parts.index);
    Size* next_free_ptr_on_entity = get_elem_ptr_cast(Size*, p_field_ranges.next_free, p_parts.index);
    Size prev_first_free = first_free_list[p_parts.type_idx];
    *next_free_ptr_on_entity = prev_first_free;
    first_free_list[p_parts.type_idx] = p_parts.index;
    num_free_list[p_parts.type_idx] += 1;
    return true;
}

bool EntityManager::destroy(Id p_id) {
    IdParts parts = get_id_parts(p_id);
    if (invalid_id_parts(parts)) {return false;}
    FieldRanges ranges = get_field_ranges(parts.type_idx);
    TypeFlags flags = type_flags_list[parts.type_idx];
    return destroy_internal(parts, ranges, flags);
}

Id EntityManager::create_internal(TypeIndex p_type, FieldRanges p_field_ranges, TypeFlags p_flags) {
    //FIXME
    return 0;
}
Id EntityManager::create(TypeIndex p_type_idx) {
    ERR_FAIL_COND_V_MSG(p_type_idx > num_types, Variant(), "type index is greater then the total number of types");
    FieldRanges ranges = get_field_ranges(p_type_idx);
    TypeFlags flags = type_flags_list[p_type_idx];
    return create_internal(p_type_idx, ranges, flags);
}


void EntityManager::define_system(TypeIndex p_num_types, bool p_enable_entity_refs) {
    ERR_FAIL_COND_MSG(init_status != UNINIT, "`define_system()` must be the FIRST step in the EntitySystem initialization process");
    init_status = SET_TOTAL_TYPES;
    num_types = p_num_types;
    enable_entity_refs = p_enable_entity_refs;
    lens = memalloc_zeroed_t(Size*, p_num_types * sizeof(Size));
    caps = memalloc_zeroed_t(Size*, p_num_types * sizeof(Size));
    next_unused_index_list = memalloc_zeroed_t(Size*, p_num_types * sizeof(Size));
    first_free_list = memalloc_zeroed_t(Size*, p_num_types * sizeof(Size));
    num_free_list = memalloc_zeroed_t(Size*, p_num_types * sizeof(Size));
    type_flags_list = memalloc_zeroed_t(TypeFlags*, p_num_types * sizeof(TypeFlags));
    field_idx_offset_list = memalloc_zeroed_t(FieldIndex*, p_num_types * sizeof(FieldIndex));
    num_fields_list = memalloc_zeroed_t(FieldIndex*, p_num_types * sizeof(FieldIndex));
    next_free_tracking_field_list = memalloc_zeroed_t(FieldIndex*, p_num_types * sizeof(FieldIndex));
    num_allowed_id_blocks_per_idx = ((num_types + 63) & ~63) >> 6;
    for (TypeIndex t = 0; t < num_types; t += 1) {
        next_unused_index_list[t] = 1;
    }
}

EntityManager::FieldRanges EntityManager::get_field_ranges(TypeIndex p_type_index) {
    EntityManager::FieldRanges ranges = {};
    FieldIndex offest = field_idx_offset_list[p_type_index];
    FieldIndex num = num_fields_list[p_type_index];
    ranges.num_fields = num;
    ranges.num_user_fields = num - 2;
    ranges.start = offest;
    FieldIndex abs_end = offest + num;
    ranges.free = abs_end - 1;
    ranges.gen = ranges.num_fields - 2;
    if (enable_entity_refs) {
        ranges.ref = abs_end - 3;
        ranges.end = abs_end - 3;
        ranges.num_user_fields = num - 3;
    } else {
        ranges.end = abs_end - 2;
        ranges.num_user_fields = num - 2;
    }
    ranges.next_free = next_free_tracking_field_list[p_type_index];
    return ranges;
}

void EntityManager::define_type(TypeIndex p_type, FieldIndex p_num_fields) {
    ERR_FAIL_COND_MSG(init_status != SET_TOTAL_TYPES, "`define_type()` calls must be the SECOND step in the EntitySystem initialization process (after `set_total_types()`)");
    ERR_FAIL_COND_MSG(p_type >= num_types, "type index out of bounds for total number of defined types");
    ERR_FAIL_COND_MSG(num_fields_list[p_type] != 0, "type was already defined");
    ERR_FAIL_COND_MSG(p_num_fields != 0, "cannot define a type with zero fields");
    if (enable_entity_refs) {
        p_num_fields += 3;
    } else {
        p_num_fields += 2;
    }
    num_fields_list[p_type] = p_num_fields;
}

void EntityManager::define_field_internal(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_type, bool p_has_allowed_ids, PackedInt32Array p_allowed_ids) {
    if (init_status == SET_TOTAL_TYPES) {
        FieldIndex f = 0;
        for (TypeIndex t = 0; t < num_types; t += 1) {
            ERR_FAIL_COND_MSG(num_fields_list[t] == 0, "you must call `define_type()` for EVERY type, as defined by `set_total_types()`");
            field_idx_offset_list[t] = f;
            f += num_fields_list[t];
        }
        total_num_fields = f;
        field_type_list = memalloc_zeroed_t(Type*, total_num_fields * sizeof(Type));
        field_allowed_id_list_index = memalloc_zeroed_t(Type*, total_num_fields * sizeof(Type));
        field_stride_list = memalloc_zeroed_t(Size*, total_num_fields * sizeof(Size));
        field_data_ptrs = memalloc_zeroed_t(void**, total_num_fields * sizeof(void*));
        init_status = DEFINING_ALL_FIELDS;
    }
    ERR_FAIL_COND_MSG(init_status != DEFINING_ALL_FIELDS, "`define_xxxxx_field()` calls must be the THRID step in the EntitySystem initialization process (after `set_total_types()` and all `define_type()` calls)");
    ERR_FAIL_COND_MSG(p_type_idx >= num_types, "type index out of bounds for total number of defined types");
    ERR_FAIL_COND_MSG(p_field_idx >= num_fields_list[p_type_idx], "field index out of bounds for defined number of fields on this type");
    FieldIndex fidx = field_idx_offset_list[p_type_idx] + p_field_idx;
    ERR_FAIL_COND_MSG(field_type_list[fidx] != 0, "field was already defined");
    ERR_FAIL_COND_MSG(p_type == 0, "no type data defined at all (resolves to NONE)");
    if (is_sub_int(p_type)) {
        set_at_least_1_sub_bit_field(p_type_idx);
    }
    field_type_list[fidx] = p_type;
    field_stride_list[fidx] = get_total_size(p_type);
    if (p_has_allowed_ids) {
        field_allowed_id_list_index[fidx] = total_num_id_fields;
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
            ERR_FAIL_COND_MSG(a > num_types, "allowed entity id type index is greater than the largest entity type");
            set_allowed_type_id(fidx, a);
        }
    } else {
        field_allowed_id_list_index[fidx] = 0;
    }
}
void EntityManager::define_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem) {
    ERR_FAIL_COND_MSG(is_entity_id(p_elem), "the `ENTITY_ID` type must be defined with `add_entity_id_field()` or `add_fixed_length_entity_id_array_field()`");
    define_field_internal(p_type_idx, p_field_idx, make_type(p_elem, NONE, 1), false, PackedInt32Array());
}
void EntityManager::define_entity_id_field(TypeIndex p_type_idx, FieldIndex p_field_idx, PackedInt32Array p_allowed_ids = PackedInt32Array()) {
    define_field_internal(p_type_idx, p_field_idx, make_type(ENTITY_ID, NONE, 1), true, p_allowed_ids);
}
void EntityManager::define_struct_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_struct) {
    ERR_FAIL_COND_MSG(!is_numeric(p_elem), "only numeric types (INT, FLOAT, U8, I32, I64, etc...) area allowed in `struct` types (VEC_2, VEC_3, COLOR_4, RECT_2, etc...)");
    define_field_internal(p_type_idx, p_field_idx, make_type(p_elem, p_struct, 1), false, PackedInt32Array());
}
void EntityManager::define_fixed_length_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_fixed) {
    ERR_FAIL_COND_MSG(is_entity_id(p_elem), "the `ENTITY_ID` type must be defined with `add_entity_id_field()` or `add_fixed_length_entity_id_array_field()`");
    define_field_internal(p_type_idx, p_field_idx, make_type(p_elem, NONE, MAX((Type)1, p_fixed)), false, PackedInt32Array());
}
void EntityManager::define_fixed_length_entity_id_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_fixed, PackedInt32Array p_allowed_ids = PackedInt32Array()) {
    define_field_internal(p_type_idx, p_field_idx, make_type(ENTITY_ID, NONE, MAX((Type)1, p_fixed)), true, p_allowed_ids);
}
void EntityManager::define_fixed_length_struct_array_field(TypeIndex p_type_idx, FieldIndex p_field_idx, Type p_elem, Type p_struct, Type p_fixed) {
    ERR_FAIL_COND_MSG(!is_numeric(p_elem), "only numeric types (INT, FLOAT, U8, I32, I64, etc...) area allowed in `struct` types (VEC_2, VEC_3, COLOR_4, RECT_2, etc...)");
    define_field_internal(p_type_idx, p_field_idx, make_type(p_elem, p_struct, MAX((Type)1, p_fixed)), false, PackedInt32Array());
}
void EntityManager::finalize_entity_system_layout() {
    ERR_FAIL_COND_MSG(init_status != DEFINING_ALL_FIELDS, "`finalize_entity_system_layout()` must be the FOURTH and final step in the EntitySystem initialization process (after `set_total_types()`, all `define_type()` calls, and all `define_xxxxx_field()` calls)");
    for (FieldIndex i = 0; i <= total_num_fields; i += 1) {
        ERR_FAIL_COND_MSG(field_type_list[i] == 0, "one of the declared fields was not defined");
    }
    for (TypeIndex t = 0; t < num_types; t += 1) {
        FieldRanges ranges = get_field_ranges(t);
        bool found_free_tracker = false;
        for (FieldIndex f = ranges.start; f < ranges.end; f += 1) {
            Size stride = field_stride_abs(f);
            if (stride >= 4) {
                found_free_tracker = true;
                next_free_tracking_field_list[t] = f;
                break;
            }
        }
        if (!found_free_tracker) {
            for (FieldIndex f = ranges.start; f < ranges.end; f += 1) {
                Type type = field_type_list[f];
                if (!is_sub_int(type)) {
                    found_free_tracker = true;
                    field_stride_list[f] = 4;
                    next_free_tracking_field_list[t] = f;
                    break;
                }
            }
        }
        ERR_FAIL_COND_MSG(!found_free_tracker, "no valid field for the next_free_id tracker was found for one of the types in the entity manager");
    }
    init_status = FINALIZED;
}

