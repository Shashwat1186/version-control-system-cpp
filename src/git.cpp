#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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

auto hash_object(std::string filename){
  std::ifstream file(filename, std::ios::binary);
  if (!file) {
    std::cerr << "Failed to open: " << path << '\n';
    return;
  }
  SHA_CTX shaContext;
  SHA1_Init(&shaContext);
  char buffer[4096];
  size_t bytesRead;
  while(file.read(buffer, sizeof(buffer)) || file.gcount()>0){
    SHA1_Update(&shaContext, reinterpret_cast<unsigned char*>(buffer), file.gcount());
  }
  unsigned char hash[20];
  SHA1_Final(hash, &shaContext);
  file.close();
  std::stringstram ss;
  for(int i = 0 ; i<20; i++){
    ss<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)hash[i];
  }
  std::string hashString = ss.str();
  std::cout<< hashString << endl;
}

} // namespace git