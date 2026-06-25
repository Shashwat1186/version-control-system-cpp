#include "checkout.hpp"
#include "git_objects.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

namespace git::checkout {

namespace {

class CheckoutEngine {
public:
    void workingTree(const std::string &commitSha) {
        std::string commitData = git::objects::readAndDecompressObject(commitSha);

        auto nullPos = commitData.find('\0');
        if (nullPos == std::string::npos) throw std::runtime_error("Invalid commit object");
        std::string content = commitData.substr(nullPos + 1);

        size_t treePos = content.find("tree ");
        if (treePos == std::string::npos) throw std::runtime_error("No tree found in commit");

        std::string treeSha = content.substr(treePos + 5, 40);
        processTree(treeSha, fs::current_path());
    }

private:
    void processTree(const std::string &treeSha, const fs::path &currentDir) {
        std::string contents = git::objects::readAndDecompressObject(treeSha);
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

            const unsigned char* sha_bytes = reinterpret_cast<const unsigned char*>(contents.data() + null_pos + 1);
            std::stringstream ss;
            for (int i = 0; i < 20; i++) {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(sha_bytes[i]);
            }
            std::string sha = ss.str();

            fs::path target_path = currentDir / name;

            if (mode == "40000") { 
                // It's a directory (tree)
                fs::create_directories(target_path);
                processTree(sha, target_path);
            } else { 
                // It's a file (blob)
                std::string blob_contents = git::objects::readAndDecompressObject(sha);
                auto blob_null_pos = blob_contents.find('\0');
                if (blob_null_pos != std::string::npos) {
                    std::string actual_data = blob_contents.substr(blob_null_pos + 1);
                    
                    std::ofstream out(target_path, std::ios::binary);
                    if (!out) throw std::runtime_error("Failed to create file: " + target_path.string());
                    out.write(actual_data.data(), actual_data.size());
                    out.close();

                    // Restore executable permissions if required
                    if (mode == "100755") {
                        fs::permissions(target_path, 
                                        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, 
                                        fs::perm_options::add);
                    }
                }
            }
            pos = null_pos + 1 + 20;
        }
    }
};

} // namespace

void workingTree(const std::string& commit_sha) {
    CheckoutEngine{}.workingTree(commit_sha);
}

} // namespace git::checkout