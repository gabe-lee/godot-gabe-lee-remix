
#pragma once

#include "core/io/file_access.h"
#include "core/io/stream_peer.h"
#include "core/math/aabb.h"
#include "core/math/math_defs.h"
#include "core/object/ref_counted.h"
#include "core/typedefs.h"
#include "core/variant/type_info.h"
#include "core/variant/variant.h"
#include <cstdint>

class HalfU16 {
public:
    uint16_t raw = 0;

    HalfU16() = default;

    _FORCE_INLINE_ static uint32_t float_as_uint32(float f) {
        uint32_t bits;
        memcpy(&bits, &f, sizeof(bits));
        return bits;
    }
    _FORCE_INLINE_ static float uint32_as_float(uint32_t bits) {
        float f;
        std::memcpy(&f, &bits, sizeof(f));
        return f;
    }

    static uint16_t float_to_half_u16(float f);
    _FORCE_INLINE_ static uint16_t double_to_half_u16(double d) {
        return float_to_half_u16(static_cast<float>(d));
    }
    _FORCE_INLINE_ static uint16_t real_to_half_u16(real_t r) {
        return float_to_half_u16(static_cast<float>(r));
    }

    static float half_u16_to_float(uint16_t h);
    _FORCE_INLINE_ static double half_u16_to_double(uint16_t h) {
        return static_cast<double>(half_u16_to_float(h));
    }
    _FORCE_INLINE_ static real_t half_u16_to_real(uint16_t h) {
        return static_cast<real_t>(half_u16_to_float(h));
    }

    _FORCE_INLINE_ HalfU16(float f) {
        raw = float_to_half_u16(f);
    }
    _FORCE_INLINE_ operator float() const {
        return half_u16_to_float(raw);
    }
};


class ReaderWriter;
class ReaderWriter_FileAccess;
class ReaderWriter_PackedByteArray;
class ReaderWriter_StreamPeer;
class ReaderWriter_PtrWithLimit;

class ReaderWriter: public RefCounted {
    GDCLASS(ReaderWriter, RefCounted);
private:
    #if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    static constexpr bool ENDIAN_KNOWN = true;
    static constexpr bool TARGET_IS_LITTLE_ENDIAN = true;
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    static constexpr bool ENDIAN_KNOWN = true;
    static constexpr bool TARGET_IS_LITTLE_ENDIAN = false;
#elif defined(_WIN32)
    static constexpr bool ENDIAN_KNOWN = true;
    static constexpr bool TARGET_IS_LITTLE_ENDIAN = true;
#else
    static constexpr bool ENDIAN_KNOWN = false;
    static constexpr bool TARGET_IS_LITTLE_ENDIAN = true;
#endif
protected:
    int64_t last_seek_delta = 0;
    uint32_t last_bytes_copied = 0;
    uint32_t num_errors = 0;
    uint32_t num_errors_this_op = 0;
    uint8_t first_error = ERROR::NONE;
    uint8_t last_error = ERROR::NONE;

