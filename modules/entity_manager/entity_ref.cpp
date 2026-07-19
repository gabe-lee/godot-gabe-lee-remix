
#include "core/object/object_id.h"
#include "core/variant/variant.h"

#include "entity_ref.h"
#include "entity_manager.h"

void EntityRef::initialize(Id p_index, EntityManager* p_manager) {
    id = p_index;
    valid = true;
    if (p_manager) {
        manager_id = p_manager->get_instance_id();
    }
}

EntityRef::ManagerCheck EntityRef::check_manager(bool check_id_exists) {
    if (!valid) {return ManagerCheck {};}
    if (manager_id.is_valid()) {
        Object* obj = ObjectDB::get_instance(manager_id);
        if (obj) {
            EntityManager* manager = Object::cast_to<EntityManager>(obj);
            if (manager) {
                if (check_id_exists && !manager->entity_exists(id)) {
                    valid = false;
                    return ManagerCheck {false, manager};
                } else {
                    return ManagerCheck {true, manager};
                }
            } else {
                valid = false;
                return ManagerCheck {};
            }
        } else {
            valid = false;
            return ManagerCheck {};
        }
    } else {
        valid = false;
        return ManagerCheck {};
    }
}

EntityRef::~EntityRef() {
    ManagerCheck result = check_manager();
    if (result.valid) {
        result.manager->destroy(id);
    }
}

bool EntityRef::set(FieldIndex p_field_index, Variant val) {
    ManagerCheck result = check_manager(false);
    if (result.valid) {
        return result.manager->set(id, p_field_index, val);
    } else {
        return false;
    }
}

Variant EntityRef::get(FieldIndex p_field_index) {
    ManagerCheck result = check_manager(false);
    if (result.valid) {
        return result.manager->get(id, p_field_index);
    } else {
        return Variant();
    }
}

bool EntityRef::destroy() {
    ManagerCheck result = check_manager(false);
    if (result.valid) {
        return result.manager->destroy(id);
    } else {
        return false;
    }
}