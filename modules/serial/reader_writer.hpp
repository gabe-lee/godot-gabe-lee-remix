
#pragma once

#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/math/basis.h"
#include "core/math/color.h"
#include "core/math/projection.h"
#include "core/math/quaternion.h"
#include "core/math/rect2.h"
#include "core/math/transform_2d.h"
#include "core/math/vector2.h"
#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/os/memory.h"
#include "core/typedefs.h"
#include "core/variant/variant.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "wrappers.hpp"

#define read_get_gds_single(m_type, m_type_name, m_default, m_result, m_read_get) inline m_type gds_##m_read_get##_##m_type_name() {\
    m_type val = m_default;\
    if (last_error == ERROR::NONE) { \
        m_result result = m_read_get##_t<m_type>(&val);\
        last_error = result.err; \
    } \
    return val; \
}
#define set_write_gds_single(m_type, m_type_name, m_default, m_result, m_write_set) inline int64_t gds_##m_write_set##_##m_type_name(m_type val) {\
    if (last_error == ERROR::NONE) { \
        m_result result = m_write_set##_t<m_type>(&val);\
        last_error = result.err; \
    } \
    return static_cast<int64_t>(last_error); \
}
#define gds_single(m_type, m_type_name, m_default) \
read_get_gds_single(m_type, m_type_name, m_default, GetSetResult, get) \
read_get_gds_single(m_type, m_type_name, m_default, ReadWriteResult, read) \
set_write_gds_single(m_type, m_type_name, m_default, GetSetResult, set) \
set_write_gds_single(m_type, m_type_name, m_default, ReadWriteResult, write) \
// #define get_gds_single_packed_arr(m_type, m_type_name) m_type get_##m_type_name() const {\
//     m_type arr;\
//     if (last_error == ERROR::NONE) { \
//         uint32_t len = get
//         GetSetResult result = get_t<mtype>(&val);\
//         last_error = result.err; \
//     } \
//     return arr; \
// }

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
        SEEK_ERROR,
        SEEK_AFTER_DATA_RANGE,
        SEEK_BEFORE_DATA_RANGE,
    };
    class SeekResult {
    public:
        int64_t real_seek_delta = 0;
        uint32_t err = ERROR::NONE;

        inline bool has_err() const {
            return err > 0;
        }
        static inline SeekResult error(uint32_t p_err) {
            return SeekResult{0, p_err};
        }
    };
    class GetSetResult {
    public:
        uint32_t real_bytes_get_or_set = 0;
        uint32_t err = ERROR::NONE;

        inline bool has_err() const {
            return err > 0;
        }
        static inline GetSetResult error(uint32_t p_err) {
            return GetSetResult{0, p_err};
        }
    };
    
    class ReadWriteResult {
    public:
        int64_t real_seek_delta = 0;
        uint32_t bytes_read_or_written = 0;
        uint32_t err = ERROR::NONE;

        inline bool has_err() const {
            return err > 0;
        }
        static inline ReadWriteResult error(uint32_t p_err) {
            return ReadWriteResult{0, 0, p_err};
        }
    };
    
    class VTable {
    public:
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
        last_error = ERROR::NONE;
    }
    inline SeekResult seek_read_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) const {
        return vtable->seek_read_pos(object, delta, from);
    }
    inline SeekResult seek_write_pos(int64_t delta, SEEK from = SEEK::FROM_CURRENT) const {
        return vtable->seek_write_pos(object, delta, from);
    }
    inline GetSetResult get_bytes(void* data_dst, uint32_t num_bytes) const {
        return vtable->get_bytes(object, data_dst, num_bytes);
    }
    // inline GetSetResultGD get_bytes_gdscript(PackedByteArray data_dst, int64_t data_dst_start_index, uint32_t num_bytes) const {
    //     Ref<GetSetResultGD> result = memnew(GetSetResultGD);
    //     result->altered_src_or_dst = data_dst;
    //     if (data_dst_start_index + num_bytes > data_dst.size()) {
    //         data_dst.resize(Size p_size)
    //     }
    //     ERR_FAIL_COND_V_MSG(data_dst_start_index + num_bytes > data_dst.size(), GetSetResult::, m_msg)
    //     return vtable->get_bytes(object, data_dst, num_bytes);
    // }
    inline ReadWriteResult read_bytes(void* data_dst, uint32_t num_bytes) const {
        return vtable->read_bytes(object, data_dst, num_bytes);
    }
    inline GetSetResult set_bytes(const void* data_src, uint32_t num_bytes) const {
        return vtable->set_bytes(object, data_src, num_bytes);
    }
    inline ReadWriteResult write_bytes(const void* data_src, uint32_t num_bytes) const {
        return vtable->write_bytes(object, data_src, num_bytes);
    }
    template<typename T>
    inline SeekResult seek_read_by_t_size() const {
        return vtable->seek_read_pos(object, sizeof(T), SEEK::FROM_CURRENT);
    }
    template<typename T>
    inline SeekResult seek_write_by_t_size() const {
        return vtable->seek_write_pos(object, sizeof(T), SEEK::FROM_CURRENT);
    }
    template<typename T>
    inline GetSetResult get_t(T* val_dst) const {
        return get_bytes(reinterpret_cast<void*>(val_dst), sizeof(T));
    }
    template<typename T>
    inline ReadWriteResult read_t(T* val_dst) const {
        return read_bytes(reinterpret_cast<void*>(val_dst), sizeof(T));
    }
    template<typename T>
    inline GetSetResult set_t(const T* val_src) const {
        return set_bytes(reinterpret_cast<const void*>(val_src), sizeof(T));
    }
    template<typename T>
    inline ReadWriteResult write_t(const T* val_src) const {
        return write_bytes(reinterpret_cast<const void*>(val_src), sizeof(T));
    }
    template<typename T>
    inline GetSetResult get_t_swap_endian(T* val_dst) const {
        uint8_t buf[sizeof(T)];
        GetSetResult result = get_bytes(reinterpret_cast<void*>(&buf[0]), sizeof(T));
        if (result.has_err()) {return result;}
        uint8_t* dst_opaque = reinterpret_cast<uint8_t*>(val_dst);
        std::reverse_copy(&buf[0], &buf[0] + sizeof(T), dst_opaque);
        return result;
    }
    template<typename T>
    inline ReadWriteResult read_t_swap_endian(T* val_dst) const {
        uint8_t buf[sizeof(T)];
        ReadWriteResult result = read_bytes(reinterpret_cast<void*>(&buf[0]), sizeof(T));
        if (result.has_err()) {return result;}
        uint8_t* dst_opaque = reinterpret_cast<uint8_t*>(val_dst);
        std::reverse_copy(&buf[0], &buf[0] + sizeof(T), dst_opaque);
        return result;
    }
    template<typename T>
    inline GetSetResult set_t_swap_endian(const T* val_src) const {
        uint8_t buf[sizeof(T)];
        const uint8_t* src_opaque = reinterpret_cast<const uint8_t*>(val_src);
        std::reverse_copy(src_opaque, src_opaque + sizeof(T), &buf[0]);
        return set_bytes(reinterpret_cast<void*>(&buf[0]), sizeof(T));
    }
    template<typename T>
    inline ReadWriteResult write_t_swap_endian(const T* val_src) const {
        uint8_t buf[sizeof(T)];
        const uint8_t* src_opaque = reinterpret_cast<const uint8_t*>(val_src);
        std::reverse_copy(src_opaque, src_opaque + sizeof(T), &buf[0]);
        return write_bytes(reinterpret_cast<void*>(&buf[0]), sizeof(T));
    }
    template<typename T>
    inline GetSetResult get_t_little_endian(T* val_dst) const {
        static_assert(ENDIAN_KNOWN, "cannot use this method when the platform endianess is not available at compile time");
        if constexpr (TARGET_IS_LITTLE_ENDIAN) {
            return get_t(val_dst);
        } else {
            return get_t_swap_endian(val_dst);
        }
    }
    template<typename T>
    inline GetSetResult get_t_big_endian(T* val_dst) const {
        static_assert(ENDIAN_KNOWN, "cannot use this method when the platform endianess is not available at compile time");
        if constexpr (!TARGET_IS_LITTLE_ENDIAN) {
            return get_t(val_dst);
        } else {
            return get_t_swap_endian(val_dst);
        }
    }
    template<typename T>
    inline ReadWriteResult read_t_little_endian(T* val_dst) const {
        static_assert(ENDIAN_KNOWN, "cannot use this method when the platform endianess is not available at compile time");
        if constexpr (TARGET_IS_LITTLE_ENDIAN) {
            return read_t(val_dst);
        } else {
            return read_t_swap_endian(val_dst);
        }
    }
    template<typename T>
    inline ReadWriteResult read_t_big_endian(T* val_dst) const {
        static_assert(ENDIAN_KNOWN, "cannot use this method when the platform endianess is not available at compile time");
        if constexpr (!TARGET_IS_LITTLE_ENDIAN) {
            return read_t(val_dst);
        } else {
            return read_t_swap_endian(val_dst);
        }
    }
    template<typename T>
    inline GetSetResult set_t_little_endian(const T* val_src) const {
        static_assert(ENDIAN_KNOWN, "cannot use this method when the platform endianess is not available at compile time");
        if constexpr (TARGET_IS_LITTLE_ENDIAN) {
            return set_t(val_src);
        } else {
            return set_t_swap_endian(val_src);
        }
    }
    template<typename T>
    inline GetSetResult set_t_big_endian(const T* val_src) const {
        static_assert(ENDIAN_KNOWN, "cannot use this method when the platform endianess is not available at compile time");
        if constexpr (!TARGET_IS_LITTLE_ENDIAN) {
            return set_t(val_src);
        } else {
            return set_t_swap_endian(val_src);
        }
    }
    template<typename T>
    inline ReadWriteResult write_t_little_endian(const T* val_src) const {
        static_assert(ENDIAN_KNOWN, "cannot use this method when the platform endianess is not available at compile time");
        if constexpr (TARGET_IS_LITTLE_ENDIAN) {
            return write_t(val_src);
        } else {
            return write_t_swap_endian(val_src);
        }
    }
    template<typename T>
    inline ReadWriteResult write_t_big_endian(const T* val_src) const {
        static_assert(ENDIAN_KNOWN, "cannot use this method when the platform endianess is not available at compile time");
        if constexpr (!TARGET_IS_LITTLE_ENDIAN) {
            return write_t(val_src);
        } else {
            return write_t_swap_endian(val_src);
        }
    }

    gds_single(bool, bool, false)
    gds_single(int64_t, integer, 0)
    gds_single(double, float, 0.0)
    gds_single(Vector2, vec2, Vector2())
    gds_single(Vector2i, vec2i, Vector2i())
    gds_single(Vector3, vec3, Vector3())
    gds_single(Vector3i, vec3i, Vector3i())
    gds_single(Vector4, vec4, Vector4())
    gds_single(Vector4i, vec4i, Vector4i())
    gds_single(Rect2, rect2, Rect2())
    gds_single(Rect2i, rect2i, Rect2i())
    gds_single(Color, color, Color())
    gds_single(Plane, plane, Plane())
    gds_single(Transform2D, transform2d, Transform2D())
    gds_single(Transform3D, transform3d, Transform3D())
    gds_single(Quaternion, quaternion, Quaternion())
    gds_single(Basis, basis, Basis())
    gds_single(Projection, projection, Projection())
    gds_single(::AABB, aabb, ::AABB())
