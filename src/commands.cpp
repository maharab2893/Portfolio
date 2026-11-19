#include "commands.hpp"
#include "object.hpp"
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include "utils.hpp"
#include <ctime>
#include <sstream>
#include <utility>



namespace fs = std::filesystem;

namespace mikit {

int cmd_init(int /*argc*/, char* /*argv*/[]) {
    bool already_exists = fs::exists(".mikit");

    try {
        fs::create_directories(".mikit/objects");
        fs::create_directories(".mikit/refs/heads");

        if (!fs::exists(".mikit/HEAD")) {
            std::ofstream head(".mikit/HEAD");
            if (!head.is_open()) {
                std::cerr << "Error: Could not create .mikit/HEAD\n";
                return 1;
            }
            head << "ref: refs/heads/main\n";
        }

        if (already_exists) {
            std::cout << "Reinitialized existing mikit repository in "
                      << fs::absolute(".mikit") << "\n";
        } else {
            std::cout << "Initialized empty mikit repository in "
                      << fs::absolute(".mikit") << "\n";
        }
        return 0;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
        return 1;
    }
}

// namespace mikit { er vitore, cmd_add er upore add koro:

static bool is_ignored_dir(const std::string& name) {
    return name == ".mikit" || name == ".git" || name == "build";
}

static bool is_ignored_file(const std::string& name) {
    return name == "mikit" || name == "mikit.exe";
}

static void stage_file(const fs::path& p, std::map<std::string, std::string>& index) {
    std::string rel_path = fs::relative(p, ".").lexically_normal().generic_string();
    std::string hash = write_blob(rel_path);

    if (index[rel_path] != hash) {
        index[rel_path] = hash;
        std::cout << "staged: " << rel_path << " (" << hash.substr(0, 7) << ")\n";
    }
}

int cmd_add(int argc, char* argv[]) {
    if (!fs::exists(".mikit")) {
        std::cerr << "fatal: not a mikit repository (run 'mikit init' first)\n";
        return 1;
    }
    if (argc < 3) {
        std::cerr << "Usage: mikit add <file|dir> [...]\n";
        return 1;
    }

    // Load existing staged entries (filepath -> sha1)
    std::map<std::string, std::string> index;
    if (fs::exists(".mikit/index")) {
        std::ifstream in(".mikit/index");
        std::string hash, path;
        while (in >> hash && std::getline(in >> std::ws, path)) {
            index[path] = hash;
        }
    }

    // Stage files or recursively stage directories (including '.')
    for (int i = 2; i < argc; ++i) {
        fs::path target = argv[i];
        if (!fs::exists(target)) {
            std::cerr << "fatal: pathspec '" << argv[i] << "' did not match any files\n";
            return 1;
        }

        try {
            if (fs::is_regular_file(target)) {
                stage_file(target, index);
            } else if (fs::is_directory(target)) {
                for (auto it = fs::recursive_directory_iterator(target);
                     it != fs::recursive_directory_iterator(); ++it) {
                    std::string name = it->path().filename().string();

                    if (it->is_directory()) {
                        if (is_ignored_dir(name)) {
                            it.disable_recursion_pending();
                        }
                        continue;
                    }

                    if (it->is_regular_file() && !is_ignored_file(name)) {
                        stage_file(it->path(), index);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error staging '" << argv[i] << "': " << e.what() << "\n";
            return 1;
        }
    }

    // Rewrite .mikit/index cleanly
    std::ofstream out(".mikit/index", std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: Could not update .mikit/index\n";
        return 1;
    }
    for (const auto& [path, hash] : index) {
        out << hash << " " << path << "\n";
    }

    return 0;
}
static std::string resolve_head_commit();

int cmd_status(int /*argc*/, char* /*argv*/[]) {
    if (!fs::exists(".mikit")) {
        std::cerr << "fatal: not a mikit repository (run 'mikit init' first)\n";
        return 1;
    }

    // 1. Read current branch from .mikit/HEAD
    std::string branch = "detached HEAD";
    if (std::ifstream head(".mikit/HEAD"); head.is_open()) {
        std::string line;
        std::getline(head, line);
        const std::string prefix = "ref: refs/heads/";
        if (line.rfind(prefix, 0) == 0) {
            branch = line.substr(prefix.size());
        }
    }
    std::cout << "On branch " << branch << "\n\n";

    // 2. Load HEAD commit tree (empty if no commits yet)
    std::map<std::string, std::string> head_files;
    try {
        std::string head_commit = resolve_head_commit();
        if (!head_commit.empty()) {
            head_files = read_commit_files(head_commit);
        }
    } catch (const std::exception& e) {
        std::cerr << "fatal: could not read HEAD tree: " << e.what() << "\n";
        return 1;
    }

    // 3. Load staged files from .mikit/index
    std::map<std::string, std::string> index;
    if (fs::exists(".mikit/index")) {
        std::ifstream in(".mikit/index");
        std::string hash, path;
        while (in >> hash && std::getline(in >> std::ws, path)) {
            index[path] = hash;
        }
    }

    // 4. Compare HEAD vs. Index (Changes to be committed)
    std::vector<std::pair<std::string, std::string>> staged_changes;
    for (const auto& [path, idx_hash] : index) {
        auto it = head_files.find(path);
        if (it == head_files.end()) {
            staged_changes.emplace_back("new file", path);
        } else if (it->second != idx_hash) {
            staged_changes.emplace_back("modified", path);
        }
    }
    for (const auto& [path, head_hash] : head_files) {
        (void)head_hash;
        if (index.find(path) == index.end()) {
            staged_changes.emplace_back("deleted", path);
        }
    }
    std::sort(staged_changes.begin(), staged_changes.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    // 5. Compare Index vs. Working Directory (Changes not staged for commit)
    std::vector<std::string> unstaged_modified;
    std::vector<std::string> unstaged_deleted;

    for (const auto& [path, idx_hash] : index) {
        if (!fs::exists(path)) {
            unstaged_deleted.push_back(path);
        } else {
            try {
                if (hash_blob(path) != idx_hash) {
                    unstaged_modified.push_back(path);
                }
            } catch (const std::exception& e) {
                std::cerr << "Warning: could not read '" << path << "': " << e.what() << "\n";
            }
        }
    }

    // 6. Scan working directory for untracked files
    std::vector<std::string> untracked;
    for (auto it = fs::recursive_directory_iterator(".");
         it != fs::recursive_directory_iterator(); ++it) {
        std::string name = it->path().filename().string();

        if (it->is_directory()) {
            if (is_ignored_dir(name)) {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (is_ignored_file(name)) continue;

        if (it->is_regular_file()) {
            std::string rel_path = fs::relative(it->path(), ".").lexically_normal().generic_string();
            if (index.find(rel_path) == index.end()) {
                untracked.push_back(rel_path);
            }
        }
    }
    std::sort(untracked.begin(), untracked.end());

    // 7. Print status output
    if (!staged_changes.empty()) {
        std::cout << "Changes to be committed:\n";
        for (const auto& [kind, path] : staged_changes) {
            std::cout << "  " << kind << ":   " << path << "\n";
        }
        std::cout << "\n";
    }

    if (!unstaged_modified.empty() || !unstaged_deleted.empty()) {
        std::cout << "Changes not staged for commit:\n";
        for (const auto& f : unstaged_modified) {
            std::cout << "  modified: " << f << "\n";
        }
        for (const auto& f : unstaged_deleted) {
            std::cout << "  deleted:  " << f << "\n";
        }
        std::cout << "\n";
    }

    if (!untracked.empty()) {
        std::cout << "Untracked files:\n";
        for (const auto& f : untracked) {
            std::cout << "  " << f << "\n";
        }
        std::cout << "\n";
    }

    if (staged_changes.empty() && unstaged_modified.empty() &&
        unstaged_deleted.empty() && untracked.empty()) {
        std::cout << "nothing to commit, working tree clean\n";
    }

    return 0;
}
static std::string trim_whitespace(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) {
        s.pop_back();
    }
    return s;
}

int cmd_commit(int argc, char* argv[]) {
    if (!fs::exists(".mikit")) {
        std::cerr << "fatal: not a mikit repository (run 'mikit init' first)\n";
        return 1;
    }

    // 1. Parse -m flag
    std::string message;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-m" && i + 1 < argc) {
            message = argv[++i];
        }
    }

    if (message.empty()) {
        std::cerr << "Usage: mikit commit -m \"<message>\"\n";
        return 1;
    }

    // 2. Load staged entries from .mikit/index
    std::map<std::string, std::string> index;
    if (fs::exists(".mikit/index")) {
        std::ifstream in(".mikit/index");
        std::string hash, path;
        while (in >> hash && std::getline(in >> std::ws, path)) {
            index[path] = hash;
        }
    }

    if (index.empty()) {
        std::cerr << "nothing to commit (staging area is empty)\n";
        return 1;
    }

    try {
        // 3. Resolve HEAD -> branch ref & optional parent commit
        std::string head_content;
        if (std::ifstream head(".mikit/HEAD"); head.is_open()) {
            std::getline(head, head_content);
            head_content = trim_whitespace(head_content);
        }

        fs::path ref_path = ".mikit/HEAD";
        std::string branch = "detached HEAD";
        const std::string ref_prefix = "ref: ";

        if (head_content.rfind(ref_prefix, 0) == 0) {
            std::string rel_ref = head_content.substr(ref_prefix.size());
            ref_path = fs::path(".mikit") / rel_ref;

            const std::string heads_prefix = "refs/heads/";
            if (rel_ref.rfind(heads_prefix, 0) == 0) {
                branch = rel_ref.substr(heads_prefix.size());
            } else {
                branch = rel_ref;
            }
        }

        std::string parent_hash;
        if (ref_path == ".mikit/HEAD") {
            parent_hash = head_content;
        } else if (fs::exists(ref_path)) {
            std::ifstream ref_in(ref_path);
            std::getline(ref_in, parent_hash);
            parent_hash = trim_whitespace(parent_hash);
        }

        // 4. Write tree and commit objects
        std::string tree_hash = write_tree(index);
        std::string commit_hash = write_commit(tree_hash, parent_hash, message);

        // 5. Update branch ref (or detached HEAD)
        if (ref_path.has_parent_path()) {
            fs::create_directories(ref_path.parent_path());
        }
        std::ofstream ref_out(ref_path, std::ios::trunc);
        if (!ref_out.is_open()) {
            std::cerr << "Error: Could not update " << ref_path << "\n";
            return 1;
        }
        ref_out << commit_hash << "\n";

        std::cout << "[" << branch
                  << (parent_hash.empty() ? " (root-commit) " : " ")
                  << commit_hash.substr(0, 7) << "] " << message << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Commit failed: " << e.what() << "\n";
        return 1;
    }
}
static void print_tree(const std::string& content) {
    std::size_t pos = 0;
    while (pos < content.size()) {
        std::size_t space = content.find(' ', pos);
        std::size_t null_pos = content.find('\0', space);
        if (space == std::string::npos || null_pos == std::string::npos || null_pos + 21 > content.size()) {
            throw std::runtime_error("Corrupt tree object");
        }

        std::string mode = content.substr(pos, space - pos);
        std::string name = content.substr(space + 1, null_pos - space - 1);
        std::string raw_hash = content.substr(null_pos + 1, 20);

        std::string entry_type = (mode == "40000" || mode == "040000") ? "tree" : "blob";
        if (mode == "40000") mode = "040000";

        std::cout << mode << " " << entry_type << " "
                  << bytes_to_hex(raw_hash) << "\t" << name << "\n";

        pos = null_pos + 21;
    }
}

int cmd_cat_file(int argc, char* argv[]) {
    if (!fs::exists(".mikit")) {
        std::cerr << "fatal: not a mikit repository (run 'mikit init' first)\n";
        return 1;
    }
    if (argc < 4) {
        std::cerr << "Usage: mikit cat-file <-p|-t> <hash>\n";
        return 1;
    }

    std::string flag = argv[2];
    std::string hash = argv[3];

    try {
        Object obj = read_object(hash);

        if (flag == "-t") {
            std::cout << obj.type << "\n";
        } else if (flag == "-p") {
            if (obj.type == "tree") {
                print_tree(obj.content);
            } else {
                std::cout << obj.content;
            }
        } else {
            std::cerr << "Unknown flag: " << flag << " (use -p or -t)\n";
            return 1;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}

static std::string resolve_head_commit() {
    if (!fs::exists(".mikit/HEAD")) return "";

    std::ifstream head(".mikit/HEAD");
    std::string line;
    std::getline(head, line);
    line = trim_whitespace(line);

    const std::string ref_prefix = "ref: ";
    if (line.rfind(ref_prefix, 0) == 0) {
        fs::path ref_path = fs::path(".mikit") / line.substr(ref_prefix.size());
        if (!fs::exists(ref_path)) return "";

        std::ifstream ref_in(ref_path);
        std::string commit_hash;
        std::getline(ref_in, commit_hash);
        return trim_whitespace(commit_hash);
    }

    return line; // detached HEAD
}

static std::string format_author_date(const std::string& author_line, std::string& out_author) {
    // Format: "Name <email> <unix_timestamp> <tz>"
    std::size_t last_space = author_line.rfind(' ');
    if (last_space == std::string::npos) {
        out_author = author_line;
        return "";
    }
    std::string tz = author_line.substr(last_space + 1);

    std::size_t ts_space = author_line.rfind(' ', last_space - 1);
    if (ts_space == std::string::npos) {
        out_author = author_line;
        return "";
    }

    out_author = author_line.substr(0, ts_space);
    std::string ts_str = author_line.substr(ts_space + 1, last_space - ts_space - 1);

    try {
        std::time_t ts = static_cast<std::time_t>(std::stoll(ts_str));
        char buf[64];
        if (std::tm* lt = std::localtime(&ts)) {
            std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", lt);
            return std::string(buf) + " " + tz;
        }
    } catch (...) {
    }
    return ts_str + " " + tz;
}

int cmd_log(int /*argc*/, char* /*argv*/[]) {
    if (!fs::exists(".mikit")) {
        std::cerr << "fatal: not a mikit repository (run 'mikit init' first)\n";
        return 1;
    }

    std::string curr_hash = resolve_head_commit();
    if (curr_hash.empty()) {
        std::cerr << "fatal: your current branch does not have any commits yet\n";
        return 1;
    }

    try {
        while (!curr_hash.empty()) {
            Object obj = read_object(curr_hash);
            if (obj.type != "commit") {
                std::cerr << "fatal: object " << curr_hash << " is not a commit\n";
                return 1;
            }

            std::istringstream ss(obj.content);
            std::string line;
            std::string parent_hash;
            std::string author_raw;

            // Parse commit headers until blank line
            while (std::getline(ss, line) && !line.empty()) {
                if (line.rfind("parent ", 0) == 0) {
                    parent_hash = line.substr(7);
                } else if (line.rfind("author ", 0) == 0) {
                    author_raw = line.substr(7);
                }
            }

            // Remaining text is the commit message
            std::string message;
            std::string msg_line;
            while (std::getline(ss, msg_line)) {
                message += "    " + msg_line + "\n";
            }

            std::string author_name;
            std::string date_str = format_author_date(author_raw, author_name);

            std::cout << "commit " << curr_hash << "\n";
            if (!author_name.empty()) {
                std::cout << "Author: " << author_name << "\n";
            }
            if (!date_str.empty()) {
                std::cout << "Date:   " << date_str << "\n";
            }
            std::cout << "\n" << message << "\n";

            curr_hash = parent_hash;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
int cmd_config(int argc, char* argv[]) {
    if (!fs::exists(".mikit")) {
        std::cerr << "fatal: not a mikit repository (run 'mikit init' first)\n";
        return 1;
    }
    if (argc < 3) {
        std::cerr << "Usage:\n"
                  << "  mikit config --list\n"
                  << "  mikit config <section.key>\n"
                  << "  mikit config <section.key> <value>\n";
        return 1;
    }

    std::string arg1 = argv[2];

    // List all config entries
    if (arg1 == "--list" || arg1 == "-l") {
        auto cfg = load_config();
        for (const auto& [k, v] : cfg) {
            std::cout << k << "=" << v << "\n";
        }
        return 0;
    }

    // Get a single key
    if (argc == 3) {
        auto cfg = load_config();
        auto it = cfg.find(arg1);
        if (it == cfg.end()) return 1;
        std::cout << it->second << "\n";
        return 0;
    }

    // Set section.key = value
    std::size_t dot = arg1.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 == arg1.size()) {
        std::cerr << "error: key does not contain a section: " << arg1
                  << " (use format like user.name)\n";
        return 1;
    }

    try {
        auto cfg = load_config();
        cfg[arg1] = argv[3];
        save_config(cfg);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

}