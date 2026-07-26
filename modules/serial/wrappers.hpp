
# pragma once

#include "core/object/ref_counted.h"
#include "core/variant/variant.h"

class PackedByteArray_RWWrapper: public RefCounted {
    GDCLASS(PackedByteArray_RWWrapper, RefCounted);
public:
    PackedByteArray arr;
    int64_t rpos = 0;
    int64_t wpos = 0;
};
