
#include "register_types.h"
#include "core/object/class_db.h"
#include "entity_manager.h"

void initialize_my_module_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_class<EntityManager>();
}

void uninitialize_my_module_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    // Perform cleanup if necessary.
}

