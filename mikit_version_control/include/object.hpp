#pragma once
#include <map>
#include <string>

namespace mikit {

struct Object {
    std::string type;
    std::string content;
};

std::string hash_blob(const std::string& filepath);
std::string write_object(const std::string& type, const std::string& content);
std::string write_blob(const std::string& filepath);
std::string write_tree(const std::map<std::string, std::string>& index);
std::string write_commit(const std::string& tree_hash,
                         const std::string& parent_hash,
                         const std::string& message);
Object read_object(const std::string& hash_prefix);
std::map<std::string, std::string> read_tree_flat(const std::string& tree_hash);
std::map<std::string, std::string> read_commit_files(const std::string& commit_hash);

} // namespace mikit