#include "common.h"

#include <bcrypt.h>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <sstream>

#pragma comment(lib, "bcrypt.lib")

// ---------- 宽字符 / UTF-8 ----------

std::string wstring_to_utf8(const std::wstring& s) {
    if (s.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 0) return std::string();
    std::string out(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &out[0], len, NULL, NULL);
    return out;
}

std::wstring utf8_to_wstring(const std::string& s) {
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    if (len <= 0) return std::wstring();
    std::wstring out(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], len);
    return out;
}

std::string file_name_utf8(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    std::wstring name = (pos == std::wstring::npos) ? path : path.substr(pos + 1);
    return wstring_to_utf8(name);
}

// ---------- 路径 ----------

std::wstring join_path(const std::wstring& dir, const std::wstring& name) {
    if (dir.empty()) return name;
    wchar_t last = dir[dir.size() - 1];
    if (last == L'\\' || last == L'/') return dir + name;
    return dir + L"\\" + name;
}

std::wstring get_current_dir_w() {
    DWORD len = GetCurrentDirectoryW(0, NULL);
    if (len == 0) return L".";
    std::vector<wchar_t> buf(len);
    DWORD n = GetCurrentDirectoryW(len, &buf[0]);
    if (n == 0) return L".";
    return std::wstring(&buf[0], n);
}

// ---------- 文件读写 ----------

std::vector<unsigned char> read_file_w(const std::wstring& path) {
    FILE* fp = _wfopen(path.c_str(), L"rb");
    if (!fp) throw std::runtime_error("cannot open file: " + file_name_utf8(path));
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz < 0) { fclose(fp); throw std::runtime_error("cannot get file size"); }
    std::vector<unsigned char> data(static_cast<size_t>(sz));
    if (sz > 0) {
        size_t rd = fread(&data[0], 1, static_cast<size_t>(sz), fp);
        if (rd != static_cast<size_t>(sz)) { fclose(fp); throw std::runtime_error("read file failed"); }
    }
    fclose(fp);
    return data;
}

void write_file_w(const std::wstring& path, const std::string& text, bool binary) {
    FILE* fp = _wfopen(path.c_str(), binary ? L"wb" : L"w");
    if (!fp) throw std::runtime_error("cannot write file: " + file_name_utf8(path));
    if (!text.empty()) {
        size_t wr = fwrite(text.data(), 1, text.size(), fp);
        if (wr != text.size()) { fclose(fp); throw std::runtime_error("write file failed"); }
    }
    fclose(fp);
}

// ---------- 基础工具 ----------

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool is_hex_string(const std::string& s) {
    if (s.empty() || s.size() % 2 != 0) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    std::vector<unsigned char> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        char c1 = hex[i];
        char c2 = hex[i + 1];
        int hi = (c1 >= '0' && c1 <= '9') ? c1 - '0'
               : (c1 >= 'a' && c1 <= 'f') ? c1 - 'a' + 10
               : (c1 >= 'A' && c1 <= 'F') ? c1 - 'A' + 10 : -1;
        int lo = (c2 >= '0' && c2 <= '9') ? c2 - '0'
               : (c2 >= 'a' && c2 <= 'f') ? c2 - 'a' + 10
               : (c2 >= 'A' && c2 <= 'F') ? c2 - 'A' + 10 : -1;
        if (hi < 0 || lo < 0) throw std::runtime_error("invalid hex char");
        out.push_back(static_cast<unsigned char>((hi << 4) | lo));
    }
    return out;
}

std::string bytes_to_hex(const std::vector<unsigned char>& data) {
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    out.reserve(data.size() * 2);
    for (size_t i = 0; i < data.size(); ++i) {
        unsigned char b = data[i];
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 0x0F]);
    }
    return out;
}

