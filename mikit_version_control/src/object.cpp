#include "object.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace mikit {

std::string hash_blob(const std::string& filepath) {
    std::string content = read_file(filepath);
    std::string store = "blob " + std::to_string(content.size()) + '\0' + content;
    return sha1_hex(store);
}

std::string write_object(const std::string& type, const std::string& content) {
    std::string store = type + " " + std::to_string(content.size()) + '\0' + content;
    std::string hash = sha1_hex(store);

    std::string dir = ".mikit/objects/" + hash.substr(0, 2);
    std::string obj_path = dir + "/" + hash.substr(2);

    if (!fs::exists(obj_path)) {
        fs::create_directories(dir);
        std::string compressed = compress_data(store);

        std::ofstream out(obj_path, std::ios::binary);
        if (!out.is_open()) {
            throw std::runtime_error("Failed to write object: " + obj_path);
        }
        out.write(compressed.data(), static_cast<std::streamsize>(compressed.size()));
    }

    return hash;
}

std::string write_blob(const std::string& filepath) {
    std::string content = read_file(filepath);
    return write_object("blob", content);
}

namespace {

struct TreeNode {
    std::map<std::string, std::string> files; // filename -> 40-char blob hex
    std::map<std::string, TreeNode> dirs;     // dirname  -> subtree
};

struct TreeEntry {
    std::string mode;
    std::string name;
    std::string raw_hash; // 20-byte binary SHA-1
    std::string sort_key; // Git sorts directories as if suffixed with '/'
};

std::string write_tree_node(const TreeNode& node) {
    std::vector<TreeEntry> entries;

    for (const auto& [name, hex_hash] : node.files) {
        entries.push_back({"100644", name, hex_to_bytes(hex_hash), name});
    }

    for (const auto& [name, child] : node.dirs) {
        std::string sub_hash = write_tree_node(child);
        entries.push_back({"40000", name, hex_to_bytes(sub_hash), name + "/"});
    }

    std::sort(entries.begin(), entries.end(),
              [](const TreeEntry& a, const TreeEntry& b) {
                  return a.sort_key < b.sort_key;
              });

    std::string body;
    for (const auto& entry : entries) {
        body += entry.mode + " " + entry.name;
        body.push_back('\0');
        body += entry.raw_hash;
    }

    return write_object("tree", body);
}

void collect_tree_entries(const std::string& tree_hash,
                          const std::string& prefix,
                          std::map<std::string, std::string>& out) {
    Object obj = read_object(tree_hash);
    if (obj.type != "tree") {
        throw std::runtime_error("Expected tree object, got: " + obj.type);
    }

    const std::string& content = obj.content;
    std::size_t pos = 0;

    while (pos < content.size()) {
        std::size_t space = content.find(' ', pos);
        std::size_t null_pos = content.find('\0', space);
        if (space == std::string::npos || null_pos == std::string::npos || null_pos + 21 > content.size()) {
            throw std::runtime_error("Corrupt tree object: " + tree_hash);
        }

        std::string mode = content.substr(pos, space - pos);
        std::string name = content.substr(space + 1, null_pos - space - 1);
        std::string hex_hash = bytes_to_hex(content.substr(null_pos + 1, 20));
        std::string full_path = prefix.empty() ? name : (prefix + "/" + name);

        if (mode == "40000" || mode == "040000") {
            collect_tree_entries(hex_hash, full_path, out);
        } else {
            out[full_path] = hex_hash;
        }

        pos = null_pos + 21;
    }
}

} // namespace

std::string write_tree(const std::map<std::string, std::string>& index) {
    TreeNode root;

    for (const auto& [filepath, hash] : index) {
        std::stringstream ss(filepath);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(ss, part, '/')) {
            if (!part.empty()) {
                parts.push_back(part);
            }
        }
        if (parts.empty()) continue;

        TreeNode* curr = &root;
        for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
            curr = &curr->dirs[parts[i]];
        }
        curr->files[parts.back()] = hash;
    }

    return write_tree_node(root);
}

