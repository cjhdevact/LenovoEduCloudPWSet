#include "common.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

static std::wstring parent_dir_of(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L".";
    if (pos == 0) return path.substr(0, 1);
    return path.substr(0, pos);
}

static void print_usage() {
    std::cout <<
        "Usage:\n"
        "  LenovoEduCloudPWSet -e -p <password> [-k <pubkey.bin>] [-o <wfpwd.bin>]\n"
        "      Encrypt. -p required. -k defaults to .\\wfbspubkey.bin, -o to .\\wfpwd.bin\n"
        "  LenovoEduCloudPWSet -d [-i <wfpwd.bin>] [-k <prikey.bin>] [-o <wfpwd.bin>]\n"
        "      Decrypt. -i defaults to .\\wfpwd.bin, -k to .\\wfbsprikey.bin, -o optional\n"
        "  LenovoEduCloudPWSet -u [-c <BSSvrCfg.ini>]\n"
        "      Read MagUserName. -c defaults to .\\BSSvrCfg.ini\n";
}

int wmain(int argc, wchar_t** argv) {
    std::cout << "LenovoEduCloudPWSet " << APP_VERSION << "\n";
    std::cout << "Made by CJH.\n";
    std::cout << "Github: https://github.com/cjhdevact/LenovoEduCloudPWSet\n\n";

    std::vector<std::wstring> wargs;
    for (int i = 0; i < argc; ++i) wargs.push_back(argv[i]);

    std::wstring mode, password, input_file, key_file, output_file, ini_file;

    for (size_t i = 1; i < wargs.size(); ++i) {
        const std::wstring& a = wargs[i];
        auto need_next = [&](const wchar_t* name) -> std::wstring {
            if (i + 1 >= wargs.size()) throw std::runtime_error("missing option value");
            return wargs[++i];
        };
        try {
            if (a == L"-e") mode = L"encrypt";
            else if (a == L"-d") mode = L"decrypt";
            else if (a == L"-u") mode = L"username";
            else if (a == L"-p") password = need_next(L"-p");
            else if (a == L"-i") input_file = need_next(L"-i");
            else if (a == L"-k") key_file = need_next(L"-k");
            else if (a == L"-o") output_file = need_next(L"-o");
            else if (a == L"-c") ini_file = need_next(L"-c");
            else if (a == L"-h" || a == L"--help") { print_usage(); return 0; }
            else { std::cerr << "unknown option\n"; print_usage(); return 1; }
        } catch (const std::exception&) {
            std::cerr << "missing option value\n";
            return 1;
        }
    }

    std::wstring cur = get_current_dir_w();

    try {
        if (mode == L"username") {
            if (ini_file.empty()) ini_file = join_path(cur, L"BSSvrCfg.ini");
            std::string user = read_mag_username(ini_file);
            if (user.empty()) {
                std::cerr << "MagUserName not found in " << file_name_utf8(ini_file) << "\n";
                return 2;
            }
            std::cout << user << std::endl;
            return 0;
        }

        if (mode == L"encrypt") {
            if (password.empty()) { std::cerr << "-e requires -p <password>\n"; return 1; }
            if (key_file.empty()) key_file = join_path(cur, L"wfbspubkey.bin");
            if (output_file.empty()) output_file = join_path(cur, L"wfpwd.bin");

            std::vector<unsigned char> pub_der = load_bin_file(key_file);
            std::string pwd_utf8 = wstring_to_utf8(password);
            std::vector<unsigned char> cipher = cng_encrypt(pub_der, pwd_utf8);
            std::string hex = bytes_to_hex(cipher);
            write_file_w(output_file, hex + "\n", true);

            // 同目录下存在 wfpwdcpy.bin 时一并写入相同密文
            std::wstring out_dir = parent_dir_of(output_file);
            std::wstring copy_path = join_path(out_dir, L"wfpwdcpy.bin");
            bool copy_updated = false;
            if (GetFileAttributesW(copy_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                write_file_w(copy_path, hex + "\n", true);
                copy_updated = true;
            }

            std::cout << "pubkey: " << file_name_utf8(key_file) << "\n";
            std::cout << "password: " << pwd_utf8 << "\n";
            std::cout << "cipher len: " << cipher.size() << " bytes\n";
            std::cout << "cipher hex: " << hex << "\n";
            std::cout << "written: " << file_name_utf8(output_file) << "\n";
            if (copy_updated) {
                std::cout << "written: " << file_name_utf8(copy_path) << "\n";
            }
            return 0;
        }

        if (mode == L"decrypt") {
            if (input_file.empty()) input_file = join_path(cur, L"wfpwd.bin");
            if (key_file.empty()) key_file = join_path(cur, L"wfbsprikey.bin");

            std::vector<unsigned char> pri_der = load_bin_file(key_file);
            std::vector<unsigned char> cipher = load_bin_file(input_file);
            std::vector<unsigned char> plain = cng_decrypt(pri_der, cipher);
            std::string text(plain.begin(), plain.end());

            std::cout << "decrypt ok\n";
            std::cout << "hex: " << bytes_to_hex(plain) << "\n";
            std::cout << "text: " << text << "\n";

            if (!output_file.empty()) {
                write_file_w(output_file, text, true);
                std::cout << "written: " << file_name_utf8(output_file) << "\n";
            }
            return 0;
        }

        std::cerr << "no mode specified, use -e / -d / -u\n";
        print_usage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 2;
    }
}