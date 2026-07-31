
#pragma once

#include "core/io/file_access.h"
#include "core/object/ref_counted.h"
#include "core/os/memory.h"
#include "core/typedefs.h"
#include "core/variant/variant.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "wrappers.h"
#include "types.h"



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
    static void _bind_methods();
    
public:
    using SERIAL_TYPE = SerialType::SERIAL_TYPE;
    using NATIVE_TYPE = SerialType::NATIVE_TYPE;
    using ARRAY_TYPE = SerialType::ARRAY_TYPE;
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
    template<typename T>
    class PtrLen {
    public:
        T* ptr = nullptr;
        uint32_t len = 0;
    };
    using SerialError = uint8_t;
    class SeekResult {
    public:
        int64_t real_seek_delta = 0;
        SerialError err = ERROR::NONE;

        inline bool has_err() const {
            return err > 0;
        }
        static inline SeekResult error(SerialError p_err) {
            return SeekResult{0, p_err};
        }
        
    };
    class GetSetResult {
    public:
        uint32_t real_bytes_get_or_set = 0;
        SerialError err = ERROR::NONE;

        inline bool has_err() const {
            return err > 0;
        }
        static inline GetSetResult error(SerialError p_err) {
            return GetSetResult{0, p_err};
        }
    };
    
    class ReadWriteResult {
    public:
        int64_t real_seek_delta = 0;
        uint32_t bytes_read_or_written = 0;
        SerialError err = ERROR::NONE;

        inline bool has_err() const {
            return err > 0;
        }
        static inline ReadWriteResult error(SerialError p_err) {
            return ReadWriteResult{0, 0, p_err};
        }
    };
    
    class VTable {
    public:
        SeekResult (*get_read_pos)(Ref<RefCounted> object) = nullptr;
        SeekResult (*get_write_pos)(Ref<RefCounted> object) = nullptr;
        SeekResult (*seek_read_pos)(Ref<RefCounted> object, int64_t delta, SEEK from) = nullptr;
        SeekResult (*seek_write_pos)(Ref<RefCounted> object, int64_t delta, SEEK from) = nullptr;
        GetSetResult (*get_bytes)(Ref<RefCounted> object, void* data_dst, uint32_t num_bytes) = nullptr;
        GetSetResult (*set_bytes)(Ref<RefCounted> object, const void* data_src, uint32_t num_bytes) = nullptr;
        ReadWriteResult (*read_bytes)(Ref<RefCounted> object, void* data_dst, uint32_t num_bytes) = nullptr;
        ReadWriteResult (*write_bytes)(Ref<RefCounted> object, const void* data_src, uint32_t num_bytes) = nullptr;
    };
    inline ReaderWriter(Ref<RefCounted> p_object, const VTable* p_vtable) {
        object = p_object;
        vtable = p_vtable;
        first_error = ERROR::NONE;
    }
private:
    template<typename T>
    inline T collect_results(T res);
