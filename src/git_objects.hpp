#pragma once

#include <string>
#include <string_view>
#include <filesystem>

namespace git::objects {

// Reads a loose object from .git/objects, decompresses it, and returns the raw contents
std::string readAndDecompressObject(std::string_view hash);

// Formats an object (type + size + \0 + content), hashes it, writes it to disk, and returns the SHA-1
std::string writeObject(const std::string &type, const std::string &content);

// Recursively builds a tree object for a directory, writes it to disk, and returns the SHA-1
std::string writeTreeObject(const std::filesystem::path &dirPath);

// Formats a commit object, writes it to disk, and returns the SHA-1
std::string writeCommitObject(const std::string &treeSha, const std::string &parentSha, const std::string &message);

} // namespace git::objects