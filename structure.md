mikit/
├── include/
│   ├── commands.hpp     # init, hash-object, cat-file, etc.
│   ├── object.hpp       # blob, tree, commit handling
│   └── utils.hpp        # SHA-1, zlib, file I/O helpers
├── src/
│   ├── main.cpp         # CLI argument parser (argc/argv)
│   ├── commands.cpp
│   ├── object.cpp
│   └── utils.cpp
├── Makefile
└── README.md