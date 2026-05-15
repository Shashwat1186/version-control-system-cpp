#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <zlib.h>

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
  auto path = std::string(".git/objects/") + std::string(hash.substr(0, 2)) +
              "/" + std::string(hash.substr(2));

  zstr::ifstream file(path);
  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

  auto header_size = contents.find('\0') + 1;

  std::cout << std::string_view(contents.begin() + header_size, contents.end());
}

} // namespace git