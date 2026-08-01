
#pragma once

#include "core/io/file_access.h"
#include "core/io/stream_peer.h"
#include "core/math/aabb.h"
#include "core/math/projection.h"
#include "core/math/quaternion.h"
#include "core/math/rect2.h"
#include "core/math/transform_2d.h"
#include "core/math/transform_3d.h"
#include "core/object/ref_counted.h"
#include "core/typedefs.h"
#include "core/variant/type_info.h"
#include "core/variant/variant.h"

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
class ReaderWriter_RawPtr;
class ReaderWriter_RawPtrStride;

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
private:
    int64_t last_seek_delta = 0;
    uint32_t last_bytes_read_or_written = 0;
    uint32_t num_errors = 0;
    uint8_t first_error = ERROR::NONE;
    uint8_t last_error = ERROR::NONE;

    template<typename T>
    _FORCE_INLINE_ static constexpr uint32_t gds_type_index() {
        if constexpr (std::is_same_v<T, bool>) {
            return 0;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return 1;
        } else if constexpr (std::is_same_v<T, double>) {
            return 2;
        } else if constexpr (std::is_same_v<T, Vector2>) {
            return 3;
        } else if constexpr (std::is_same_v<T, Vector2i>) {
            return 4;
        } else if constexpr (std::is_same_v<T, Vector3>) {
            return 5;
        } else if constexpr (std::is_same_v<T, Vector3i>) {
            return 6;
        } else if constexpr (std::is_same_v<T, Vector4>) {
            return 7;
        } else if constexpr (std::is_same_v<T, Vector4i>) {
            return 8;
        } else if constexpr (std::is_same_v<T, Rect2>) {
            return 9;
        } else if constexpr (std::is_same_v<T, Rect2i>) {
            return 10;
        } else if constexpr (std::is_same_v<T, Color>) {
            return 11;
        } else if constexpr (std::is_same_v<T, Transform2D>) {
            return 12;
        } else if constexpr (std::is_same_v<T, Transform3D>) {
            return 13;
        } else if constexpr (std::is_same_v<T, Plane>) {
            return 14;
        } else if constexpr (std::is_same_v<T, Basis>) {
            return 15;
        } else if constexpr (std::is_same_v<T, Projection>) {
            return 16;
        } else if constexpr (std::is_same_v<T, Quaternion>) {
            return 17;
        } else if constexpr (std::is_same_v<T, AABB>) {
            return 18;
        } else {
           return 0xFFFFFFFF;
        }
    }
    using T_NATIVE_ELEM_LIST = std::tuple<
        bool, // BOOLEAN
        int64_t, // INTEGER
        double, // FLOAT
        real_t, // VEC2
        int32_t, // VEC2I
        real_t, // VEC3
        int32_t, // VEC3I
        real_t, // VEC4
        int32_t, // VEC4I
        real_t, // RECT2
        int32_t, // RECT2I
        float, // COLOR
        real_t, // TRANSFORM2D
        real_t, // TRANSFORM3D
        real_t, // PLANE
        real_t, // BASIS
        real_t, // PROJECTION
        real_t, // QUATERNION
        real_t // RECT3 (AABB)
    >;

    
protected:
    static void _bind_methods();
    
public:
    enum SEEK {
        FROM_START,
        FROM_CURRENT,
        FROM_END,
    };
    enum ERROR {
        NONE = 0,
        INVALID_STATE,
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
    };

    ReaderWriter() = default;

    virtual int64_t get_read_pos() = 0;
    virtual int64_t get_write_pos() = 0;
    virtual bool seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) = 0;
    virtual bool seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) = 0;
    virtual bool get_bytes(void* data_dst, uint32_t num_bytes) = 0;
    virtual bool set_bytes(const void* data_src, uint32_t num_bytes) = 0;
    virtual bool read_bytes(void* data_dst, uint32_t num_bytes) = 0;
    virtual bool write_bytes(const void* data_src, uint32_t num_bytes) = 0;

    _FORCE_INLINE_ void clear_errors();
    _FORCE_INLINE_ bool has_errors();
    _FORCE_INLINE_ uint8_t get_first_error();
    _FORCE_INLINE_ uint8_t get_last_error();
    _FORCE_INLINE_ uint32_t get_num_errors();
    _FORCE_INLINE_ int64_t get_last_seek_delta();
    _FORCE_INLINE_ uint32_t get_last_bytes_read_or_written();
    inline void add_result(int64_t p_seek_delta, uint32_t p_bytes_read_or_written, uint8_t p_error = ERROR::NONE);
    _FORCE_INLINE_ void add_error(uint8_t p_error = ERROR::NONE);

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
    inline T_NATIVE get_gds();

    template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    inline T_NATIVE read_gds();

    template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    inline bool set_gds(T_NATIVE val);

    template<typename T_SERIAL, typename T_NATIVE, typename T_NATIVE_ELEM, uint32_t T_NATIVE_ELEM_COUNT>
    inline bool write_gds(T_NATIVE val);

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
    _FORCE_INLINE_ static Ref<ReaderWriter> from_packed_byte_array(PackedByteArray file_access);
    // _FORCE_INLINE_ static Ref<ReaderWriter> from_stream_peer(Ref<StreamPeer> stream);
    // _FORCE_INLINE_ static Ref<ReaderWriter> from_raw_ptr(void* ptr);
    // _FORCE_INLINE_ static Ref<ReaderWriter> from_raw_ptr_with_stride(void* ptr, int64_t stride);
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
    bool get_bytes(void* data_dst, uint32_t num_bytes) override;
    bool set_bytes(const void* data_src, uint32_t num_bytes) override;
    bool read_bytes(void* data_dst, uint32_t num_bytes) override;
    bool write_bytes(const void* data_src, uint32_t num_bytes) override;
};

