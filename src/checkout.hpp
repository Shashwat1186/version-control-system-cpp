#pragma once
#include <string>

namespace git::checkout {

// Reconstructs the working directory from a commit SHA-1
void workingTree(const std::string& commit_sha);

} // namespace git::checkout