
#include "register_types.h"
#include "core/object/class_db.h"
#include "serial.h"

void initialize_serial_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_abstract_class<Serializer>();
    ClassDB::register_abstract_class<ReaderWriter>();
    ClassDB::register_class<ReaderWriter_FileAccess>();
    ClassDB::register_class<ReaderWriter_PackedByteArray>();
    ClassDB::register_class<ReaderWriter_StreamPeer>();
}

void uninitialize_serial_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    // Perform cleanup if necessary.
}