class ReaderWriter_PackedByteArray: public ReaderWriter {
    GDCLASS(ReaderWriter_PackedByteArray, RefCounted);
public:
    PackedByteArray array;
    int64_t wpos = 0;
    int64_t rpos = 0;

    ReaderWriter_PackedByteArray() = default;

    int64_t get_read_pos() override;
    int64_t get_write_pos() override;
    bool seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
    bool seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
    bool get_bytes(void* data_dst, uint32_t num_bytes) override;
    bool set_bytes(const void* data_src, uint32_t num_bytes) override;
    bool read_bytes(void* data_dst, uint32_t num_bytes) override;
    bool write_bytes(const void* data_src, uint32_t num_bytes) override;
};

// class ReaderWriter_StreamPeer: public ReaderWriter {
//     GDCLASS(ReaderWriter_StreamPeer, RefCounted);
// public:
//     Ref<StreamPeer> stream;

//     ReaderWriter_StreamPeer() = default;

//     int64_t get_read_pos() override;
//     int64_t get_write_pos() override;
//     bool seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
//     bool seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
//     bool get_bytes(void* data_dst, uint32_t num_bytes) override;
//     bool set_bytes(const void* data_src, uint32_t num_bytes) override;
//     bool read_bytes(void* data_dst, uint32_t num_bytes) override;
//     bool write_bytes(const void* data_src, uint32_t num_bytes) override;
// };

// class ReaderWriter_RawPtr: public ReaderWriter {
// public:
//     void* ptr;
//     int64_t wpos = 0;
//     int64_t rpos = 0;

//     ReaderWriter_RawPtr() = default;

//     int64_t get_read_pos() override;
//     int64_t get_write_pos() override;
//     bool seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
//     bool seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
//     bool get_bytes(void* data_dst, uint32_t num_bytes) override;
//     bool set_bytes(const void* data_src, uint32_t num_bytes) override;
//     bool read_bytes(void* data_dst, uint32_t num_bytes) override;
//     bool write_bytes(const void* data_src, uint32_t num_bytes) override;
// };

// class ReaderWriter_RawPtrStride: public ReaderWriter {
// public:
//     void* ptr;
//     int64_t wpos = 0;
//     int64_t rpos = 0;
//     int64_t stride = 1;

//     ReaderWriter_RawPtrStride() = default;

//     int64_t get_read_pos() override;
//     int64_t get_write_pos() override;
//     bool seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
//     bool seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) override;
//     bool get_bytes(void* data_dst, uint32_t num_bytes) override;
//     bool set_bytes(const void* data_src, uint32_t num_bytes) override;
//     bool read_bytes(void* data_dst, uint32_t num_bytes) override;
//     bool write_bytes(const void* data_src, uint32_t num_bytes) override;
// };

VARIANT_ENUM_CAST(ReaderWriter::SEEK);
VARIANT_ENUM_CAST(ReaderWriter::ERROR);

class Serializer : public RefCounted {
    GDCLASS(Serializer, RefCounted);

protected:
    static void _bind_methods();

public:
    Serializer() = default;

    virtual void serialize(Ref<ReaderWriter> reader_writer) = 0;
    virtual void deserialize(Ref<ReaderWriter> reader_writer) = 0;
};