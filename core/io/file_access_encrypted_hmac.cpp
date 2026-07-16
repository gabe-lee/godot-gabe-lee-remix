/**************************************************************************/
/*  file_access_encrypted_hmac.cpp                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "file_access_encrypted_hmac.h"
// #include "core/string/ustring.h"
#include "core/variant/variant.h"

// #include "core/variant/variant_utility.h"
// #include "core/string/print_string.h"
// #include <cstdint>

// void hash_to_hex(const uint8_t* in, char* out) { //DEBUG
//     static const uint8_t hexDigits[] = "0123456789abcdef";
//     int i = 0; // Initialization
    
//     while (i <= 32) { // Condition check
//         int ii = i * 2;
// 		out[ii] = hexDigits[(in[i] >> 4) & 0x0F];
// 		out[ii + 1] = hexDigits[in[i] & 0x0F];
//         i++;
//     }
// 	out[64] = 0;
// }

bool constant_time_compare(const uint8_t* a, const uint8_t* b, size_t size) {
    uint8_t result = 0;
    for (size_t i = 0; i < size; ++i) {
        result |= (a[i] ^ b[i]);
    }
    return result == 0;
}


struct HMAC_SHA256_Context {
    CryptoCore::SHA256Context inner_sha;
    uint8_t opad[64];

    void init(const Vector<uint8_t>& p_mac_key) {
        uint8_t key_block[64] = {0};
        
        // Handle keys larger than the block size
        if (p_mac_key.size() > 64) {
            CryptoCore::SHA256Context sha;
            sha.start();
            sha.update(p_mac_key.ptr(), p_mac_key.size());
            sha.finish(key_block);
        } else {
            memcpy(key_block, p_mac_key.ptr(), p_mac_key.size());
        }

        uint8_t ipad[64];
        for (int i = 0; i < 64; i++) {
            ipad[i] = key_block[i] ^ 0x36;
            this->opad[i] = key_block[i] ^ 0x5c;
        }

        // Begin the inner hash calculation: SHA256(ipad || ...)
        inner_sha.start();
        inner_sha.update(ipad, 64);
    }

    void update(const uint8_t* p_data, size_t p_len) {
        inner_sha.update(p_data, p_len);
    }

    void finish(uint8_t r_hmac[32]) {
        uint8_t inner_hash[32];
        inner_sha.finish(inner_hash);

        // Finalize outer hash calculation: SHA256(opad || inner_hash)
        CryptoCore::SHA256Context outer_sha;
        outer_sha.start();
        outer_sha.update(opad, 64);
        outer_sha.update(inner_hash, 32);
        outer_sha.finish(r_hmac);
    }
};

CryptoCore::RandomGenerator *FileAccessEncryptedHMAC::_fae_static_rng = nullptr;

void FileAccessEncryptedHMAC::deinitialize() {
	if (_fae_static_rng) {
		memdelete(_fae_static_rng);
		_fae_static_rng = nullptr;
	}
}

Error FileAccessEncryptedHMAC::open_and_parse(Ref<FileAccess> p_base, const Vector<uint8_t> &p_mac_key, const Vector<uint8_t> &p_enc_key, Mode p_mode, bool p_with_magic, const Vector<uint8_t> &p_iv) {
	ERR_FAIL_COND_V_MSG(file.is_valid(), ERR_ALREADY_IN_USE, vformat("Can't open file while another file from path '%s' is open.", file->get_path_absolute()));
	ERR_FAIL_COND_V_MSG(p_enc_key.size() != 32, ERR_INVALID_PARAMETER, "Encryption key must be exactly 32 bytes");
	ERR_FAIL_COND_V_MSG(p_mac_key.size() != 32, ERR_INVALID_PARAMETER, "MAC key must be exactly 32 bytes");

	if (p_enc_key.size() == p_mac_key.size()) {
        bool identical = true;
        for (int i = 0; i < p_enc_key.size(); i++) {
            if (p_enc_key[i] != p_mac_key[i]) {
                identical = false;
                break;
            }
        }
        ERR_FAIL_COND_V_MSG(identical, ERR_INVALID_PARAMETER, "Security Error: Encryption key and MAC key must not be identical.");
    }

	pos = 0;
	eofed = false;
	use_magic = p_with_magic;
	if (p_mode == MODE_WRITE_AES256) {
		data.clear();
		writing = true;
		file = p_base;
		key = p_enc_key;
		mac_key = p_mac_key;
		if (p_iv.is_empty()) {
			iv.resize(16);
			if (unlikely(!_fae_static_rng)) {
				_fae_static_rng = memnew(CryptoCore::RandomGenerator);
				if (_fae_static_rng->init() != OK) {
					memdelete(_fae_static_rng);
					_fae_static_rng = nullptr;
					ERR_FAIL_V_MSG(FAILED, "Failed to initialize random number generator.");
				}
			}
			Error err = _fae_static_rng->get_random_bytes(iv.ptrw(), 16);
			ERR_FAIL_COND_V(err != OK, err);
		} else {
			ERR_FAIL_COND_V(p_iv.size() != 16, ERR_INVALID_PARAMETER);
			iv = p_iv;
		}

	} else if (p_mode == MODE_READ) {
		// 1. Set vars
		writing = false;
		key = p_enc_key;
		mac_key = p_mac_key;

		// 2. Read Magic
		if (use_magic) {
			uint32_t magic = p_base->get_32(); // READ MAGIC
			ERR_FAIL_COND_V(magic != HMAC_ENCRYPTED_HEADER_MAGIC, ERR_FILE_UNRECOGNIZED);
		}

		// 4. Read data Length and the IV
		
		length = p_base->get_64(); // READ LENGTH
		iv.resize(16); 
		p_base->get_buffer(iv.ptrw(), 16); // READ IV

		base = p_base->get_position();
		ERR_FAIL_COND_V(p_base->get_length() < base + length + 32, ERR_FILE_CORRUPT);

		// Calculate the block-aligned ciphertext size
		uint64_t encrypted_data_size = length;
		if (encrypted_data_size % 16) {
			encrypted_data_size += 16 - (encrypted_data_size % 16);
		}
		data.resize(encrypted_data_size);

		// 5. Read the raw Ciphertext from the disk
		uint64_t encrypted_read_len = p_base->get_buffer(data.ptrw(), encrypted_data_size); // READ DATA
		ERR_FAIL_COND_V(encrypted_read_len != encrypted_data_size, ERR_FILE_CORRUPT);

		uint8_t stored_hmac[32];
		p_base->get_buffer(stored_hmac, 32); // READ HMAC
		
		// 6. Verify HMAC in-place (no temporary vector allocations)
		HMAC_SHA256_Context hmac_ctx;
		hmac_ctx.init(mac_key);
		// Feed read length (8 bytes)
		hmac_ctx.update(reinterpret_cast<const uint8_t*>(&length), 8);
		// Feed read IV (16 bytes)
		hmac_ctx.update(iv.ptr(), 16);
 		// Feed read ciphertext (ds bytes) directly from the read buffer
		hmac_ctx.update(data.ptr(), encrypted_data_size);

		uint8_t computed_hmac[32];
		hmac_ctx.finish(computed_hmac);

		// Stop execution immediately if the signature does not match
		if (!constant_time_compare(stored_hmac, computed_hmac, 32)) {
			data.clear();
			ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "HMAC validation failed. File is corrupt or tampered.");
		}

		// 7. Safe Decryption (only runs if the data was fully authenticated)
		{
			CryptoCore::AESContext ctx;
			ctx.set_encode_key(key.ptr(), 256); // Due to the nature of CFB, same key schedule is used for both encryption and decryption!
			ctx.decrypt_cfb(encrypted_data_size, iv.ptrw(), data.ptrw(), data.ptrw());
		}
		data.resize(length); // Trim trailing padding bytes
	}

	return OK;
}

Error FileAccessEncryptedHMAC::open_and_parse_password(Ref<FileAccess> p_base, const String &p_mac_key,  const String &p_key, Mode p_mode) {
	String cs = p_key.md5_text();
	ERR_FAIL_COND_V_MSG(cs.length() != 32, ERR_INVALID_PARAMETER, "Encryption key password must be 32 chars of hexidecimal text");
	Vector<uint8_t> key_md5;
	key_md5.resize(32);
	for (int i = 0; i < 32; i++) {
		key_md5.write[i] = cs[i];
	}

	cs = p_mac_key.md5_text();
	ERR_FAIL_COND_V_MSG(cs.length() != 32, ERR_INVALID_PARAMETER, "MAC key password must be 32 chars of hexidecimal text");
	Vector<uint8_t> mac_md5;
	mac_md5.resize(32);
	for (int i = 0; i < 32; i++) {
		mac_md5.write[i] = cs[i];
	}

	return open_and_parse(p_base, mac_md5, key_md5, p_mode);
}

Error FileAccessEncryptedHMAC::open_internal(const String &p_path, int p_mode_flags) {
	return OK;
}

void FileAccessEncryptedHMAC::_close() {
	if (file.is_null()) {
        return;
    }

    if (writing) {
		uint64_t data_len = data.size();
		uint64_t encrypted_aligned_len = data_len;
		if (encrypted_aligned_len % 16) {
			encrypted_aligned_len += 16 - (encrypted_aligned_len % 16);
		}

		unsigned char _iv[16];
		memcpy(_iv, iv.ptr(), 16);	

		data.resize(encrypted_aligned_len);
		memset(data.ptrw() + data_len, 0, encrypted_aligned_len - data_len);

		CryptoCore::AESContext ctx;
		ctx.set_encode_key(key.ptrw(), 256);

		if (use_magic) {
			file->store_32(HMAC_ENCRYPTED_HEADER_MAGIC);
		}

		file->store_64(data_len);
		file->store_buffer(iv.ptr(), 16);

		ctx.encrypt_cfb(encrypted_aligned_len, _iv, data.ptr(), data.ptrw());

		file->store_buffer(data.ptr(), encrypted_aligned_len);

		HMAC_SHA256_Context hmac_ctx;
        hmac_ctx.init(mac_key);

        // Feed the original data size (8 bytes)
        hmac_ctx.update(reinterpret_cast<const uint8_t*>(&data_len), 8);

        // Feed the IV (16 bytes)
        hmac_ctx.update(iv.ptr(), 16);

        // Feed the encrypted ciphertext
        hmac_ctx.update(data.ptr(), encrypted_aligned_len);

        uint8_t computed_hmac[32];

        hmac_ctx.finish(computed_hmac);
		file->store_buffer(computed_hmac, 32);

		data.clear();
    }

    file.unref();
}

bool FileAccessEncryptedHMAC::is_open() const {
	return file.is_valid();
}

String FileAccessEncryptedHMAC::get_path() const {
	if (file.is_valid()) {
		return file->get_path();
	} else {
		return "";
	}
}

String FileAccessEncryptedHMAC::get_path_absolute() const {
	if (file.is_valid()) {
		return file->get_path_absolute();
	} else {
		return "";
	}
}

void FileAccessEncryptedHMAC::seek(uint64_t p_position) {
	if (p_position > get_length()) {
		p_position = get_length();
	}

	pos = p_position;
	eofed = false;
}

void FileAccessEncryptedHMAC::seek_end(int64_t p_position) {
	seek(get_length() + p_position);
}

uint64_t FileAccessEncryptedHMAC::get_position() const {
	return pos;
}

uint64_t FileAccessEncryptedHMAC::get_length() const {
	return data.size();
}

bool FileAccessEncryptedHMAC::eof_reached() const {
	return eofed;
}

uint64_t FileAccessEncryptedHMAC::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V_MSG(writing, -1, "File has not been opened in read mode.");

	if (!p_length) {
		return 0;
	}

	ERR_FAIL_NULL_V(p_dst, -1);

	uint64_t to_copy = MIN(p_length, get_length() - pos);

	memcpy(p_dst, data.ptr() + pos, to_copy);
	pos += to_copy;

	if (to_copy < p_length) {
		eofed = true;
	}

	return to_copy;
}

Error FileAccessEncryptedHMAC::get_error() const {
	return eofed ? ERR_FILE_EOF : OK;
}

bool FileAccessEncryptedHMAC::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND_V_MSG(!writing, false, "File has not been opened in write mode.");

	if (!p_length) {
		return true;
	}

	ERR_FAIL_NULL_V(p_src, false);

	if (pos + p_length >= get_length()) {
		ERR_FAIL_COND_V(data.resize(pos + p_length) != OK, false);
	}

	memcpy(data.ptrw() + pos, p_src, p_length);
	pos += p_length;

	return true;
}

void FileAccessEncryptedHMAC::flush() {
	ERR_FAIL_COND_MSG(!writing, "File has not been opened in write mode.");

	// encrypted files keep data in memory till close()
}

bool FileAccessEncryptedHMAC::file_exists(const String &p_name) {
	Ref<FileAccess> fa = FileAccess::open(p_name, FileAccess::READ);
	if (fa.is_null()) {
		return false;
	}
	return true;
}

uint64_t FileAccessEncryptedHMAC::_get_modified_time(const String &p_file) {
	if (file.is_valid()) {
		return file->get_modified_time(p_file);
	} else {
		return 0;
	}
}

uint64_t FileAccessEncryptedHMAC::_get_access_time(const String &p_file) {
	if (file.is_valid()) {
		return file->get_access_time(p_file);
	} else {
		return 0;
	}
}

int64_t FileAccessEncryptedHMAC::_get_size(const String &p_file) {
	if (file.is_valid()) {
		return file->get_size(p_file);
	} else {
		return -1;
	}
}

BitField<FileAccess::UnixPermissionFlags> FileAccessEncryptedHMAC::_get_unix_permissions(const String &p_file) {
	if (file.is_valid()) {
		return file->_get_unix_permissions(p_file);
	}
	return 0;
}

Error FileAccessEncryptedHMAC::_set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions) {
	if (file.is_valid()) {
		return file->_set_unix_permissions(p_file, p_permissions);
	}
	return FAILED;
}

bool FileAccessEncryptedHMAC::_get_hidden_attribute(const String &p_file) {
	if (file.is_valid()) {
		return file->_get_hidden_attribute(p_file);
	}
	return false;
}

Error FileAccessEncryptedHMAC::_set_hidden_attribute(const String &p_file, bool p_hidden) {
	if (file.is_valid()) {
		return file->_set_hidden_attribute(p_file, p_hidden);
	}
	return FAILED;
}

bool FileAccessEncryptedHMAC::_get_read_only_attribute(const String &p_file) {
	if (file.is_valid()) {
		return file->_get_read_only_attribute(p_file);
	}
	return false;
}

Error FileAccessEncryptedHMAC::_set_read_only_attribute(const String &p_file, bool p_ro) {
	if (file.is_valid()) {
		return file->_set_read_only_attribute(p_file, p_ro);
	}
	return FAILED;
}

void FileAccessEncryptedHMAC::close() {
	_close();
}

FileAccessEncryptedHMAC::~FileAccessEncryptedHMAC() {
	_close();
}