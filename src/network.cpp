#include "network.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace git::network {

namespace {
    // libcurl write callback to append downloaded data into a std::vector<unsigned char>
    size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t realsize = size * nmemb;
        auto* mem = static_cast<std::vector<unsigned char>*>(userp);
        auto* ptr = static_cast<unsigned char*>(contents);
        mem->insert(mem->end(), ptr, ptr + realsize);
        return realsize;
    }

    std::vector<unsigned char> httpGet(const std::string& url) {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to initialize CURL");

        std::vector<unsigned char> response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
        }

        curl_easy_cleanup(curl);
        return response;
    }

    std::vector<unsigned char> httpPost(const std::string& url, const std::string& body) {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to initialize CURL");

        std::vector<unsigned char> response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/x-git-upload-pack-request");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            throw std::runtime_error("CURL POST request failed: " + std::string(curl_easy_strerror(res)));
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return response;
    }
} // namespace

std::string discoverRefs(const std::string& url) {
    std::string request_url = url + "/info/refs?service=git-upload-pack";
    auto response = httpGet(request_url);
    std::string resp_str(response.begin(), response.end());

    // Parse pkt-lines to find HEAD or master
    // Format of a line: 4 hex bytes length, then data.
    size_t pos = 0;
    std::string target_sha = "";
    
    while (pos + 4 <= resp_str.size()) {
        std::string len_str = resp_str.substr(pos, 4);
        int len = std::stoi(len_str, nullptr, 16);
        
        if (len == 0) { // flush packet
            pos += 4;
            continue;
        }
        if (pos + len > resp_str.size()) break;

        std::string line = resp_str.substr(pos + 4, len - 4);
        pos += len;

        // Capture the first valid HEAD or master branch SHA-1
        if (line.find("HEAD") != std::string::npos && target_sha.empty()) {
            target_sha = line.substr(0, 40);
        } else if (line.find("refs/heads/master") != std::string::npos || 
                   line.find("refs/heads/main") != std::string::npos) {
            target_sha = line.substr(0, 40);
        }
    }
    
    if (target_sha.empty()) {
        throw std::runtime_error("Could not find HEAD or master/main ref");
    }
    
    return target_sha;
}

std::vector<unsigned char> fetchPackfile(const std::string& url, const std::string& head_sha) {
    std::string request_url = url + "/git-upload-pack";
    
    // Pkt-line format: Length in hex (4 bytes) + data
    // 0032 (hex) = 50 bytes total -> "0032" + "want " + 40 char sha1 + "\n"
    std::string want_pkt = "0032want " + head_sha + "\n";
    std::string flush_pkt = "0000";
    std::string done_pkt = "0009done\n";
    
    std::string body = want_pkt + flush_pkt + done_pkt;

    auto response = httpPost(request_url, body);

    // Git servers often prefix the packfile with pkt-lines like "NAK" or multiplex bands. 
    // The most robust way to find the packfile is to search for the "PACK" signature (0x50 0x41 0x43 0x4B).
    const unsigned char pack_sig[] = {'P', 'A', 'C', 'K'};
    auto it = std::search(response.begin(), response.end(), std::begin(pack_sig), std::end(pack_sig));
    
    if (it == response.end()) {
        throw std::runtime_error("Packfile signature 'PACK' not found in response");
    }

    return std::vector<unsigned char>(it, response.end());
}

} // namespace git::network