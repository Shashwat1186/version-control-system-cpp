#include "network.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace git::network {

namespace {

class HttpClient {
public:
    std::vector<unsigned char> get(const std::string &url) const {
        return request(url, nullptr, 0L, false);
    }

    std::vector<unsigned char> post(const std::string &url, const std::string &body) const {
        return request(url, body.c_str(), static_cast<long>(body.size()), true);
    }

private:
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp) {
        size_t realSize = size * nmemb;
        auto *buffer = static_cast<std::vector<unsigned char> *>(userp);
        auto *bytes = static_cast<unsigned char *>(contents);
        buffer->insert(buffer->end(), bytes, bytes + realSize);
        return realSize;
    }

    std::vector<unsigned char> request(const std::string &url, const char *body, long bodySize, bool isPost) const {
        CURL *curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to initialize CURL");

        std::vector<unsigned char> response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        struct curl_slist *headers = nullptr;
        if (isPost) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, bodySize);
            headers = curl_slist_append(headers, "Content-Type: application/x-git-upload-pack-request");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            throw std::runtime_error(isPost ? "CURL POST request failed: " : "CURL request failed: ");
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return response;
    }
};

class RemoteRepository {
public:
    explicit RemoteRepository(std::string baseUrl) : baseUrl_(std::move(baseUrl)) {}

    std::string discoverRefs() const {
        std::string requestUrl = baseUrl_ + "/info/refs?service=git-upload-pack";
        auto response = client_.get(requestUrl);
        std::string respStr(response.begin(), response.end());

        size_t pos = 0;
        std::string targetSha;

        while (pos + 4 <= respStr.size()) {
            std::string lenStr = respStr.substr(pos, 4);
            int len = std::stoi(lenStr, nullptr, 16);

            if (len == 0) {
                pos += 4;
                continue;
            }
            if (pos + len > respStr.size()) break;

            std::string line = respStr.substr(pos + 4, len - 4);
            pos += len;

            if (line.find("HEAD") != std::string::npos && targetSha.empty()) {
                targetSha = line.substr(0, 40);
            } else if (line.find("refs/heads/master") != std::string::npos || line.find("refs/heads/main") != std::string::npos) {
                targetSha = line.substr(0, 40);
            }
        }

        if (targetSha.empty()) {
            throw std::runtime_error("Could not find HEAD or master/main ref");
        }

        return targetSha;
    }

    std::vector<unsigned char> fetchPackfile(const std::string &headSha) const {
        std::string requestUrl = baseUrl_ + "/git-upload-pack";
        std::string body = "0032want " + headSha + "\n0000" + "0009done\n";

        auto response = client_.post(requestUrl, body);

        const unsigned char packSig[] = {'P', 'A', 'C', 'K'};
        auto it = std::search(response.begin(), response.end(), std::begin(packSig), std::end(packSig));
        if (it == response.end()) {
            throw std::runtime_error("Packfile signature 'PACK' not found in response");
        }

        return std::vector<unsigned char>(it, response.end());
    }

private:
    std::string baseUrl_;
    HttpClient client_;
};

} // namespace

std::string discoverRefs(const std::string &url) {
    return RemoteRepository(url).discoverRefs();
}

std::vector<unsigned char> fetchPackfile(const std::string &url, const std::string &head_sha) {
    return RemoteRepository(url).fetchPackfile(head_sha);
}

} // namespace git::network