private:
    Ref<RefCounted> object = nullptr;
    const VTable* vtable = nullptr;
    uint32_t last_error = ERROR::NONE;
// FileAccess VTable
private:
    static SeekResult file_access_seek(Ref<RefCounted> p_object, int64_t delta, SEEK from) {
        if (p_object.is_null()) {return SeekResult::error(ERROR::INVALID_STATE);}
        FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
        if (fa == nullptr) {return SeekResult::error(ERROR::INVALID_STATE);}
        if (!fa->is_open()) {return SeekResult::error(ERROR::INVALID_STATE);}
        int64_t pos = static_cast<int64_t>(fa->get_position());
        int64_t new_pos = pos;
        switch (from) {
            case SEEK::FROM_START: {
                ERR_FAIL_COND_V_MSG(delta < 0, SeekResult::error(ERROR::SEEK_BEFORE_DATA_RANGE), "cannot seek to before beginning of data");
                fa->seek(static_cast<uint64_t>(delta));
                int64_t e = fa->get_error();
                ERR_FAIL_COND_V_MSG(e > 0, SeekResult::error(ERROR::SEEK_AFTER_DATA_RANGE), "cannot seek to after end of data");
                break;
            }
            case SEEK::FROM_CURRENT: {
                int64_t spos = pos;
                spos += delta;
                ERR_FAIL_COND_V_MSG(spos < 0, SeekResult::error(ERROR::SEEK_BEFORE_DATA_RANGE), "cannot seek to before beginning of data");
                fa->seek(static_cast<uint64_t>(spos));
                int64_t e = fa->get_error();
                ERR_FAIL_COND_V_MSG(e > 0, SeekResult::error(ERROR::SEEK_AFTER_DATA_RANGE), "cannot seek to after end of data");
                break;
            }
            case SEEK::FROM_END: {
                ERR_FAIL_COND_V_MSG(delta > 0, SeekResult::error(ERROR::SEEK_AFTER_DATA_RANGE), "cannot seek to after end of data");
                fa->seek_end(delta);
                int64_t e = fa->get_error();
                ERR_FAIL_COND_V_MSG(e > 0, SeekResult::error(ERROR::SEEK_BEFORE_DATA_RANGE), "cannot seek to before beginning of data");
                break;
            }
        }
        new_pos = static_cast<int64_t>(fa->get_position());
        return SeekResult{new_pos - pos, ERROR::NONE};
    }
    static ReadWriteResult file_access_read(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
        if (p_object.is_null()) {return ReadWriteResult::error(ERROR::INVALID_STATE);}
        FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
        if (fa == nullptr) {return ReadWriteResult::error(ERROR::INVALID_STATE);}
        if (!fa->is_open()) {return ReadWriteResult::error(ERROR::INVALID_STATE);}
        uint64_t num_read = fa->get_buffer(reinterpret_cast<uint8_t*>(data_dst), static_cast<uint64_t>(num_bytes));
        uint32_t e = ERROR::NONE;
        if (num_read < num_bytes) {
            e = ERROR::OUT_OF_DATA_TO_READ;
        }
        return ReadWriteResult{static_cast<int64_t>(num_read), static_cast<uint32_t>(num_read), e};
    }
    static GetSetResult file_access_get(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
        if (p_object.is_null()) {return GetSetResult::error(ERROR::INVALID_STATE);}
        FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
        if (fa == nullptr) {return GetSetResult::error(ERROR::INVALID_STATE);}
        if (!fa->is_open()) {return GetSetResult::error(ERROR::INVALID_STATE);}
        int64_t initial_pos = static_cast<int64_t>(fa->get_position());
        uint64_t num_read = fa->get_buffer(reinterpret_cast<uint8_t*>(data_dst), static_cast<uint64_t>(num_bytes));
        uint32_t e = ERROR::NONE;
        if (num_read < num_bytes) {
            e = ERROR::OUT_OF_DATA_TO_READ;
        }
        fa->seek(initial_pos);
        if (e == 0) {
            if (fa->get_error()) {
                e = ERROR::SEEK_ERROR;
            }
        }
        return GetSetResult{static_cast<uint32_t>(num_read), e};
    }
    static ReadWriteResult file_access_write(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
        if (p_object.is_null()) {return ReadWriteResult::error(ERROR::INVALID_STATE);}
        FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
        if (fa == nullptr) {return ReadWriteResult::error(ERROR::INVALID_STATE);}
        if (!fa->is_open()) {return ReadWriteResult::error(ERROR::INVALID_STATE);}
        uint64_t num_written = fa->store_buffer(reinterpret_cast<const uint8_t*>(data_src), static_cast<uint64_t>(num_bytes));
        uint32_t e = ERROR::NONE;
        if (num_written < num_bytes) {
            e = ERROR::OUT_OF_SPACE_TO_WRITE;
        }
        return ReadWriteResult{static_cast<int64_t>(num_written), static_cast<uint32_t>(num_written), e};
    }
    static GetSetResult file_access_set(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
        if (p_object.is_null()) {return GetSetResult::error(ERROR::INVALID_STATE);}
        FileAccess* fa = Object::cast_to<FileAccess>(p_object.ptr());
        if (fa == nullptr) {return GetSetResult::error(ERROR::INVALID_STATE);}
        if (!fa->is_open()) {return GetSetResult::error(ERROR::INVALID_STATE);}
        int64_t initial_pos = static_cast<int64_t>(fa->get_position());
        uint64_t num_written = fa->store_buffer(reinterpret_cast<const uint8_t*>(data_src), static_cast<uint64_t>(num_bytes));
        uint32_t e = ERROR::NONE;
        if (num_written < num_bytes) {
            e = ERROR::OUT_OF_SPACE_TO_WRITE;
        }
        fa->seek(initial_pos);
        if (e == 0) {
            if (fa->get_error()) {
                e = ERROR::SEEK_ERROR;
            }
        }
        return GetSetResult{static_cast<uint32_t>(num_written), e};
    }
    static constexpr VTable FileAccessVTable = {
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
// PackedByteArray wrapper VTable
private:
    static SeekResult packed_bytes_seek_read(Ref<RefCounted> p_object, int64_t delta, SEEK from) {
        if (p_object.is_null()) {return SeekResult::error(ERROR::INVALID_STATE);}
        PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
        if (wrapper == nullptr) {return SeekResult::error(ERROR::INVALID_STATE);}
        int64_t old_pos = wrapper->rpos;
        int64_t new_pos;
        switch (from) {
            case SEEK::FROM_START: {
                new_pos = delta;
                break;
            }
            case SEEK::FROM_CURRENT: {
                new_pos = old_pos + delta;
                break;
            }
            case SEEK::FROM_END: {
                new_pos = static_cast<int64_t>(wrapper->arr.size()) + delta;
                break;
            }
        }
        uint32_t e = ERROR::NONE;
        if (new_pos < 0) {e = ERROR::SEEK_BEFORE_DATA_RANGE;}
        else if (new_pos > wrapper->arr.size()) {e = ERROR::SEEK_AFTER_DATA_RANGE;}
        new_pos = MAX((int64_t)0, MIN(new_pos, MIN(wrapper->wpos, wrapper->arr.size())));
        wrapper->rpos = new_pos;
        int64_t true_delta = new_pos - old_pos;
        return SeekResult{true_delta, e};
    }
    static SeekResult packed_bytes_seek_write(Ref<RefCounted> p_object, int64_t delta, SEEK from) {
        if (p_object.is_null()) {return SeekResult::error(ERROR::INVALID_STATE);}
        PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
        if (wrapper == nullptr) {return SeekResult::error(ERROR::INVALID_STATE);}
        int64_t old_pos = wrapper->wpos;
        int64_t new_pos;
        switch (from) {
            case SEEK::FROM_START: {
                new_pos = delta;
                break;
            }
            case SEEK::FROM_CURRENT: {
                new_pos = old_pos + delta;
                break;
            }
            case SEEK::FROM_END: {
                new_pos = static_cast<int64_t>(wrapper->arr.size()) + delta;
                break;
            }
        }
        uint32_t e = ERROR::NONE;
        if (new_pos < 0) {e = ERROR::SEEK_BEFORE_DATA_RANGE;}
        else if (new_pos > wrapper->arr.size()) {e = ERROR::SEEK_AFTER_DATA_RANGE;}
        new_pos = MAX(MAX((int64_t)0, wrapper->rpos), MIN(new_pos,  wrapper->arr.size()));
        wrapper->wpos = new_pos;
        int64_t true_delta = new_pos - old_pos;
        return SeekResult{true_delta, e};
    }
    static ReadWriteResult packed_bytes_read(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
        if (p_object.is_null()) {return ReadWriteResult::error(ERROR::INVALID_STATE);}
        PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
        if (wrapper == nullptr) {return ReadWriteResult::error(ERROR::INVALID_STATE);}
        int64_t end = wrapper->rpos + static_cast<int64_t>(num_bytes);
        if (end > static_cast<int64_t>(wrapper->arr.size()))  {return ReadWriteResult::error(ERROR::OUT_OF_DATA_TO_READ);}
        const uint8_t* data_src = wrapper->arr.ptr();
        memcpy(data_dst, data_src, num_bytes);
        wrapper->rpos = end;
        return ReadWriteResult{num_bytes, num_bytes, ERROR::NONE};
    }
    static GetSetResult packed_bytes_get(Ref<RefCounted> p_object, void* data_dst, uint32_t num_bytes) {
        if (p_object.is_null()) {return GetSetResult::error(ERROR::INVALID_STATE);}
        PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
        if (wrapper == nullptr) {return GetSetResult::error(ERROR::INVALID_STATE);}
        int64_t end = wrapper->rpos + static_cast<int64_t>(num_bytes);
        if (end > static_cast<int64_t>(wrapper->arr.size()))  {return GetSetResult::error(ERROR::OUT_OF_DATA_TO_READ);}
        const uint8_t* data_src = wrapper->arr.ptr();
        memcpy(data_dst, data_src, num_bytes);
        return GetSetResult{num_bytes, ERROR::NONE};
    }
    static ReadWriteResult packed_bytes_write(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
        if (p_object.is_null()) {return ReadWriteResult::error(ERROR::INVALID_STATE);}
        PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
        if (wrapper == nullptr) {return ReadWriteResult::error(ERROR::INVALID_STATE);}
        int64_t end = wrapper->wpos + static_cast<int64_t>(num_bytes);
        if (end > static_cast<int64_t>(wrapper->arr.size()))  {
            if (wrapper->arr.resize(end)) {
                {return ReadWriteResult::error(ERROR::OUT_OF_SPACE_TO_WRITE);}
            }
        }
        uint8_t* data_dst = wrapper->arr.ptrw();
        memcpy(data_dst, data_src, num_bytes);
        wrapper->wpos = end;
        return ReadWriteResult{num_bytes, num_bytes, ERROR::NONE};
    }
    static GetSetResult packed_bytes_set(Ref<RefCounted> p_object, const void* data_src, uint32_t num_bytes) {
        if (p_object.is_null()) {return GetSetResult::error(ERROR::INVALID_STATE);}
        PackedByteArray_RWWrapper* wrapper = Object::cast_to<PackedByteArray_RWWrapper>(p_object.ptr());
        if (wrapper == nullptr) {return GetSetResult::error(ERROR::INVALID_STATE);}
        int64_t end = wrapper->wpos + static_cast<int64_t>(num_bytes);
        if (end > static_cast<int64_t>(wrapper->arr.size())) {
            if (wrapper->arr.resize(end)) {
                {return GetSetResult::error(ERROR::OUT_OF_SPACE_TO_WRITE);}
            }
        }
        uint8_t* data_dst = wrapper->arr.ptrw();
        memcpy(data_dst, data_src, num_bytes);
        return GetSetResult{num_bytes, ERROR::NONE};
    }
    static constexpr VTable PackedBytesVTable = {
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
        return wrapper;
    }
    static ReaderWriter from_packed_byte_array_wrapper(Ref<PackedByteArray_RWWrapper> wrapper) {
        return ReaderWriter{wrapper, &PackedBytesVTable};
    }
};

