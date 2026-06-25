#pragma once

#include <cstddef>
#include <vector>

namespace git::packfile {

class PackfileProcessor {
public:
	PackfileProcessor();
	explicit PackfileProcessor(bool strictBounds);

	void process(const std::vector<unsigned char> &packData) const;

private:
	bool strictBounds_;
};

// Parses the binary packfile data, extracts all objects, resolves deltas, 
// and writes them into the local .git/objects database.
void process(const std::vector<unsigned char>& packData);

} // namespace git::packfile