#pragma once

#include <string_view>
namespace git {
auto init() -> void;
auto cat_file(std::string_view file_name) -> void;
auto hash_object(std::string filename) -> void;
auto ls_tree_name_only(std::string_view file_name) -> void;
auto ls_tree(std::string_view file_name) -> void;

} // namespace git