VARIANT_ENUM_CAST(ReaderWriter::SEEK);
VARIANT_ENUM_CAST(ReaderWriter::ERROR);

void ReaderWriter::_bind_methods() {
    BIND_ENUM_CONSTANT(FROM_CURRENT);
    BIND_ENUM_CONSTANT(FROM_END);
    BIND_ENUM_CONSTANT(FROM_START);
    BIND_ENUM_CONSTANT(NONE);
    BIND_ENUM_CONSTANT(INVALID_STATE);
    BIND_ENUM_CONSTANT(OUT_OF_DATA_TO_READ);
    BIND_ENUM_CONSTANT(OUT_OF_SPACE_TO_WRITE);
    BIND_ENUM_CONSTANT(SEEK_ERROR);
    BIND_ENUM_CONSTANT(SEEK_AFTER_DATA_RANGE);
    BIND_ENUM_CONSTANT(SEEK_BEFORE_DATA_RANGE);
    ClassDB::bind_method(D_METHOD("seek_read_pos", "delta", "from"), &ReaderWriter::seek_read_pos);
    ClassDB::bind_method(D_METHOD("seek_write_pos", "delta", "from"), &ReaderWriter::seek_write_pos);
    ClassDB::bind_method(D_METHOD("get_bytes", "delta", "from"), &ReaderWriter::get_bytes);
    ClassDB::bind_method(D_METHOD("read_bytes", "delta", "from"), &ReaderWriter::read_bytes);
}