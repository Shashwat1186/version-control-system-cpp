#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include <chrono>
#include <ctime>
#include "zstr.hpp"
#include <openssl/sha.h>

namespace fs = std::filesystem;

namespace {

std::vector<unsigned char> readFile(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open file: " + path);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::vector<unsigned char> compressZlib(const std::string &data) {
    z_stream zs{};
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) throw std::runtime_error("deflateInit failed");

    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    std::vector<unsigned char> outBuffer;
    unsigned char temp[4096];

    int ret;
    do {
        zs.next_out = temp;
        zs.avail_out = sizeof(temp);
        ret = deflate(&zs, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&zs);
            throw std::runtime_error("deflate failed");
        }
        outBuffer.insert(outBuffer.end(), temp, temp + (sizeof(temp) - zs.avail_out));
    } while (zs.avail_out == 0);

    deflateEnd(&zs);
    return outBuffer;
}

std::string decompressZlib(const std::vector<unsigned char> &data) {
    z_stream zs{};
    zs.next_in = const_cast<Bytef *>(data.data());
    zs.avail_in = static_cast<uInt>(data.size());

    if (inflateInit(&zs) != Z_OK) throw std::runtime_error("inflateInit failed");

    std::vector<char> outBuffer;
    char temp[4096];
    int ret;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(temp);
        zs.avail_out = sizeof(temp);

        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&zs);
            throw std::runtime_error("inflate failed");
        }

        outBuffer.insert(outBuffer.end(), temp, temp + (sizeof(temp) - zs.avail_out));
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);
    return std::string(outBuffer.begin(), outBuffer.end());
}

std::string sha1Hex(const std::string &data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(data.data()), data.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string writeObject(const std::string &type, const std::string &content) {
    std::string header = type + " " + std::to_string(content.size()) + '\0';
    std::string storeData = header + content;
    std::string sha = sha1Hex(storeData);

    std::vector<unsigned char> compressed = compressZlib(storeData);

    std::string dir = ".git/objects/" + sha.substr(0, 2);
    std::filesystem::create_directories(dir);

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
            std::vector<unsigned char> contentVec = readFile(entry.path().string());
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

std::string readAndDecompressObject(std::string_view hash) {
    auto path = std::string(".git/objects/") + std::string(hash.substr(0, 2)) + "/" + std::string(hash.substr(2));

    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open: " + path);

    std::vector<unsigned char> compressed((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return decompressZlib(compressed);
}

std::string getCurrentTimestamp() {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto seconds = system_clock::to_time_t(now);

    std::tm local = *std::localtime(&seconds);
    std::tm utc = *std::gmtime(&seconds);

    std::time_t local_time = std::mktime(&local);
    std::time_t utc_time = std::mktime(&utc);

    long offset = static_cast<long>(std::difftime(local_time, utc_time));

    char sign = offset >= 0 ? '+' : '-';
    offset = std::abs(offset);

    int hours = offset / 3600;
    int mins = (offset % 3600) / 60;

    std::stringstream ss;
    ss << seconds << " "
       << sign
       << std::setw(2) << std::setfill('0') << hours
       << std::setw(2) << mins;

    return ss.str();
}

std::string writeCommitObject(
    const std::string& treeSha,
    const std::string& parentSha,
    const std::string& message)
{
    std::stringstream commit;

    commit << "tree " << treeSha << "\n";

    if (!parentSha.empty())
        commit << "parent " << parentSha << "\n";

    std::string timestamp = getCurrentTimestamp();

    std::string author =
        "Codecrafters <codecrafters@example.com>";

    commit << "author "
           << author
           << " "
           << timestamp
           << "\n";

    commit << "committer "
           << author
           << " "
           << timestamp
           << "\n\n";

    commit << message << "\n";

    return writeObject("commit", commit.str());
}

} // namespace

namespace git {

auto init() -> void {
    try {
        std::filesystem::create_directory(".git");
        std::filesystem::create_directory(".git/objects");
        std::filesystem::create_directory(".git/refs");

        std::ofstream headFile(".git/HEAD");
        if (headFile.is_open()) {
            headFile << "ref: refs/heads/main\n";
        } else {
            std::cerr << "Failed to create .git/HEAD file.\n";
        }

        std::cout << "Initialized git directory\n";
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << e.what() << '\n';
    }
}

auto cat_file(std::string_view hash) -> void {
    try {
        std::string contents = readAndDecompressObject(hash);
        auto header_size = contents.find('\0');
        if (header_size == std::string::npos) {
            throw std::runtime_error("Invalid object contents");
        }
        std::cout << std::string_view(contents.data() + header_size + 1, contents.size() - header_size - 1);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
}

auto hash_object(std::string filename) -> void {
    std::vector<unsigned char> contentVec = readFile(filename);
    std::string content(contentVec.begin(), contentVec.end());
    std::cout << writeObject("blob", content) << "\n";
}

auto ls_tree_name_only(std::string_view hash) -> void {
    try {
        std::string contents = readAndDecompressObject(hash);
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

auto ls_tree(std::string_view hash) -> void {
    try {
        std::string contents = readAndDecompressObject(hash);
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

auto writeTree(const fs::path &dirPath) -> void {
    std::cout << writeTreeObject(dirPath) << "\n";
}

auto commitTree(const std::string& treeSha, const std::string& parentSha,const std::string& message)-> void {
    std::cout<< writeCommitObject(treeSha, parentSha, message)<< '\n';
}

} // namespace git