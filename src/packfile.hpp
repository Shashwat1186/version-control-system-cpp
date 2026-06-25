#pragma once

#include <vector>

namespace git::packfile {

// Parses the binary packfile data, extracts all objects, resolves deltas, 
// and writes them into the local .git/objects database.
void process(const std::vector<unsigned char>& packData);

} // namespace git::packfile