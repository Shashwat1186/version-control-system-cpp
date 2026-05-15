#pragma once

#include <string_view>
namespace git {
auto init() -> void;
auto cat_file(std::string_view file_name) -> void;
} // namespace git