public:
    ReaderWriter() = default;

    inline void clear_errors();
    inline bool has_error();
    inline uint8_t get_first_error();
    inline uint8_t get_last_error();
    inline int64_t get_read_pos();
    inline int64_t get_write_pos();
    inline SeekResult seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT);
    inline SeekResult seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT);
    inline GetSetResult get_bytes(void* data_dst, uint32_t num_bytes);
    inline ReadWriteResult read_bytes(void* data_dst, uint32_t num_bytes);
    inline GetSetResult set_bytes(const void* data_src, uint32_t num_bytes);
    inline ReadWriteResult write_bytes(const void* data_src, uint32_t num_bytes);

    template<typename T>
    inline SeekResult seek_read_by_t_size();

    template<typename T>
    inline SeekResult seek_write_by_t_size();

    template<typename T>
    inline GetSetResult get_t(T* val_dst);

    template<typename T>
    inline ReadWriteResult read_t(T* val_dst);

    template<typename T>
    inline GetSetResult set_t(const T* val_src);

    template<typename T>
    inline ReadWriteResult write_t(const T* val_src);

    template<typename T>
    inline T get_t_val();

    template<typename T>
    inline T read_t_val();

    template<typename T>
    inline SerialError set_t_val(T val);

    template<typename T>
    inline SerialError write_t_val(T val);

    template<typename T_SERIAL, typename T_NATIVE>
    inline GetSetResult get_t_cast(T_NATIVE* val_dst);

    template<typename T_SERIAL, typename T_NATIVE>
    inline ReadWriteResult read_t_cast(T_NATIVE* val_dst);

    template<typename T_SERIAL, typename T_NATIVE>
    inline GetSetResult set_t_cast(const T_NATIVE* val_src);

    template<typename T_SERIAL, typename T_NATIVE>
    inline ReadWriteResult write_t_cast(const T_NATIVE* val_src);

    template<typename T_SERIAL, typename T_NATIVE>
    inline T_NATIVE get_t_cast_val();

    template<typename T_SERIAL, typename T_NATIVE>
    inline T_NATIVE read_t_cast_val();

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError set_t_cast_val(T_NATIVE val_src);

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError write_t_cast_val(T_NATIVE val_src);

    template<typename T_SERIAL, typename T_NATIVE>
    inline T_NATIVE get_gds();

    template<typename T_SERIAL,typename T_NATIVE>
    inline T_NATIVE read_gds();

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError set_gds(T_NATIVE val);

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError write_gds(T_NATIVE val);

    template<typename T>
    inline SerialError get_t_array(T* val_dst, uint32_t count);

    template<typename T>
    inline SerialError read_t_array(T* val_dst, uint32_t count);

    template<typename T>
    inline SerialError set_t_array(const T* val_src, uint32_t count);

    template<typename T>
    inline SerialError write_t_array(const T* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError get_t_cast_array(T_NATIVE* val_dst, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError read_t_cast_array(T_NATIVE* val_dst, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError set_t_cast_array(const T_NATIVE* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError write_t_cast_array(const T_NATIVE* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY get_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY read_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline SerialError set_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline SerialError write_t_cast_array_class(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY get_gds_array(T_ARRAY dest_array, uint32_t array_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY read_gds_array(T_ARRAY dest_array, uint32_t array_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    SerialError set_gds_array(T_ARRAY src_array, uint32_t array_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    SerialError write_gds_array(T_ARRAY src_array, uint32_t array_offset, uint32_t count);

    template<typename T>
    inline SerialError get_t_array_len_prefix(T* val_dst);

    template<typename T>
    inline SerialError read_t_array_len_prefix(T* val_dst);

    template<typename T>
    inline SerialError set_t_array_len_prefix(const T* val_src, uint32_t count);

    template<typename T>
    inline SerialError write_t_array_len_prefix(const T* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError get_t_cast_array_len_prefix(T_NATIVE* val_dst);

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError read_t_cast_array_len_prefix(T_NATIVE* val_dst);

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError set_t_cast_array_len_prefix(const T_NATIVE* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE>
    inline SerialError write_t_cast_array_len_prefix(const T_NATIVE* val_src, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY get_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset);
    
    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY read_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline SerialError set_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline SerialError write_t_cast_array_class_len_prefix(T_ARRAY arr, uint32_t arr_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY get_gds_array_len_prefix(T_ARRAY dest_array, uint32_t array_offset);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    inline T_ARRAY read_gds_array_len_prefix(T_ARRAY dest_array, uint32_t array_offset);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    SerialError set_gds_array_len_prefix(T_ARRAY src_array, uint32_t array_offset, uint32_t count);

    template<typename T_SERIAL, typename T_NATIVE, typename T_ARRAY>
    SerialError write_gds_array_len_prefix(T_ARRAY src_array, uint32_t array_offset, uint32_t count);
    
private:
    Ref<RefCounted> object = nullptr;
    const VTable* vtable = nullptr;
    int64_t last_seek_delta = 0;
    uint32_t last_bytes_read_or_written = 0;
    uint32_t num_errors = 0;
    uint8_t first_error = ERROR::NONE;
    uint8_t last_error = ERROR::NONE;
/*****************
 *  FileAccess
 *****************/
private:
    static SeekResult file_access_position(Ref<RefCounted> p_object);
    static SeekResult file_access_seek(Ref<RefCounted> p_object, int64_t delta, SEEK from);
    static ReadWriteResult file_access_read(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes);
    static GetSetResult file_access_get(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes);
    static ReadWriteResult file_access_write(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes);
    static GetSetResult file_access_set(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes);
    static constexpr VTable FileAccessVTable = {
        file_access_position,
        file_access_position,
        file_access_seek,
        file_access_seek,
        file_access_get,
        file_access_set,
        file_access_read,
        file_access_write,
    };
public:
    static ReaderWriter from_file_access(Ref<FileAccess> file_access) {
        return ReaderWriter{file_access, &FileAccessVTable};
    }
/*****************
 *  PackedByteArray
 *****************/
private:
    static SeekResult packed_bytes_read_pos(Ref<RefCounted> p_object);
    static SeekResult packed_bytes_write_pos(Ref<RefCounted> p_object);
    static SeekResult packed_bytes_seek_read(Ref<RefCounted> p_object, int64_t delta, SEEK from);
    static SeekResult packed_bytes_seek_write(Ref<RefCounted> p_object, int64_t delta, SEEK from);
    static ReadWriteResult packed_bytes_read(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes);
    static GetSetResult packed_bytes_get(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes);
    static ReadWriteResult packed_bytes_write(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes);
    static GetSetResult packed_bytes_set(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes);
    static constexpr VTable PackedBytesVTable = {
        packed_bytes_read_pos,
        packed_bytes_write_pos,
        packed_bytes_seek_read,
        packed_bytes_seek_write,
        packed_bytes_get,
        packed_bytes_set,
        packed_bytes_read,
        packed_bytes_write,
    };
public:
    static Ref<PackedByteArray_RWWrapper> wrapper_for_packed_byte_array(PackedByteArray p_arr) {
        Ref<PackedByteArray_RWWrapper> wrapper = memnew(PackedByteArray_RWWrapper);
        wrapper->arr = p_arr;
        wrapper->wpos = p_arr.size();
        return wrapper;
    }
    static ReaderWriter from_packed_byte_array_wrapper(Ref<PackedByteArray_RWWrapper> wrapper) {
        return ReaderWriter{wrapper, &PackedBytesVTable};
    }
};

VARIANT_ENUM_CAST(ReaderWriter::SEEK);
VARIANT_ENUM_CAST(ReaderWriter::ERROR);

