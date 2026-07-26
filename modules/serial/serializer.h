
#pragma once

#include "core/object/ref_counted.h"
#include "reader_writer.hpp"

class Serializer : public RefCounted {
    GDCLASS(Serializer, RefCounted);

private:
    Ref<RefCounted> object;
    ReaderWriter::ReadWriteResult (*serialize)(Ref<RefCounted> object) = nullptr;
    ReaderWriter::ReadWriteResult (*deserialize)(Ref<RefCounted> object) = nullptr;

protected:
    static void _bind_methods();
    // Protected vars/methods

public:
    // Public vars/methods
    // int add(int a, int b) // example

    Serializer();
    // ~Serializer(); // Destructor
};

