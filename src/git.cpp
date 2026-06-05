#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <string_view>
#include "zstr.hpp"
#include <openssl/sha.h>
namespace git {

auto init() -> void {
  try {
    std::filesystem::create_directory(".git");
    std::filesystem::create_directory(".git/objects");
    std::filesystem::create_directory(".git/refs");

    std::ofstream headFile(".git/HEAD");

    if (headFile.is_open()) {
      headFile << "ref: refs/heads/main\n";
      headFile.close();
    } else {
      std::cerr << "Failed to create .git/HEAD file.\n";
    }

    std::cout << "Initialized git directory\n";
  } catch (const std::filesystem::filesystem_error &e) {
    std::cerr << e.what() << '\n';
  }
}

auto cat_file(std::string_view hash) -> void {
  auto path = std::string(".git/objects/") + std::string(hash.substr(0, 2)) + "/" + std::string(hash.substr(2));

  zstr::ifstream file(path);
  if (!file) {
    std::cerr << "Failed to open: " << path << '\n';
    return;
  }
  std::string contents((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());

  auto header_size = contents.find('\0') + 1;

  std::cout << std::string_view(contents.begin() + header_size, contents.end());
}

auto hash_object(std::string filename) -> void {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open: " << filename << '\n';
        return;
    }

    // Read entire file content
    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    file.close(); // Close it as soon as we are done reading

    // Git blob format: "blob <size>\0<content>"
    std::string blob = "blob " + std::to_string(content.size()) + '\0' + content;

    // Calculate SHA-1 Hash
    SHA_CTX shaContext;
    SHA1_Init(&shaContext);
    SHA1_Update(&shaContext, blob.data(), blob.size());

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final(hash, &shaContext);

    // Convert hash bytes to a 40-character hex string
    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        ss << std::hex
           << std::setw(2)
           << std::setfill('0')
           << static_cast<unsigned int>(hash[i]);
    }
    std::string hashString = ss.str();

    // Print the calculated hash to standard output (required by CodeCrafters/Git)
    std::cout << hashString << '\n';

    // Build the Git directory layout paths
    std::string dir_prefix = hashString.substr(0, 2);
    std::string file_suffix = hashString.substr(2);
    
    std::filesystem::path target_dir = std::filesystem::path(".git/objects") / dir_prefix;
    std::filesystem::path newPath = target_dir / file_suffix;
    
    try {
        // 1. Ensure the nested subfolder (e.g. .git/objects/88) exists before writing
        std::filesystem::create_directories(target_dir);

        // 2. Compress and write the blob to disk using zstr
        zstr::ofstream outFile(newPath.string());
        if (!outFile) {
            std::cerr << "Failed to create object file at: " << newPath << '\n';
            return;
        }
        
        outFile << blob;
        outFile.close();

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error storing git object: " << e.what() << '\n';
    }
}
}