
#pragma once

#include "core/object/ref_counted.h"
#include "reader_writer.h"

class Serializer : public RefCounted {
    GDCLASS(Serializer, RefCounted);

private:
    Ref<RefCounted> object;
    ReaderWriter::ReadWriteResult (*serialize)(Ref<RefCounted> object) = nullptr;
    ReaderWriter::ReadWriteResult (*deserialize)(Ref<RefCounted> object) = nullptr;

protected:
    static void _bind_methods();

public:

    Serializer() = default;
};