std::vector<unsigned char> load_bin_file(const std::wstring& path) {
    std::vector<unsigned char> raw = read_file_w(path);
    std::string content(raw.begin(), raw.end());

    std::string compact;
    compact.reserve(content.size());
    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        if (!std::isspace(static_cast<unsigned char>(c))) compact.push_back(c);
    }
    if (is_hex_string(compact)) return hex_to_bytes(compact);
    return raw;
}

// ---------- 极简 DER 解析 ----------

struct DerReader {
    const unsigned char* p;
    const unsigned char* end;

    DerReader(const unsigned char* data, size_t len) : p(data), end(data + len) {}

    bool eof() const { return p >= end; }

    void read_tlv(unsigned char expected,
                  const unsigned char*& value, size_t& value_len) {
        if (eof()) throw std::runtime_error("DER early end");
        unsigned char tag = *p++;
        if (expected != 0 && tag != expected) throw std::runtime_error("DER tag mismatch");
        if (eof()) throw std::runtime_error("DER length missing");
        size_t len = 0;
        unsigned char first = *p++;
        if ((first & 0x80) == 0) {
            len = first;
        } else {
            int n = first & 0x7F;
            if (n < 1 || n > 4) throw std::runtime_error("DER length unsupported");
            len = 0;
            for (int i = 0; i < n; ++i) {
                if (eof()) throw std::runtime_error("DER length out of range");
                len = (len << 8) | *p++;
            }
        }
        if (len > static_cast<size_t>(end - p)) throw std::runtime_error("DER value out of range");
        value = p;
        value_len = len;
        p += len;
    }

    std::vector<unsigned char> read_integer() {
        const unsigned char* v = NULL;
        size_t n = 0;
        read_tlv(0x02, v, n);
        if (n == 0) throw std::runtime_error("DER INTEGER empty");
        size_t start = 0;
        while (start < n && v[start] == 0x00) ++start;
        if (start == n) return std::vector<unsigned char>(1, 0x00);
        return std::vector<unsigned char>(v + start, v + n);
    }
};

static void parse_spki_public(const std::vector<unsigned char>& der,
                              std::vector<unsigned char>& modulus,
                              std::vector<unsigned char>& exponent) {
    DerReader r(der.data(), der.size());
    const unsigned char* seq = NULL; size_t seq_len = 0;
    r.read_tlv(0x30, seq, seq_len);
    DerReader outer(seq, seq_len);

    const unsigned char* alg = NULL; size_t alg_len = 0;
    outer.read_tlv(0x30, alg, alg_len);
    const unsigned char* bits = NULL; size_t bits_len = 0;
    outer.read_tlv(0x03, bits, bits_len);
    if (bits_len < 1) throw std::runtime_error("BIT STRING empty");

    const unsigned char* rsa_seq_data = bits + 1;
    size_t rsa_seq_len = bits_len - 1;

    DerReader rs(rsa_seq_data, rsa_seq_len);
    const unsigned char* inner = NULL; size_t inner_len = 0;
    rs.read_tlv(0x30, inner, inner_len);
    DerReader pub(inner, inner_len);
    modulus = pub.read_integer();
    exponent = pub.read_integer();
}

static std::vector<unsigned char> extract_pkcs1_from_pkcs8(const std::vector<unsigned char>& der) {
    DerReader r(der.data(), der.size());
    const unsigned char* seq = NULL; size_t seq_len = 0;
    r.read_tlv(0x30, seq, seq_len);
    DerReader outer(seq, seq_len);

    outer.read_integer();
    const unsigned char* alg = NULL; size_t alg_len = 0;
    outer.read_tlv(0x30, alg, alg_len);
    const unsigned char* oct = NULL; size_t oct_len = 0;
    outer.read_tlv(0x04, oct, oct_len);
    return std::vector<unsigned char>(oct, oct + oct_len);
}

struct RsaPrivateParts {
    std::vector<unsigned char> modulus;
    std::vector<unsigned char> pub_exp;
    std::vector<unsigned char> priv_exp;
    std::vector<unsigned char> prime1;
    std::vector<unsigned char> prime2;
    std::vector<unsigned char> exp1;
    std::vector<unsigned char> exp2;
    std::vector<unsigned char> coeff;
};

