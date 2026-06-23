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
  file.close();
  auto header_size = contents.find('\0') + 1;

  std::cout << std::string_view(contents.begin() + header_size, contents.end());
}

auto hash_object(std::string filename) -> void {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open: " << filename << '\n';
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    file.close(); 

    std::string blob = "blob " + std::to_string(content.size()) + '\0' + content;

    SHA_CTX shaContext;
    SHA1_Init(&shaContext);
    SHA1_Update(&shaContext, blob.data(), blob.size());

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final(hash, &shaContext);

    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        ss << std::hex
           << std::setw(2)
           << std::setfill('0')
           << static_cast<unsigned int>(hash[i]);
    }
    std::string hashString = ss.str();

    std::cout << hashString << '\n';

    std::string dir_name = hashString.substr(0, 2);
    std::string file_name = hashString.substr(2);
    
    std::filesystem::path target_dir = std::filesystem::path(".git/objects") / dir_name;
    std::filesystem::path newPath = target_dir / file_name;
    
    try {
        std::filesystem::create_directories(target_dir);

        // 2. Allocate a buffer large enough for the compressed data
        uLongf compressed_size = compressBound(blob.size());
        //unsigned char is used because compressed data is binary
        std::vector<unsigned char> compressed_data(compressed_size);

        int zlib_status = compress(compressed_data.data(), &compressed_size, reinterpret_cast<const unsigned char*>(blob.data()), blob.size());
                                   
        if (zlib_status != Z_OK) {
            std::cerr << "Zlib compression failed with error code: " << zlib_status << '\n';
            return;
        }

        std::ofstream outFile(newPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to create object file at: " << newPath << '\n';
            return;
        }
        
        outFile.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_size);
        outFile.close();

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error storing git object: " << e.what() << '\n';
    
    } 
}

auto ls_tree(std::string_view hash) -> void {
  auto path = std::string(".git/objects/") + std::string(hash.substr(0, 2)) + "/" + std::string(hash.substr(2));

  zstr::ifstream file(path);
  if (!file) {
    std::cerr << "Failed to open: " << path << '\n';
    return;
  }
  std::string contents((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
  file.close();
  auto header_size = contents.find('\0') + 1;

  std::cout << std::string_view(contents.begin() + header_size, contents.end());
}

}