    static void _bind_methods();
    
public:
    static constexpr uint32_t INVALID = 0xFFFFFFFF; 
    enum ERROR {
        NONE = 0,
        INVALID_STATE,
        INVALID_ENUM_INPUT,
        INVALID_TYPE_TAG,
        READ_ERROR,
        WRITE_ERROR,
        OUT_OF_DATA_TO_READ,
        OUT_OF_SPACE_TO_WRITE,
        ARRAY_SOURCE_TOO_SHORT_FOR_OFFSET_AND_COUNT,
        CANNOT_SEEK_READ,
        CANNOT_SEEK_WRITE,
        CANNOT_READ,
        CANNOT_WRITE,
        CANNOT_GET,
        CANNOT_SET,
        SEEK_ERROR,
        SEEK_AFTER_DATA_RANGE,
        SEEK_BEFORE_DATA_RANGE,
        _ERR_LIMIT,
        _ERR_INVALID = 0xFFFFFFFF,
    };
    enum SEEK {
        FROM_START = _ERR_LIMIT,
        FROM_CURRENT,
        FROM_END,
        _SEEK_LIMIT,
        _SEEK_INVALID = 0xFFFFFFFF,
        _SEEK_MIN = _ERR_LIMIT,
    };
    enum GODOT_TYPE {
        BOOLEAN = _SEEK_LIMIT,
        INTEGER,
        FLOAT,
        VEC_2,
        VEC_2I,
        VEC_3,
        VEC_3I,
        VEC_4,
        VEC_4I,
        COLOR,
        RECT_2,
        RECT_2I,
        AABB,
        PLANE,
        BASIS,
        TRANSFORM_2D,
        TRANSFORM_3D,
        QUATERNION,
        PROJECTION,
        STRING,
        DICTIONARY,
        ARRAY,
        PACKED_BYTE_ARRAY,
        PACKED_INT32_ARRAY,
        PACKED_INT64_ARRAY,
        PACKED_FLOAT32_ARRAY,
        PACKED_FLOAT64_ARRAY,
        PACKED_STRING_ARRAY,
        PACKED_VECTOR2_ARRAY,
        PACKED_VECTOR3_ARRAY,
        PACKED_VECTOR4_ARRAY,
        PACKED_COLOR_ARRAY,
        ANY,
        _GD_TYPE_LIMIT,
        _GD_TYPE_INVALID = 0xFFFFFFFF,
        RECT_3 = AABB,
        _GD_TYPE_MIN = _SEEK_LIMIT,
    };
    static constexpr uint32_t GODOT_ELEM_COUNT[] = {
        1,// BOOL,
        1,// INT,
        1,// FLOAT,
        2,// VEC_2,
        2,// VEC_2I,
        3,// VEC_3,
        3,// VEC_3I,
        4,// VEC_4,
        4,// VEC_4I,
        4,// COLOR,
        4,// RECT_2,
        4,// RECT_2I,
        6,// AABB,
        4,// PLANE,
        9,// BASIS,
        6,// TRANSFORM_2D,
        12,// TRANSFORM_3D,
        4,// QUATERNION,
        16,// PROJECTION,
        1,// STRING,
        1,// VARIANT
    };
    using T_NATIVE_ELEM_LIST = std::tuple<
        bool,// BOOL,
        int64_t,// INT,
        double,// FLOAT,
        real_t,// VEC_2,
        int32_t,// VEC_2I,
        real_t,// VEC_3,
        int32_t,// VEC_3I,
        real_t,// VEC_4,
        int32_t,// VEC_4I,
        float,// COLOR,
        real_t,// RECT_2,
        int32_t,// RECT_2I,
        real_t,// AABB,
        real_t,// PLANE,
        real_t,// BASIS,
        real_t,// TRANSFORM_2D,
        real_t,// TRANSFORM_3D,
        real_t,// QUATERNION,
        real_t,// PROJECTION,
        uint32_t,// STRING,
        void// VARIANT,
    >;
    
