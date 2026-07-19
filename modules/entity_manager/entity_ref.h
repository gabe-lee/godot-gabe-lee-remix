#pragma once

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/object/object_id.h"
#include "core/typedefs.h"
#include "core/variant/variant.h"

typedef int64_t Id;
typedef uint16_t FieldIndex;

class EntityManager;

class EntityRef : public RefCounted {
    GDCLASS(EntityRef, RefCounted);

private:
    Id id = 0;
    ObjectID manager_id;
    bool valid = false;

    struct ManagerCheck {
        bool valid = false;
        EntityManager* manager = nullptr;
    };

    friend class EntityManager;

    _FORCE_INLINE_ void initialize(Id p_index, EntityManager* p_manager);

    _FORCE_INLINE_ void invalidate() {
        id = 0;
        valid = false;
        manager_id = ObjectID();
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("is_valid"), &EntityRef::is_valid);
        ClassDB::bind_method(D_METHOD("get_id"), &EntityRef::get_id);
        ClassDB::bind_method(D_METHOD("get", "field"), &EntityRef::get);
        ClassDB::bind_method(D_METHOD("set", "field", "value"), &EntityRef::set);
        ClassDB::bind_method(D_METHOD("destroy"), &EntityRef::destroy);
    }

    ManagerCheck check_manager(bool check_id_exists = true);

public:
    EntityRef() {}
    virtual ~EntityRef() override;

    bool set(FieldIndex p_field_index, Variant val);
    Variant get(FieldIndex p_field_index);
    bool destroy();

    _FORCE_INLINE_ bool is_valid() {
        return check_manager().valid;
    }

    _FORCE_INLINE_ Id get_id() const { return id; }
};
