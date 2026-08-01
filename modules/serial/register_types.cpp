
#include "register_types.h"
#include "core/object/class_db.h"
#include "serial.h"

void initialize_serial_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_class<Serializer>();
    ClassDB::register_class<ReaderWriter>();
}

void uninitialize_serial_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    // Perform cleanup if necessary.
}

