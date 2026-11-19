# mikit Architecture & Function Reference

This document details the layered architecture of `mikit`, its on-disk layout, and how every module and function operates internally.

---

## High-Level Architecture

```text
CLI Input (src/main.cpp)
   │  CommandEntry constexpr function-pointer dispatch table
   ▼
Command Layer (src/commands.cpp)
   │  CLI flag parsing, 3-way status diffing, .mikit/index & HEAD management
   ▼
Object Layer (src/object.cpp)
   │  Git object encoding/decoding (blob, tree, commit) & recursive tree traversal
   ▼
Utility Layer (src/utils.cpp)
      Binary file I/O, OpenSSL SHA-1, zlib compress/inflate, hex<->bytes, INI config I/O
```

All internal symbols live inside `namespace mikit` with `#pragma once` headers to avoid symbol collisions with C libraries (`zlib`, `OpenSSL`).

---

## On-Disk Repository Layout (`.mikit/`)

```text
.mikit/
├── HEAD                 # Symbolic ref ("ref: refs/heads/main") or detached commit SHA-1
├── config               # INI-style repository configuration ([user] name/email)
├── index                # Staging area: sorted lines of "<40-char-sha1> <relative-path>"
├── objects/
│   └── <xx>/
│       └── <38-chars>   # zlib-compressed "<type> <size>\0<raw-content>"
└── refs/
    └── heads/
        └── main         # 40-char SHA-1 of the latest commit on branch "main"
```

---

## 1. Entry Point & Routing (`src/main.cpp`)

- **`struct CommandEntry`**: Maps a `std::string_view name` to a command handler `int (*handler)(int argc, char* argv[])`.
- **`main(int argc, char* argv[])`**:
  1. Validates `argc >= 2` (prints usage on failure).
  2. Checks global version flags via `mikit::handle_version_flag()`.
  3. Scans the `constexpr CommandEntry commands[]` table (`init`, `add`, `status`, `commit`, `cat-file`, `log`, `config`) and dispatches to the matching handler.

---

## 2. Version Handling (`src/version.cpp`)

- **`handle_version_flag(int argc, char* argv[])`**: Scans arguments for `--version` or `-v`. If present, prints `MIKIT 1.0.0.0` and returns `true` so `main()` exits with status `0`.

---

## 3. Utility Layer (`src/utils.cpp`)

| Function | Purpose & Internal Mechanics |
|---|---|
| `read_file(path)` | Opens `path` in `std::ios::binary` mode and reads raw bytes into a `std::string`. Throws `std::runtime_error` on failure. |
| `sha1_hex(data)` | Computes a 20-byte digest via OpenSSL's `SHA1()` and formats it as a 40-character lowercase hex string. |
| `compress_data(data)` | Allocates an upper-bound buffer with `compressBound()`, compresses bytes using zlib's `::compress()`, and trims the string to the exact output length. |
| `decompress_data(data)` | Uses zlib's streaming `inflateInit()` / `inflate()` / `inflateEnd()` loop with a 16 KB chunk buffer to handle arbitrarily large objects without knowing their uncompressed size in advance. |
| `hex_to_bytes(hex)` | Converts a 40-character SHA-1 hex string into 20 raw bytes for binary tree serialization. |
| `bytes_to_hex(bytes)` | Converts 20 raw bytes back into a 40-character lowercase hex string when parsing tree objects. |
| `trim_copy(s)` *(internal)* | Strips leading and trailing whitespace (`' '`, `'\t'`, `'\r'`, `'\n'`) for INI parsing. |
| `load_config()` | Parses `.mikit/config` INI sections (`[section]`) and `key = value` pairs into a flat `std::map<std::string, std::string>` (`section.key -> value`). |
| `save_config(cfg)` | Groups flat `section.key` entries by section and rewrites `.mikit/config` in Git's `[section]\n\tkey = value` format. |
| `get_config_value(key, default_val)` | Looks up `key` via `load_config()` and returns its value or `default_val` if missing/empty. |

---

## 4. Object Store & Serialization (`src/object.cpp`)

Every object follows Git's storage envelope: `zlib_compress("<type> <size>\0<content>")`.

