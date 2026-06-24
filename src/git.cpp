#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <string_view>
#include "zstr.hpp"
#include <openssl/sha.h>

namespace fs = std::filesystem;
std::vector<unsigned char> readFile(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open file: " + path);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
}

std::vector<unsigned char> compressZlib(const std::string &data) {
    z_stream zs{};
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) throw std::runtime_error("deflateInit failed");

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
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
    zs.next_in = const_cast<Bytef*>(data.data());
    zs.avail_in = static_cast<uInt>(data.size());

    if (inflateInit(&zs) != Z_OK) throw std::runtime_error("inflateInit failed");

    std::vector<char> outBuffer;
    char temp[4096];
    int ret;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(temp);
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
    SHA1(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

namespace git {

auto init() -> void {
  try {
    std::filesystem::create_directory(".git");
    std::filesystem::create_directory(".git/objects");
    std::filesystem::create_directory(".git/refs");

    std::ofstream headFile(".git/HEAD");

    if (headFile.is_open()) {
      headFile << "ref: refs/heads/main\n";
      headFile.close();
    } else {
      std::cerr << "Failed to create .git/HEAD file.\n";
    }

    std::cout << "Initialized git directory\n";
  } catch (const std::filesystem::filesystem_error &e) {
    std::cerr << e.what() << '\n';
  }
}

auto cat_file(std::string_view hash) -> void {
  auto path = std::string(".git/objects/") + std::string(hash.substr(0, 2)) + "/" + std::string(hash.substr(2));

  zstr::ifstream file(path);
  if (!file) {
    std::cerr << "Failed to open: " << path << '\n';
    return;
  }
  std::string contents((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
  file.close();
  auto header_size = contents.find('\0') + 1;

  std::cout << std::string_view(contents.begin() + header_size, contents.end());
}

auto hash_object(std::string filename) -> void {
    std::vector<unsigned char> contentVec = readFile(fileArg);
    std::string content(contentVec.begin(), contentVec.end());

    std::string header = "blob " + std::to_string(content.size()) + '\0';
    std::string storeData = header + content;
    std::string sha = sha1Hex(storeData);

    std::vector<unsigned char> compressed = compressZlib(storeData);

    std::string dir = ".git/objects/" + sha.substr(0, 2);
    std::filesystem::create_directories(dir);

    std::string path = dir + "/" + sha.substr(2);
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to write blob object");
    out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
    out.close();

    std::cout << sha << "\n";
}

auto ls_tree_name_only(std::string_view hash) -> void {
  auto path = std::string(".git/objects/") + std::string(hash.substr(0, 2)) + "/" + std::string(hash.substr(2));

  std::vector<unsigned char> contentVec = readFile(fileArg);
  std::string contents(contentVec.begin(), contentVec.end());
  auto pos = contents.find('\0') + 1;  // skip "tree <size>\0"

    while (pos < contents.size()) {
        auto null_pos = contents.find('\0', pos);

        // "40000 banana"
        std::string_view entry(
            contents.data() + pos,
            null_pos - pos
        );

        auto space_pos = entry.find(' ');
        std::string_view name = entry.substr(space_pos + 1);

        std::cout << name << '\n';

        // skip '\0' + 20-byte SHA1
        pos = null_pos + 1 + 20;
    }
}

auto ls_tree(std::string_view hash) -> void {
  auto path = std::string(".git/objects/") + std::string(hash.substr(0, 2)) + "/" + std::string(hash.substr(2));

  zstr::ifstream file(path);
  if (!file) {
    std::cerr << "Failed to open: " << path << '\n';
    return;
  }
  std::string contents((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
  file.close();
  auto pos = contents.find('\0') + 1;

while (pos < contents.size()) {
    auto null_pos = contents.find('\0', pos);

    std::string_view entry(
        contents.data() + pos,
        null_pos - pos
    );

    auto space_pos = entry.find(' ');

    std::string mode(entry.substr(0, space_pos));
    std::string name(entry.substr(space_pos + 1));

    const unsigned char* sha =
        reinterpret_cast<const unsigned char*>(
            contents.data() + null_pos + 1
        );

    std::stringstream ss;

    for (int i = 0; i < 20; i++) {
        ss << std::hex
           << std::setw(2)
           << std::setfill('0')
           << static_cast<int>(sha[i]);
    }

    std::string type =
        (mode == "40000") ? "tree" : "blob";

    std::cout
        << mode << ' '
        << type << ' '
        << ss.str() << ' '
        << name << '\n';

    pos = null_pos + 1 + 20;
}
}

auto writeTree(const fs::path &dirPath) -> void {
    std::vector<std::tuple<std::string, std::string, std::string>> entries;

    for (auto &entry : fs::directory_iterator(dirPath)) {
        if (entry.path().filename() == ".git") continue;

        std::string name = entry.path().filename().string();
        if (entry.is_directory()) {
            std::string sha = writeTree(entry.path());
            entries.push_back({"40000", name, sha});
        } else if (entry.is_regular_file()) {
            std::vector<unsigned char> contentVec = readFile(entry.path().string());
            std::string content(contentVec.begin(), contentVec.end());
            std::string sha = writeBlob(content);

            fs::perms p = entry.status().permissions();
            std::string mode = (p & fs::perms::owner_exec) != fs::perms::none ? "100755" : "100644";
            entries.push_back({mode, name, sha});
        }
    }

    std::sort(entries.begin(), entries.end(), [](auto &a, auto &b) { return std::get<1>(a) < std::get<1>(b); });

    std::string data;
    for (auto &[mode, name, sha] : entries) {
        data += mode + " " + name + '\0';
        for (size_t i = 0; i < 20; ++i) {
            unsigned int byte;
            std::stringstream ss;
            ss << std::hex << sha.substr(i*2, 2);
            ss >> byte;
            data.push_back(static_cast<char>(byte));
        }
    }

    std::string header = "tree " + std::to_string(data.size()) + '\0';
    std::string storeData = header + data;

    std::string sha = sha1Hex(storeData);
    std::vector<unsigned char> compressed = compressZlib(storeData);

    std::string dir = ".git/objects/" + sha.substr(0, 2);
    std::filesystem::create_directories(dir);

    std::string path = dir + "/" + sha.substr(2);
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to write tree object");
    out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
    out.close();

    std::cout << sha << "\n";
}
}