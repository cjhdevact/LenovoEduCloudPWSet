#pragma once

#include <windows.h>
#include <string>
#include <vector>

#define APP_VERSION   "1.0.0.26101"
#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
#define APP_VERSION_W WIDEN(APP_VERSION)

// 宽字符 / UTF-8
std::string wstring_to_utf8(const std::wstring& s);
std::wstring utf8_to_wstring(const std::string& s);
std::string file_name_utf8(const std::wstring& path);

// 路径
std::wstring join_path(const std::wstring& dir, const std::wstring& name);
std::wstring get_current_dir_w();

// 文件读写（宽字符路径）
std::vector<unsigned char> read_file_w(const std::wstring& path);
void write_file_w(const std::wstring& path, const std::string& text, bool binary);
std::vector<unsigned char> load_bin_file(const std::wstring& path);

// 基础工具
std::string trim(const std::string& s);
bool is_hex_string(const std::string& s);
std::vector<unsigned char> hex_to_bytes(const std::string& hex);
std::string bytes_to_hex(const std::vector<unsigned char>& data);

// RSA / CNG
std::vector<unsigned char> cng_encrypt(const std::vector<unsigned char>& pub_der,
                                       const std::string& plaintext);
std::vector<unsigned char> cng_decrypt(const std::vector<unsigned char>& pri_der,
                                       const std::vector<unsigned char>& ciphertext);

// ini
std::string read_mag_username(const std::wstring& ini_path);