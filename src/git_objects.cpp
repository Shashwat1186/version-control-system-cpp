#include "git_objects.hpp"
#include "utils.hpp"
#include <fstream>
#include <sstream>
#include <tuple>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iomanip>

namespace fs = std::filesystem;

namespace git::objects {

std::string readAndDecompressObject(std::string_view hash) {
    auto path = std::string(".git/objects/") + std::string(hash.substr(0, 2)) + "/" + std::string(hash.substr(2));

    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open: " + path);

    std::vector<unsigned char> compressed((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return git::utils::decompressZlib(compressed);
}

std::string writeObject(const std::string &type, const std::string &content) {
    std::string header = type + " " + std::to_string(content.size()) + '\0';
    std::string storeData = header + content;
    std::string sha = git::utils::sha1Hex(storeData);

    std::vector<unsigned char> compressed = git::utils::compressZlib(storeData);

    std::string dir = ".git/objects/" + sha.substr(0, 2);
    fs::create_directories(dir);

    std::string path = dir + "/" + sha.substr(2);
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to write " + type + " object");
    out.write(reinterpret_cast<const char *>(compressed.data()), compressed.size());
    return sha;
}

std::string writeTreeObject(const fs::path &dirPath) {
    std::vector<std::tuple<std::string, std::string, std::string>> entries;

    for (const auto &entry : fs::directory_iterator(dirPath)) {
        if (entry.path().filename() == ".git") continue;

        std::string name = entry.path().filename().string();
        if (entry.is_directory()) {
            std::string sha = writeTreeObject(entry.path());
            entries.push_back({"40000", name, sha});
        } else if (entry.is_regular_file()) {
            std::vector<unsigned char> contentVec = git::utils::readFile(entry.path().string());
            std::string content(contentVec.begin(), contentVec.end());
            std::string sha = writeObject("blob", content);

            fs::perms p = entry.status().permissions();
            std::string mode = (p & fs::perms::owner_exec) != fs::perms::none ? "100755" : "100644";
            entries.push_back({mode, name, sha});
        }
    }

    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
        return std::get<1>(a) < std::get<1>(b);
    });

    std::string data;
    for (const auto &[mode, name, sha] : entries) {
        data += mode + " " + name + '\0';
        for (size_t i = 0; i < 20; ++i) {
            unsigned int byte;
            std::stringstream ss;
            ss << std::hex << sha.substr(i * 2, 2);
            ss >> byte;
            data.push_back(static_cast<char>(byte));
        }
    }

    return writeObject("tree", data);
}

std::string writeCommitObject(const std::string& treeSha, const std::string& parentSha, const std::string& message) {
    std::stringstream commit;

    commit << "tree " << treeSha << "\n";

    if (!parentSha.empty())
        commit << "parent " << parentSha << "\n";

    std::string timestamp = git::utils::getCurrentTimestamp();
    std::string author = "Codecrafters <codecrafters@example.com>";

    commit << "author " << author << " " << timestamp << "\n";
    commit << "committer "<< author << " " << timestamp << "\n\n";
    commit << message << "\n";

    return writeObject("commit", commit.str());
}

} // namespace git::objects