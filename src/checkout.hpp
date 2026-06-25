#pragma once

#include <string>
#include <filesystem>

namespace git::checkout {

class CheckoutEngine {
public:
	CheckoutEngine();
	explicit CheckoutEngine(std::filesystem::path workingRoot);

	void workingTree(const std::string &commit_sha) const;

private:
	std::filesystem::path workingRoot_;

	void processTree(const std::string &treeSha, const std::filesystem::path &currentDir) const;
};

// Reconstructs the working directory from a commit SHA-1
void workingTree(const std::string& commit_sha);

} // namespace git::checkout