    enum SERIAL_TYPE {
        BOOL = _GD_TYPE_LIMIT,
        U8,
        I8,
        U16,
        I16,
        U32,
        I32,
        U64,
        I64,
        F16,
        F32,
        F64,
        DEFAULT,
        _SERIAL_TYPE_LIMIT,
        _SERIAL_INVALID = 0xFFFFFFFF,
        _SERIAL_TYPE_MIN = _GD_TYPE_LIMIT,
        REAL = sizeof(real_t) == 4 ? F32 : F64,
    };
    using T_SERIAL_TYPE_LIST = std::tuple<
        bool, // BOOL,
        uint8_t,// U8,
        int8_t,// I8,
        uint16_t,// U16,
        int16_t,// I16,
        uint32_t,// U32,
        int32_t,// I32,
        uint64_t,// U64,
        int64_t,// I64,
        HalfU16,// F16,
        float,// F32,
        double,// F64,
        void//DEFAULT
    >;
    static constexpr uint32_t GODOT_DEFAULT_ELEM[] = {
        BOOL,// BOOL,
        I64,// INT,
        F64,// FLOAT,
        REAL,// VEC_2,
        I32,// VEC_2I,
        REAL,// VEC_3,
        I32,// VEC_3I,
        REAL,// VEC_4,
        I32,// VEC_4I,
        F32,// COLOR,
        REAL,// RECT_2,
        I32,// RECT_2I,
        REAL,// AABB,
        REAL,// PLANE,
        REAL,// BASIS,
        REAL,// TRANSFORM_2D,
        REAL,// TRANSFORM_3D,
        REAL,// QUATERNION,
        REAL,// PROJECTION,
        U32,// STRING,
        _SERIAL_INVALID// VARIANT,
    };
    static constexpr uint32_t VARIANT_TYPE_TO_GODOT_TYPE[] = {
        _GD_TYPE_INVALID,// NIL,
		BOOLEAN,// BOOL,
		INTEGER,// INT,
		FLOAT,// FLOAT,
		STRING,// STRING,
		VEC_2,// VECTOR2,
		VEC_2I,// VECTOR2I,
		RECT_2,// RECT2,
		RECT_2I,// RECT2I,
		VEC_3,// VECTOR3,
		VEC_3I,// VECTOR3I,
		TRANSFORM_2D,// TRANSFORM2D,
		VEC_4,// VECTOR4,
		VEC_4I,// VECTOR4I,
		PLANE,// PLANE,
		QUATERNION,// QUATERNION,
		AABB,// AABB,
		BASIS,// BASIS,
		TRANSFORM_3D,// TRANSFORM3D,
		PROJECTION,// PROJECTION,
		COLOR,// COLOR,
		_GD_TYPE_INVALID,// STRING_NAME,
		_GD_TYPE_INVALID,// NODE_PATH,
		_GD_TYPE_INVALID,// RID,
		_GD_TYPE_INVALID,// OBJECT,
		_GD_TYPE_INVALID,// CALLABLE,
		_GD_TYPE_INVALID,// SIGNAL,
		DICTIONARY,// DICTIONARY,
		ARRAY,// ARRAY,
		PACKED_BYTE_ARRAY,// PACKED_BYTE_ARRAY,
		PACKED_INT32_ARRAY,// PACKED_INT32_ARRAY,
		PACKED_INT64_ARRAY,// PACKED_INT64_ARRAY,
		PACKED_FLOAT32_ARRAY,// PACKED_FLOAT32_ARRAY,
		PACKED_FLOAT64_ARRAY,// PACKED_FLOAT64_ARRAY,
		PACKED_STRING_ARRAY,// PACKED_STRING_ARRAY,
		PACKED_VECTOR2_ARRAY,// PACKED_VECTOR2_ARRAY,
		PACKED_VECTOR3_ARRAY,// PACKED_VECTOR3_ARRAY,
		PACKED_COLOR_ARRAY,// PACKED_COLOR_ARRAY,
		PACKED_VECTOR4_ARRAY,// PACKED_VECTOR4_ARRAY,
		_GD_TYPE_INVALID,// VARIANT_MAX
    };

    ReaderWriter() = default;

