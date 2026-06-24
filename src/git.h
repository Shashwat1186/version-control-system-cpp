#pragma once

#include <string_view>
#include<filesystem>
namespace fs = std::filesystem;

namespace git {
auto init() -> void;
auto cat_file(std::string_view file_name) -> void;
auto hash_object(std::string filename) -> void;
auto ls_tree_name_only(std::string_view file_name) -> void;
auto ls_tree(std::string_view file_name) -> void;
auto writeTree(const fs::path &dirPath) -> void ;
} // namespace git