# Build Your Own Git (C++17 OOP Implementation)

This repository contains a custom, from-scratch implementation of Git internals written in modern C++17. Originally started as part of the [CodeCrafters](https://codecrafters.io) "Build Your Own Git" challenge, this project has been heavily refactored into a robust Object-Oriented architecture.

## 🏗️ Architecture & Design

Unlike standard functional approaches, this implementation heavily leverages Object-Oriented Programming (OOP) principles to model Git's internal data structures:

* **Encapsulation:** The repository state, filesystem interactions, and network buffers are securely encapsulated within dedicated management classes, preventing state leakage and ensuring safe resource handling (RAII).
* **Polymorphic Git Objects:** Core Git entities (`Blob`, `Tree`, `Commit`) are modeled as classes, inheriting from a base `GitObject` interface. 
* **Operator Overloading:** Custom operator overloads (e.g., `operator<<` and `operator>>`) are utilized for clean, idiomatic serialization and deserialization of Git objects to and from the Zlib-compressed `.git/objects` database.

## ✨ Features Implemented

This client supports the core lifecycle of a Git repository, including remote interactions via the Smart HTTP protocol.

* `init`: Initialize a new local `.git` directory structure.
* `cat-file -p`: Parse, decompress, and pretty-print the contents of a blob, tree, or commit object.
* `hash-object -w`: Compute the SHA-1 hash of a local file, compress it via Zlib, and write it to the object database.
* `ls-tree`: Read a tree object and list its contents (handles file modes, object types, and SHA-1s).
* `write-tree`: Recursively traverse the current working directory and build a complete Git tree structure.
* `commit-tree`: Construct a commit object linking a tree to its parent commit with an author timestamp and message.
* `clone`: Negotiate with a remote Git server, download a compressed packfile, resolve `OFS_DELTA` and `REF_DELTA` objects, and reconstruct the working tree locally.

## 🛠️ Prerequisites

To build and run this project, you will need:

* A C++17 compatible compiler (GCC, Clang, or MSVC)
* [CMake](https://cmake.org/) (v3.10+)
* **Zlib** (for object compression/decompression)
* **OpenSSL** (for SHA-1 hashing)
* **libcurl** (for Smart HTTP network requests)

## 🚀 Build Instructions

This project uses CMake for its build system. To compile the project locally:

```bash
# Clone the repository
git clone <your-repo-url>
cd <your-repo-directory>

# Create a build directory
mkdir build && cd build

# Configure and compile
cmake ..
make