    virtual int64_t get_read_pos() = 0;
    virtual int64_t get_write_pos() = 0;
    virtual bool seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) = 0;
    virtual bool seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) = 0;
    virtual bool read_bytes(void* data_dst, uint32_t num_bytes, bool no_seek = false, bool no_copy = false) = 0;
    virtual bool write_bytes(const void* data_src, uint32_t num_bytes, bool no_seek = false, bool no_copy = false) = 0;

    _FORCE_INLINE_ void clear_errors();
    _FORCE_INLINE_ void clear_deltas();
    _FORCE_INLINE_ bool has_errors();
    _FORCE_INLINE_ uint8_t get_first_error();
    _FORCE_INLINE_ uint8_t get_last_error();
    _FORCE_INLINE_ uint32_t get_num_errors();
    _FORCE_INLINE_ int64_t get_last_seek_delta();
    _FORCE_INLINE_ uint32_t get_last_bytes_copied();
    inline void add_result(int64_t p_seek_delta, uint32_t p_bytes_read_or_written, uint8_t p_error = ERROR::NONE);
    inline void add_error(uint8_t p_error = ERROR::NONE);

    _FORCE_INLINE_ bool get_bytes(void* data_dst, uint32_t num_bytes);
    _FORCE_INLINE_ bool set_bytes(const void* data_src, uint32_t num_bytes);

    template<typename T>
    inline bool seek_read_by_t_size();

    template<typename T>
    inline bool seek_write_by_t_size();

    template<typename T>
    inline bool get_t(T* val_dst);

    template<typename T>
    inline bool read_t(T* val_dst);

    template<typename T>
    inline bool set_t(const T* val_src);

    template<typename T>
    inline bool write_t(const T* val_src);

    template<typename T>
    inline T get_t_val();

    template<typename T>
    inline T read_t_val();

    template<typename T>
    inline bool set_t_val(T val);

    template<typename T>
    inline bool write_t_val(T val);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool get_t_cast(T_NATIVE* val_dst);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool read_t_cast(T_NATIVE* val_dst);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool set_t_cast(const T_NATIVE* val_src);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool write_t_cast(const T_NATIVE* val_src);

    template<typename T_SERIAL, typename T_NATIVE>
    inline T_NATIVE get_t_cast_val();

    template<typename T_SERIAL, typename T_NATIVE>
    inline T_NATIVE read_t_cast_val();

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool set_t_cast_val(T_NATIVE val_src);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool write_t_cast_val(T_NATIVE val_src);

    template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    inline T_NATIVE get_gds_impl();

    template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    inline T_NATIVE read_gds_impl();

    template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    inline bool set_gds_impl(T_NATIVE val);

    template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    inline bool write_gds_impl(T_NATIVE val);

    Variant get_gds(GODOT_TYPE type, SERIAL_TYPE serial_type = SERIAL_TYPE::DEFAULT);
    Variant read_gds(GODOT_TYPE type, SERIAL_TYPE serial_type = SERIAL_TYPE::DEFAULT);
    bool set_gds(GODOT_TYPE type, Variant val, SERIAL_TYPE serial_type = SERIAL_TYPE::DEFAULT);
    bool write_gds(GODOT_TYPE type, Variant val, SERIAL_TYPE serial_type = SERIAL_TYPE::DEFAULT);

    template<typename T>
    inline bool get_t_array(T* val_dst, uint32_t count);

    template<typename T>
    inline bool read_t_array(T* val_dst, uint32_t count);

    template<typename T>
    inline bool set_t_array(const T* val_src, uint32_t count);

    template<typename T>
    inline bool write_t_array(const T* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool get_t_cast_array(T_NATIVE* val_dst, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool read_t_cast_array(T_NATIVE* val_dst, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool set_t_cast_array(const T_NATIVE* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool write_t_cast_array(const T_NATIVE* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY get_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY read_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline bool set_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline bool write_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    inline T_ARRAY get_gds_array(T_ARRAY dest_array, uint32_t array_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    inline T_ARRAY read_gds_array(T_ARRAY dest_array, uint32_t array_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    bool set_gds_array(T_ARRAY src_array, uint32_t array_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    bool write_gds_array(T_ARRAY src_array, uint32_t array_offset, uint32_t count);

    template<typename T>
    inline bool get_t_array_len_prefix(T* val_dst);

    template<typename T>
    inline bool read_t_array_len_prefix(T* val_dst);

    template<typename T>
    inline bool set_t_array_len_prefix(const T* val_src, uint32_t count);

    template<typename T>
    inline bool write_t_array_len_prefix(const T* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool get_t_cast_array_len_prefix(T_NATIVE* val_dst);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool read_t_cast_array_len_prefix(T_NATIVE* val_dst);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool set_t_cast_array_len_prefix(const T_NATIVE* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline bool write_t_cast_array_len_prefix(const T_NATIVE* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY get_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset);
    
    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY read_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline bool set_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline bool write_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    inline T_ARRAY get_gds_array_len_prefix(T_ARRAY dest_array, uint32_t array_offset);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    inline T_ARRAY read_gds_array_len_prefix(T_ARRAY dest_array, uint32_t array_offset);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    bool set_gds_array_len_prefix(T_ARRAY src_array, uint32_t array_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    bool write_gds_array_len_prefix(T_ARRAY src_array, uint32_t array_offset, uint32_t count);
    

    _FORCE_INLINE_ static Ref<ReaderWriter_FileAccess> from_file_access(Ref<FileAccess> file_access);
    _FORCE_INLINE_ static Ref<ReaderWriter_PackedByteArray> from_packed_byte_array(PackedByteArray file_access);
    _FORCE_INLINE_ static Ref<ReaderWriter_StreamPeer> from_stream_peer(Ref<StreamPeer> stream);
    template<typename T>
    _FORCE_INLINE_ static Ref<ReaderWriter_PtrWithLimit> from_ptr_with_len(T* ptr, uint64_t ptr_len, bool can_realloc = false);
};

class ReaderWriter_FileAccess: public ReaderWriter {
    GDCLASS(ReaderWriter_FileAccess, RefCounted);
public:
    Ref<FileAccess> file_access;

    ReaderWriter_FileAccess() = default;

    int64_t get_read_pos() override;
    int64_t get_write_pos() override;
    bool seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
    bool seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
    bool read_bytes(void* data_dst, uint32_t num_bytes, bool no_seek, bool no_copy) override;
    bool write_bytes(const void* data_src, uint32_t num_bytes, bool no_seek, bool no_copy) override;
};

class ReaderWriter_PackedByteArray: public ReaderWriter {
    GDCLASS(ReaderWriter_PackedByteArray, RefCounted);
private:
    int64_t wpos = 0;
    int64_t rpos = 0;
public:
    PackedByteArray array;
    
    ReaderWriter_PackedByteArray() = default;

    int64_t get_read_pos() override;
    int64_t get_write_pos() override;
    bool seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
    bool seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
    bool read_bytes(void* data_dst, uint32_t num_bytes, bool no_seek, bool no_copy) override;
    bool write_bytes(const void* data_src, uint32_t num_bytes, bool no_seek, bool no_copy) override;
};

class ReaderWriter_StreamPeer: public ReaderWriter {
    GDCLASS(ReaderWriter_StreamPeer, RefCounted);
public:
    Ref<StreamPeer> stream;

    ReaderWriter_StreamPeer() = default;

    int64_t get_read_pos() override;
    int64_t get_write_pos() override;
    bool seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
    bool seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
    bool read_bytes(void* data_dst, uint32_t num_bytes, bool no_seek, bool no_copy) override;
    bool write_bytes(const void* data_src, uint32_t num_bytes, bool no_seek, bool no_copy) override;
};

class ReaderWriter_PtrWithLimit: public ReaderWriter {
private:
    uint8_t* ptr;
    uint64_t limit = 0;
    int64_t wpos = 0;
    int64_t rpos = 0;
    bool can_realloc = false;
public:
    ReaderWriter_PtrWithLimit() = default;
    friend class ReaderWriter;

    int64_t get_read_pos() override;
    int64_t get_write_pos() override;
    bool seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
    bool seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
    bool read_bytes(void* data_dst, uint32_t num_bytes, bool no_seek, bool no_copy) override;
    bool write_bytes(const void* data_src, uint32_t num_bytes, bool no_seek, bool no_copy) override;
};

VARIANT_ENUM_CAST(ReaderWriter::SEEK);
VARIANT_ENUM_CAST(ReaderWriter::ERROR);
VARIANT_ENUM_CAST(ReaderWriter::SERIAL_TYPE);
VARIANT_ENUM_CAST(ReaderWriter::GODOT_TYPE);

class Serializer : public RefCounted {
    GDCLASS(Serializer, RefCounted);

protected:
    static void _bind_methods();

public:

    virtual void serialize(Ref<ReaderWriter> reader_writer) = 0;
    virtual void deserialize(Ref<ReaderWriter> reader_writer) = 0;
};