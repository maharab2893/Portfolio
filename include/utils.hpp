#pragma once
#include <map>
#include <string>

namespace mikit {

std::string read_file(const std::string& path);
std::string sha1_hex(const std::string& data);
std::string compress_data(const std::string& data);
std::string decompress_data(const std::string& data);
std::string hex_to_bytes(const std::string& hex);
std::string bytes_to_hex(const std::string& bytes);

std::map<std::string, std::string> load_config();
void save_config(const std::map<std::string, std::string>& cfg);
std::string get_config_value(const std::string& key, const std::string& default_val = "");

} // namespace mikit