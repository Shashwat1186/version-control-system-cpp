#pragma once

#include <string>
#include <string_view>
#include <filesystem>

namespace git::objects {

class ObjectStore {
public:
	ObjectStore();
	explicit ObjectStore(std::filesystem::path gitRoot);
	ObjectStore(std::filesystem::path gitRoot, std::filesystem::path objectsRoot);

	std::string readAndDecompressObject(std::string_view hash) const;
	std::string writeObject(const std::string &type, const std::string &content) const;
	std::string writeTreeObject(const std::filesystem::path &dirPath) const;
	std::string writeCommitObject(const std::string &treeSha, const std::string &parentSha, const std::string &message) const;

private:
	std::filesystem::path gitRoot_;
	std::filesystem::path objectsRoot_;

	std::filesystem::path objectPath(std::string_view hash) const;
	std::string readLooseObject(std::string_view hash) const;
	std::string writeLooseObject(const std::string &type, const std::string &content) const;
	std::string writeTreeRecursive(const std::filesystem::path &dirPath) const;
};

// Reads a loose object from .git/objects, decompresses it, and returns the raw contents
std::string readAndDecompressObject(std::string_view hash);

// Formats an object (type + size + \0 + content), hashes it, writes it to disk, and returns the SHA-1
std::string writeObject(const std::string &type, const std::string &content);

// Recursively builds a tree object for a directory, writes it to disk, and returns the SHA-1
std::string writeTreeObject(const std::filesystem::path &dirPath);

// Formats a commit object, writes it to disk, and returns the SHA-1
std::string writeCommitObject(const std::string &treeSha, const std::string &parentSha, const std::string &message);

} // namespace git::objects