| Function | Purpose & Internal Mechanics |
|---|---|
| `hash_blob(filepath)` | Read-only helper used by `cmd_status`. Computes the blob SHA-1 in memory without writing to `.mikit/objects/`. |
| `write_object(type, content)` | Core object writer. Prepends `"<type> <size>\0"`, hashes with `sha1_hex()`, and—if not already present—compresses and writes to `.mikit/objects/<xx>/<38-chars>`. |
| `write_blob(filepath)` | Reads file bytes via `read_file()` and delegates to `write_object("blob", content)`. |
| `write_tree(index)` | Converts the flat `filepath -> sha1` index map into an in-memory `TreeNode` hierarchy and serializes it bottom-up via `write_tree_node()`. |
| `write_tree_node(node)` *(internal)* | Sorts files (`100644`) and subdirectories (`40000`) using Git's sorting rule (directories sorted as `name + "/"`), encodes each entry as `<mode> <name>\0<20-byte-raw-sha1>`, and stores the tree object. |
| `write_commit(tree_hash, parent_hash, message)` | Resolves author/committer (`MIKIT_AUTHOR_*` env vars -> `.mikit/config` -> defaults), attaches Unix timestamp + local timezone (`%z`), formats the commit payload, and writes it via `write_object("commit", body)`. |
| `read_object(hash_prefix)` | Resolves a full 40-char hash or an unambiguous short prefix (4+ chars) in `.mikit/objects/<xx>/`, decompresses with `decompress_data()`, splits `<type> <size>\0<content>`, and returns `Object{type, content}`. |
| `collect_tree_entries(tree_hash, prefix, out)` *(internal)* | Recursively parses binary tree entries (`<mode> <name>\0<20-byte-hash>`), descending into `40000` / `040000` subtrees and populating `out[relative_path] = blob_hex`. |
| `read_tree_flat(tree_hash)` | Public wrapper around `collect_tree_entries()` returning a flat `filepath -> blob_sha1` map for an entire tree snapshot. |
| `read_commit_files(commit_hash)` | Reads a commit object, extracts its `tree <hash>` header, and returns `read_tree_flat(tree_hash)`. |

---

## 5. CLI Command Layer (`src/commands.cpp`)

### `cmd_init`
Creates `.mikit/objects` and `.mikit/refs/heads`. Writes `ref: refs/heads/main\n` to `.mikit/HEAD` only if `HEAD` does not already exist.

### `cmd_add`
- **Helpers:** `is_ignored_dir()` (`.mikit`, `.git`, `build`), `is_ignored_file()` (`mikit`, `mikit.exe`), `stage_file()` (normalizes path via `lexically_normal().generic_string()`, writes blob, updates index only when hash changes).
- **Flow:** Loads `.mikit/index`, stages each target file or recursively walks directories (`recursive_directory_iterator`), and rewrites `.mikit/index` in sorted path order.

### `cmd_status`
Implements a **3-way comparison**:
1. Reads `.mikit/HEAD` to print the current branch name and loads `head_files` via `resolve_head_commit()` + `read_commit_files()`.
2. Loads `.mikit/index`.
3. **HEAD vs. Index (`Changes to be committed`):** Detects `new file` (in index, not HEAD), `modified` (hash differs), and `deleted` (in HEAD, not index).
4. **Index vs. Working Directory (`Changes not staged for commit`):** Uses `hash_blob()` to detect `modified` and `deleted` files on disk.
5. **Working Directory scan (`Untracked files`):** Collects non-ignored files absent from `.mikit/index`.
6. Prints `nothing to commit, working tree clean` when all three lists are empty.

### `cmd_commit`
- **Helper:** `trim_whitespace()` strips trailing newlines/spaces from ref files.
- **Flow:** Parses `-m <message>`, loads `.mikit/index`, resolves `.mikit/HEAD` to its branch ref file and optional `parent_hash`, calls `write_tree()` and `write_commit()`, updates the branch ref file with the new commit hash, and prints `[<branch> <short-hash>] <message>`.

### `cmd_cat_file`
- **Helper:** `print_tree()` decodes binary tree entries into Git's `mode type 40-char-hash\tname` format.
- **Flow:** Loads the object via `read_object(hash)`. Prints `obj.type` for `-t`; pretty-prints trees or outputs raw blob/commit content for `-p`.

### `cmd_log`
- **Helpers:** `resolve_head_commit()` follows `.mikit/HEAD` to the active commit SHA-1; `format_author_date()` converts `<unix_timestamp> <tz>` into a readable local date string.
- **Flow:** Walks `curr_hash -> parent_hash` from `HEAD` to the root commit, printing each commit's hash, author, formatted date, and indented message.

### `cmd_config`
Supports three modes against `.mikit/config`:
- `--list` / `-l`: Prints all `section.key=value` entries via `load_config()`.
- `<section.key>`: Looks up and prints a single key (exits `1` if absent).
- `<section.key> <value>`: Validates the `section.key` format, updates the map, and persists via `save_config()`.