static RsaPrivateParts parse_pkcs1_private(const std::vector<unsigned char>& pkcs1) {
    DerReader r(pkcs1.data(), pkcs1.size());
    const unsigned char* seq = NULL; size_t seq_len = 0;
    r.read_tlv(0x30, seq, seq_len);
    DerReader s(seq, seq_len);

    s.read_integer();
    RsaPrivateParts parts;
    parts.modulus = s.read_integer();
    parts.pub_exp = s.read_integer();
    parts.priv_exp = s.read_integer();
    parts.prime1 = s.read_integer();
    parts.prime2 = s.read_integer();
    parts.exp1 = s.read_integer();
    parts.exp2 = s.read_integer();
    parts.coeff = s.read_integer();
    return parts;
}

// ---------- CNG BLOB ----------

static std::vector<unsigned char> build_rsa_public_blob(const std::vector<unsigned char>& modulus,
                                                        const std::vector<unsigned char>& exponent) {
    BCRYPT_RSAKEY_BLOB hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.Magic = BCRYPT_RSAPUBLIC_MAGIC;
    hdr.BitLength = static_cast<ULONG>(modulus.size() * 8);
    hdr.cbPublicExp = static_cast<ULONG>(exponent.size());
    hdr.cbModulus = static_cast<ULONG>(modulus.size());

    std::vector<unsigned char> blob(sizeof(hdr));
    memcpy(&blob[0], &hdr, sizeof(hdr));
    blob.insert(blob.end(), exponent.begin(), exponent.end());
    blob.insert(blob.end(), modulus.begin(), modulus.end());
    return blob;
}

static std::vector<unsigned char> build_rsa_private_blob(const RsaPrivateParts& p) {
    BCRYPT_RSAKEY_BLOB hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.Magic = BCRYPT_RSAPRIVATE_MAGIC;
    hdr.BitLength = static_cast<ULONG>(p.modulus.size() * 8);
    hdr.cbPublicExp = static_cast<ULONG>(p.pub_exp.size());
    hdr.cbModulus = static_cast<ULONG>(p.modulus.size());
    hdr.cbPrime1 = static_cast<ULONG>(p.prime1.size());
    hdr.cbPrime2 = static_cast<ULONG>(p.prime2.size());

    std::vector<unsigned char> blob(sizeof(hdr));
    memcpy(&blob[0], &hdr, sizeof(hdr));

    blob.insert(blob.end(), p.pub_exp.begin(), p.pub_exp.end());
    blob.insert(blob.end(), p.modulus.begin(), p.modulus.end());
    blob.insert(blob.end(), p.prime1.begin(), p.prime1.end());
    blob.insert(blob.end(), p.prime2.begin(), p.prime2.end());
    blob.insert(blob.end(), p.exp1.begin(), p.exp1.end());
    blob.insert(blob.end(), p.exp2.begin(), p.exp2.end());
    blob.insert(blob.end(), p.coeff.begin(), p.coeff.end());
    blob.insert(blob.end(), p.priv_exp.begin(), p.priv_exp.end());
    return blob;
}

// ---------- CNG 加解密 ----------

static void check_ntstatus(NTSTATUS st, const char* what) {
    if (st != 0) {
        char buf[256];
        sprintf(buf, "%s failed, NTSTATUS=0x%08lX", what, static_cast<unsigned long>(st));
        throw std::runtime_error(buf);
    }
}

