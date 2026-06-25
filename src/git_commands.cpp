#include "git_commands.hpp"
#include "git_objects.hpp"
#include "utils.hpp"
#include "network.hpp"   // To be implemented next
#include "packfile.hpp"  // To be implemented next
#include "checkout.hpp"  // To be implemented next

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace git {

void init() {
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

void commit_tree(const std::string& treeSha, const std::string& parentSha, const std::string& message) {
    std::cout << git::objects::writeCommitObject(treeSha, parentSha, message) << '\n';
}

void clone(const std::string& url, const std::string& dir) {
    // 1. Create target directory and initialize git repository
    std::filesystem::create_directories(dir);
    std::filesystem::current_path(dir);
    init();

    // 2. Discover refs and get the HEAD commit SHA-1
    std::string head_sha = git::network::discoverRefs(url);

    // 3. Negotiate and fetch the packfile
    std::vector<unsigned char> packfile = git::network::fetchPackfile(url, head_sha);

    // 4. Parse the packfile, resolve deltas, and write loose objects to .git/objects
    git::packfile::process(packfile);

    // 5. Read the HEAD commit, recursively parse its tree, and write files to the working directory
    git::checkout::workingTree(head_sha);
}

} // namespace git