#pragma once

#include <string>
#include <vector>

namespace git::network {

class RemoteRepository {
public:
	RemoteRepository();
	explicit RemoteRepository(std::string baseUrl);

	std::string discoverRefs() const;
	std::string discoverRefs(const std::string &url) const;
	std::vector<unsigned char> fetchPackfile(const std::string &head_sha) const;
	std::vector<unsigned char> fetchPackfile(const std::string &url, const std::string &head_sha) const;

private:
	std::string baseUrl_;
};

// Discovers remote references using the Git Smart HTTP protocol and returns the SHA-1 of the master/main/HEAD branch.
std::string discoverRefs(const std::string& url);

// Negotiates with the remote server to fetch the raw packfile containing the objects for the given commit SHA-1.
std::vector<unsigned char> fetchPackfile(const std::string& url, const std::string& head_sha);

} // namespace git::network