
#include "entity_system.h"
#include "core/variant/variant.h"
#include <cstdint>

// int EntitySystem::add(int a, int b) {
//     return a + b;
// }

void EntitySystem::_bind_methods() {
    // bind gdscript visible methods here
    // ClassDB::bind_method(D_METHOD(add, a, b), &EntitySystem::add);
}

EntitySystem::EntitySystem() {
    // Initialize class state when created
}
// EntitySystem::~EntitySystem() {
//     // De-initialize class state when destroyed
// }


void EntitySystem::ensure_capacity(uint32_t p_size) {
    if (p_size > cap) {
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
        for (TypeIndex i = 0; i < num_types; i++) {
            void* data = data_ptrs[i];
            uint64_t t = type_list[i];
            uint32_t s = get_total_size(t);
            uint32_t total = cap * s;
            data = memrealloc(data, total);
            CRASH_COND_MSG(!data, "Out of memory");
            i += 1;
        }
    } else if (p_size < len) {
        WARN_VERBOSE("reserve() called with a cap smaller than the current size. This is likely a mistake.");
    }
}

void EntitySystem::reset() {
    clear();
    for (TypeIndex i = 0; i < num_types; i++) {
        void* data = data_ptrs[i];
        if (data) {
            memfree(data);
            data = nullptr;
        }
    }
    cap = 0;
}
void EntitySystem::resize(Size p_size) {
    if (p_size < len) {
        for (TypeIndex i = 0; i < num_types; i++) {
            Type type = type_list[i];
            if (is_variant(type)) {
                Variant* data = reinterpret_cast<Variant*>(data_ptrs[i]);
                for (Size ii = p_size; ii < len; ii++) {
                    data[ii].~Variant();
                }
            }
        }
        len = p_size;
    } else if (p_size > len) {
        ensure_capacity(p_size);
        for (TypeIndex i = 0; i < num_types; i++) {
            Type type = type_list[i];
            if (is_variant(type)) {
                Variant* data = reinterpret_cast<Variant*>(data_ptrs[i]);
                for (Size ii = p_size; ii < len; ii++) {
                    data[ii].~Variant();
                }
                memnew_arr_placement(data + len, p_size - len);
            }
        }
        len = p_size;
    }
}