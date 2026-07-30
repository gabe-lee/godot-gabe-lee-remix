
#include "register_types.h"
#include "core/object/class_db.h"
#include "modules/serial/reader_writer.h"
#include "modules/serial/types.h"
#include "modules/serial/wrappers.h"
#include "serializer.h"

void initialize_serial_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_class<Serializer>();
    ClassDB::register_class<ReaderWriter>();
    ClassDB::register_class<PackedByteArray_RWWrapper>();
    ClassDB::register_class<SerialType>();
}

void uninitialize_serial_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    // Perform cleanup if necessary.
}

