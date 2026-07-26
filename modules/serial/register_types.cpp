
#include "register_types.h"
#include "core/object/class_db.h"
#include "serializer.h"

void initialize_serializer_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_class<Serializer>();
}

void uninitialize_serializer_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    // Perform cleanup if necessary.
}

