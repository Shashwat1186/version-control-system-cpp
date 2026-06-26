#pragma once
#include <string>

namespace git::checkout {

// Reconstructs the working directory from a commit SHA-1 or branch name
void workingTree(const std::string& target);

} // namespace git::checkout