std::string write_commit(const std::string& tree_hash,
                         const std::string& parent_hash,
                         const std::string& message) {
    const char* env_name = std::getenv("MIKIT_AUTHOR_NAME");
    const char* env_email = std::getenv("MIKIT_AUTHOR_EMAIL");

    std::string author_name = (env_name && *env_name)
        ? env_name
        : get_config_value("user.name", "mikit user");
    std::string author_email = (env_email && *env_email)
        ? env_email
        : get_config_value("user.email", "user@mikit.local");

    std::time_t now = std::time(nullptr);
    char tz[8] = "+0000";
    if (std::tm* lt = std::localtime(&now)) {
        std::strftime(tz, sizeof(tz), "%z", lt);
    }

    std::ostringstream body;
    body << "tree " << tree_hash << "\n";
    if (!parent_hash.empty()) {
        body << "parent " << parent_hash << "\n";
    }
    body << "author " << author_name << " <" << author_email << "> " << now << " " << tz << "\n";
    body << "committer " << author_name << " <" << author_email << "> " << now << " " << tz << "\n";
    body << "\n" << message << "\n";

    return write_object("commit", body.str());
}

Object read_object(const std::string& hash_prefix) {
    if (hash_prefix.size() < 4 || hash_prefix.size() > 40) {
        throw std::runtime_error("Invalid object hash: " + hash_prefix);
    }

    fs::path dir = fs::path(".mikit/objects") / hash_prefix.substr(0, 2);
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        throw std::runtime_error("Object not found: " + hash_prefix);
    }

    std::string rest = hash_prefix.substr(2);
    fs::path obj_path;

    if (hash_prefix.size() == 40) {
        obj_path = dir / rest;
        if (!fs::exists(obj_path)) {
            throw std::runtime_error("Object not found: " + hash_prefix);
        }
    } else {
        std::vector<fs::path> matches;
        for (const auto& entry : fs::directory_iterator(dir)) {
            std::string fname = entry.path().filename().string();
            if (fname.rfind(rest, 0) == 0) {
                matches.push_back(entry.path());
            }
        }
        if (matches.empty()) {
            throw std::runtime_error("Object not found: " + hash_prefix);
        }
        if (matches.size() > 1) {
            throw std::runtime_error("Ambiguous short hash: " + hash_prefix);
        }
        obj_path = matches[0];
    }

    std::string compressed = read_file(obj_path.string());
    std::string raw = decompress_data(compressed);

    std::size_t null_pos = raw.find('\0');
    if (null_pos == std::string::npos) {
        throw std::runtime_error("Corrupt object (missing header terminator): " + hash_prefix);
    }

    std::string header = raw.substr(0, null_pos);
    std::size_t space_pos = header.find(' ');
    if (space_pos == std::string::npos) {
        throw std::runtime_error("Corrupt object header: " + header);
    }

    std::string type = header.substr(0, space_pos);
    std::string content = raw.substr(null_pos + 1);
    return {type, content};
}

std::map<std::string, std::string> read_tree_flat(const std::string& tree_hash) {
    std::map<std::string, std::string> files;
    collect_tree_entries(tree_hash, "", files);
    return files;
}

std::map<std::string, std::string> read_commit_files(const std::string& commit_hash) {
    if (commit_hash.empty()) return {};

    Object obj = read_object(commit_hash);
    if (obj.type != "commit") {
        throw std::runtime_error("Expected commit object, got: " + obj.type);
    }

    std::istringstream ss(obj.content);
    std::string line;
    while (std::getline(ss, line) && !line.empty()) {
        if (line.rfind("tree ", 0) == 0) {
            return read_tree_flat(line.substr(5));
        }
    }

    throw std::runtime_error("Commit missing tree header: " + commit_hash);
}

} // namespace mikit