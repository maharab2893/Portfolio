#include "utils.hpp"
#include <fstream>
#include <iomanip>
#include <openssl/sha.h>
#include <sstream>
#include <stdexcept>
#include <zlib.h>

namespace mikit {

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string sha1_hex(const std::string& data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash);

    std::ostringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string compress_data(const std::string& data) {
    uLongf dest_len = compressBound(data.size());
    std::string compressed(dest_len, '\0');

    int res = ::compress(
        reinterpret_cast<Bytef*>(compressed.data()), &dest_len,
        reinterpret_cast<const Bytef*>(data.data()), data.size()
    );
    if (res != Z_OK) {
        throw std::runtime_error("zlib compression failed");
    }

    compressed.resize(dest_len);
    return compressed;
}
std::string hex_to_bytes(const std::string& hex) {
    if (hex.size() != 40) {
        throw std::runtime_error("Invalid SHA-1 hex length: " + std::to_string(hex.size()));
    }

    std::string bytes;
    bytes.reserve(20);
    for (std::size_t i = 0; i < 40; i += 2) {
        unsigned int byte = std::stoul(hex.substr(i, 2), nullptr, 16);
        bytes.push_back(static_cast<char>(byte));
    }
    return bytes;
}
std::string decompress_data(const std::string& data) {
    z_stream zs{};
    if (inflateInit(&zs) != Z_OK) {
        throw std::runtime_error("zlib inflateInit failed");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    std::string out;
    char buffer[16384];
    int ret = Z_OK;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(buffer);
        zs.avail_out = sizeof(buffer);

        ret = inflate(&zs, 0);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&zs);
            throw std::runtime_error("zlib decompression failed");
        }
        out.append(buffer, sizeof(buffer) - zs.avail_out);
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);
    return out;
}

std::string bytes_to_hex(const std::string& bytes) {
    std::ostringstream ss;
    for (unsigned char b : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return ss.str();
}
namespace {

std::string trim_copy(const std::string& s) {
    std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // namespace

std::map<std::string, std::string> load_config() {
    std::map<std::string, std::string> cfg;
    std::ifstream in(".mikit/config");
    if (!in.is_open()) return cfg;

    std::string line;
    std::string section;
    while (std::getline(in, line)) {
        std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            section = trim_copy(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }

        std::size_t eq = trimmed.find('=');
        if (eq != std::string::npos) {
            std::string key = trim_copy(trimmed.substr(0, eq));
            std::string val = trim_copy(trimmed.substr(eq + 1));
            std::string full_key = section.empty() ? key : (section + "." + key);
            cfg[full_key] = val;
        }
    }
    return cfg;
}

void save_config(const std::map<std::string, std::string>& cfg) {
    std::map<std::string, std::map<std::string, std::string>> sections;
    for (const auto& [full_key, val] : cfg) {
        std::size_t dot = full_key.find('.');
        if (dot != std::string::npos) {
            sections[full_key.substr(0, dot)][full_key.substr(dot + 1)] = val;
        } else {
            sections[""][full_key] = val;
        }
    }

    std::ofstream out(".mikit/config", std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("Could not write .mikit/config");
    }

    for (const auto& [sec, entries] : sections) {
        if (!sec.empty()) {
            out << "[" << sec << "]\n";
        }
        for (const auto& [k, v] : entries) {
            out << "\t" << k << " = " << v << "\n";
        }
    }
}

std::string get_config_value(const std::string& key, const std::string& default_val) {
    auto cfg = load_config();
    auto it = cfg.find(key);
    if (it != cfg.end() && !it->second.empty()) {
        return it->second;
    }
    return default_val;
}

}