std::vector<unsigned char> cng_encrypt(const std::vector<unsigned char>& pub_der,
                                       const std::string& plaintext) {
    std::vector<unsigned char> modulus, exponent;
    parse_spki_public(pub_der, modulus, exponent);
    std::vector<unsigned char> blob = build_rsa_public_blob(modulus, exponent);

    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_KEY_HANDLE key = NULL;
    check_ntstatus(BCryptOpenAlgorithmProvider(&alg, BCRYPT_RSA_ALGORITHM, NULL, 0),
                   "BCryptOpenAlgorithmProvider");
    check_ntstatus(BCryptImportKeyPair(alg, NULL, BCRYPT_RSAPUBLIC_BLOB, &key,
                                       &blob[0], static_cast<ULONG>(blob.size()), 0),
                   "BCryptImportKeyPair(public)");

    BCRYPT_OAEP_PADDING_INFO oaep;
    memset(&oaep, 0, sizeof(oaep));
    oaep.pszAlgId = BCRYPT_SHA1_ALGORITHM;

    ULONG out_len = 0;
    check_ntstatus(BCryptEncrypt(key,
                                 reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.data())),
                                 static_cast<ULONG>(plaintext.size()),
                                 &oaep, NULL, 0, NULL, 0, &out_len, BCRYPT_PAD_OAEP),
                   "BCryptEncrypt(size)");

    std::vector<unsigned char> out(out_len);
    check_ntstatus(BCryptEncrypt(key,
                                 reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.data())),
                                 static_cast<ULONG>(plaintext.size()),
                                 &oaep, NULL, 0, &out[0], out_len, &out_len, BCRYPT_PAD_OAEP),
                   "BCryptEncrypt");
    out.resize(out_len);

    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    return out;
}

std::vector<unsigned char> cng_decrypt(const std::vector<unsigned char>& pri_der,
                                       const std::vector<unsigned char>& ciphertext) {
    std::vector<unsigned char> pkcs1 = extract_pkcs1_from_pkcs8(pri_der);
    RsaPrivateParts parts = parse_pkcs1_private(pkcs1);
    std::vector<unsigned char> blob = build_rsa_private_blob(parts);

    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_KEY_HANDLE key = NULL;
    check_ntstatus(BCryptOpenAlgorithmProvider(&alg, BCRYPT_RSA_ALGORITHM, NULL, 0),
                   "BCryptOpenAlgorithmProvider");
    check_ntstatus(BCryptImportKeyPair(alg, NULL, BCRYPT_RSAPRIVATE_BLOB, &key,
                                       &blob[0], static_cast<ULONG>(blob.size()), 0),
                   "BCryptImportKeyPair(private)");

    BCRYPT_OAEP_PADDING_INFO oaep;
    memset(&oaep, 0, sizeof(oaep));
    oaep.pszAlgId = BCRYPT_SHA1_ALGORITHM;

    ULONG out_len = 0;
    check_ntstatus(BCryptDecrypt(key,
                                 const_cast<PUCHAR>(&ciphertext[0]),
                                 static_cast<ULONG>(ciphertext.size()),
                                 &oaep, NULL, 0, NULL, 0, &out_len, BCRYPT_PAD_OAEP),
                   "BCryptDecrypt(size)");

    std::vector<unsigned char> out(out_len);
    check_ntstatus(BCryptDecrypt(key,
                                 const_cast<PUCHAR>(&ciphertext[0]),
                                 static_cast<ULONG>(ciphertext.size()),
                                 &oaep, NULL, 0, &out[0], out_len, &out_len, BCRYPT_PAD_OAEP),
                   "BCryptDecrypt");
    out.resize(out_len);

    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    return out;
}

// ---------- ini ----------

std::string read_mag_username(const std::wstring& ini_path) {
    std::vector<unsigned char> raw = read_file_w(ini_path);
    std::string content(raw.begin(), raw.end());

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == ';' || t[0] == '#') continue;
        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));
        if (val.size() >= 2 &&
            ((val[0] == '"' && val[val.size() - 1] == '"') ||
             (val[0] == '\'' && val[val.size() - 1] == '\''))) {
            val = val.substr(1, val.size() - 2);
        }
        if (key == "MagUserName") return val;
    }
    return "";
}