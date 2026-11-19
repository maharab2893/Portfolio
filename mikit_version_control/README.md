# mikit

A lightweight, Git-compatible version control system built from scratch in **C++17** to explore Git internals—content-addressable object storage, SHA-1 hashing, zlib compression/decompression, recursive tree serialization, 3-way status comparison, and commit history traversal.

---

## Features

- **`mikit init`** — Initializes a `.mikit/` repository (`objects/`, `refs/heads/`, and `HEAD` pointing to `refs/heads/main`). Safe to re-run without overwriting an existing `HEAD`.
- **`mikit add <file|dir> [...]`** — Computes Git-compatible SHA-1 blob hashes (`blob <size>\0<content>`), compresses objects with `zlib`, stores them in `.mikit/objects/<xx>/<38-char-hash>`, and updates `.mikit/index`. Supports recursive directory staging (`mikit add .`) while ignoring `.mikit/`, `.git/`, `build/`, and the compiled binary.
- **`mikit status`** — Performs a **3-way comparison** across `HEAD`'s commit tree, `.mikit/index`, and the working directory (using read-only hashing) to report staged changes (`new file`, `modified`, `deleted`), unstaged changes (`modified`, `deleted`), and untracked files.
- **`mikit commit -m "<message>"`** — Recursively builds Git-compatible binary tree objects from `.mikit/index`, writes a commit object linked to the current `HEAD` parent (if any), and advances the active branch reference.
- **`mikit cat-file <-p|-t> <hash>`** — Decompresses and inspects stored `blob`, `tree`, and `commit` objects using either full 40-character hashes or unique short prefixes (4+ characters).
- **`mikit log`** — Resolves `HEAD` and walks the parent commit chain backwards, printing commit hashes, author metadata, formatted local timestamps, and commit messages.
- **`mikit config`** — Reads, writes, and lists persistent repository settings stored in `.mikit/config` using Git's `[section]` INI format (`--list`, `<section.key>`, `<section.key> <value>`).
- **`mikit --version` / `-v`** — Prints the current version (`MIKIT 1.0.0.0`).

---

## Tech Stack & Dependencies

- **Language:** C++17 (`std::filesystem`, `std::string_view`)
- **Hashing:** OpenSSL (`libcrypto` — SHA-1)
- **Compression:** zlib (`libz` — `compress` & streaming `inflate`)
- **Build System:** GNU Make + `g++`

### Install Dependencies (Ubuntu / Debian)
```bash
sudo apt update
sudo apt install g++ make libssl-dev zlib1g-dev
```

---

## Project Structure

```text
mikit/
├── include/
│   ├── commands.hpp    # CLI command declarations (init, add, status, commit, cat-file, log, config)
│   ├── object.hpp      # Object model: blobs, trees, commits, and recursive tree reading
│   ├── utils.hpp       # File I/O, SHA-1, zlib compress/decompress, hex/bytes, and INI config I/O
│   └── version.hpp     # Global version flag handling
├── src/
│   ├── main.cpp        # Entry point & CommandEntry function-pointer dispatch table
│   ├── commands.cpp
│   ├── object.cpp
│   ├── utils.cpp
│   └── version.cpp
├── build/              # Compiled object files (generated)
├── ARCHITECTURE.md     # Module & function-by-function internal reference
├── README.md
└── Makefile
```

---

## Build & Usage

```bash
# Build
make

# Initialize repository
./mikit init

# Configure author identity permanently (.mikit/config)
./mikit config user.name "Your Name"
./mikit config user.email "you@example.com"
./mikit config --list

# Stage specific files or the entire working tree
./mikit add structure.md
./mikit add .

# Check 3-way repository status
./mikit status

# Create a commit
./mikit commit -m "initial commit"

# View commit history
./mikit log

# Inspect any commit, tree, or blob (full or short hash)
./mikit cat-file -t 6360564
./mikit cat-file -p 6360564

# Check version
./mikit --version

# Clean build artifacts
make clean
```

### Author Resolution Priority
When creating a commit, `mikit` resolves author/committer identity in this order:
1. Environment variables (`MIKIT_AUTHOR_NAME`, `MIKIT_AUTHOR_EMAIL`)
2. Repository config (`.mikit/config` -> `user.name`, `user.email`)
3. Fallback defaults (`mikit user` / `user@mikit.local`)

### Verify Git Compatibility
Blobs, trees, and commits follow Git's exact header and binary serialization rules:
```bash
./mikit add structure.md
cat .mikit/index
git hash-object structure.md
```
Both 40-character SHA-1 hashes match identically.

---

## Roadmap

- [x] `init`, `add` (single & recursive), `--version`
- [x] `commit -m` — Recursive tree objects, commit objects, and `HEAD` updates
- [x] `cat-file -p / -t` — Object decompression and inspection (full & short hashes)
- [x] `log` — Commit history traversal
- [x] `config` — Persistent `.mikit/config` INI storage (`user.name`, `user.email`)
- [x] 3-way `status` (`HEAD` tree vs. `.mikit/index` vs. working directory)
- [ ] `branch` & `checkout` — Branch management and working-tree switching
- [ ] `diff` — Line-by-line diff between working tree, index, and commits