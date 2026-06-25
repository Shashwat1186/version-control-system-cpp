#include "git_commands.hpp"
#include "git_objects.hpp"
#include "network.hpp"
#include "packfile.hpp"
#include "checkout.hpp"
#include "utils.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string readLooseObject(std::string_view hash) {
    auto path = std::string(".git/objects/") + std::string(hash.substr(0, 2)) + "/" + std::string(hash.substr(2));

    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open: " + path);

    std::vector<unsigned char> compressed((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return git::utils::decompressZlib(compressed);
}

std::string writeLooseObject(const std::string &type, const std::string &content) {
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

std::string writeTreeRecursive(const fs::path &dirPath) {
    std::vector<std::tuple<std::string, std::string, std::string>> entries;

    for (const auto &entry : fs::directory_iterator(dirPath)) {
        if (entry.path().filename() == ".git") continue;

        std::string name = entry.path().filename().string();
        if (entry.is_directory()) {
            entries.push_back({"40000", name, writeTreeRecursive(entry.path())});
        } else if (entry.is_regular_file()) {
            std::vector<unsigned char> contentVec = git::utils::readFile(entry.path().string());
            std::string content(contentVec.begin(), contentVec.end());
            std::string mode = (entry.status().permissions() & fs::perms::owner_exec) != fs::perms::none ? "100755" : "100644";
            entries.push_back({mode, name, writeLooseObject("blob", content)});
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

    return writeLooseObject("tree", data);
}

} // namespace

namespace git::objects {

std::string readAndDecompressObject(std::string_view hash) {
    return readLooseObject(hash);
}

std::string writeObject(const std::string &type, const std::string &content) {
    return writeLooseObject(type, content);
}

std::string writeTreeObject(const std::filesystem::path &dirPath) {
    return writeTreeRecursive(dirPath);
}

std::string writeCommitObject(const std::string &treeSha, const std::string &parentSha, const std::string &message) {
    std::string commitData;
    commitData += "tree " + treeSha + '\n';
    if (!parentSha.empty()) {
        commitData += "parent " + parentSha + '\n';
    }
    std::string timestamp = git::utils::getCurrentTimestamp();
    commitData += "author Codecrafters <codecrafters@example.com> " + timestamp + '\n';
    commitData += "committer Codecrafters <codecrafters@example.com> " + timestamp + "\n\n";
    commitData += message;
    commitData += '\n';

    return writeLooseObject("commit", commitData);
}

} // namespace git::objects

namespace git {

void init() {
    try {
        fs::create_directory(".git");
        fs::create_directory(".git/objects");
        fs::create_directory(".git/refs");

        std::ofstream headFile(".git/HEAD");
        if (headFile.is_open()) {
            headFile << "ref: refs/heads/main\n";
        } else {
            std::cerr << "Failed to create .git/HEAD file.\n";
        }

        std::cout << "Initialized git directory\n";
    } catch (const fs::filesystem_error &e) {
        std::cerr << e.what() << '\n';
    }
}

void cat_file(std::string_view hash) {
    try {
        std::string contents = git::objects::readAndDecompressObject(hash);
        auto header_size = contents.find('\0');
        if (header_size == std::string::npos) {
            throw std::runtime_error("Invalid object contents");
        }
        std::cout << std::string_view(contents.data() + header_size + 1, contents.size() - header_size - 1);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
}

void hash_object(std::string filename) {
    std::vector<unsigned char> contentVec = git::utils::readFile(filename);
    std::string content(contentVec.begin(), contentVec.end());
    std::cout << git::objects::writeObject("blob", content) << "\n";
}

void ls_tree_name_only(std::string_view hash) {
    try {
        std::string contents = git::objects::readAndDecompressObject(hash);
        auto pos = contents.find('\0');
        if (pos == std::string::npos) throw std::runtime_error("Invalid tree object");
        pos += 1;

        while (pos < contents.size()) {
            auto null_pos = contents.find('\0', pos);
            if (null_pos == std::string::npos || null_pos + 21 > contents.size()) break;

            std::string_view entry(contents.data() + pos, null_pos - pos);
            auto space_pos = entry.find(' ');
            std::cout << entry.substr(space_pos + 1) << '\n';
            pos = null_pos + 1 + 20;
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
}

void ls_tree(std::string_view hash) {
    try {
        std::string contents = git::objects::readAndDecompressObject(hash);
        auto pos = contents.find('\0');
        if (pos == std::string::npos) throw std::runtime_error("Invalid tree object");
        pos += 1;

        while (pos < contents.size()) {
            auto null_pos = contents.find('\0', pos);
            if (null_pos == std::string::npos || null_pos + 21 > contents.size()) break;

            std::string_view entry(contents.data() + pos, null_pos - pos);
            auto space_pos = entry.find(' ');

            std::string mode(entry.substr(0, space_pos));
            std::string name(entry.substr(space_pos + 1));

            const unsigned char *sha = reinterpret_cast<const unsigned char *>(contents.data() + null_pos + 1);
            std::stringstream ss;
            for (int i = 0; i < 20; i++) {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(sha[i]);
            }

            std::string type = (mode == "40000") ? "tree" : "blob";
            std::cout << mode << ' ' << type << ' ' << ss.str() << ' ' << name << '\n';
            pos = null_pos + 1 + 20;
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
}

void write_tree(const std::filesystem::path &dirPath) {
    std::cout << git::objects::writeTreeObject(dirPath) << "\n";
}

void commit_tree(const std::string &treeSha, const std::string &parentSha, const std::string &message) {
    std::cout << git::objects::writeCommitObject(treeSha, parentSha, message) << '\n';
}

void clone(const std::string &url, const std::string &dir) {
    std::filesystem::create_directories(dir);
    std::filesystem::current_path(dir);
    init();

    std::string head_sha = git::network::discoverRefs(url);
    std::vector<unsigned char> packfile = git::network::fetchPackfile(url, head_sha);
    git::packfile::process(packfile);
    git::checkout::workingTree(head_sha);
}

} // namespace git