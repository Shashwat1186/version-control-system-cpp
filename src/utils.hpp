#pragma once

#include <string>
#include <vector>
#include <string_view>

namespace git::utils {

// Reads an entire file into a byte vector
std::vector<unsigned char> readFile(const std::string &path);

// Compresses a string using Zlib
std::vector<unsigned char> compressZlib(const std::string &data);

// Decompresses a Zlib compressed byte vector into a string
std::string decompressZlib(const std::vector<unsigned char> &data);

// Computes the SHA-1 hash of a string and returns it as a hex string
std::string sha1Hex(const std::string &data);

// Generates the current timestamp in Git's format (e.g., "1691234567 +0000")
std::string getCurrentTimestamp();

} // namespace git::utils