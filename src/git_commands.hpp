#pragma once

#include <string>
#include <string_view>
#include <filesystem>

namespace git {

void init();
void cat_file(std::string_view hash);
void hash_object(std::string filename);
void ls_tree_name_only(std::string_view hash);
void ls_tree(std::string_view hash);
void write_tree(const std::filesystem::path &dirPath);
void commit_tree(const std::string& treeSha, const std::string& parentSha, const std::string& message);

// New command for the final stage
void clone(const std::string& url, const std::string& dir);

} // namespace git