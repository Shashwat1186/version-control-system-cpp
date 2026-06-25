#include "checkout.hpp"
#include "git_objects.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace git::checkout {

CheckoutEngine::CheckoutEngine() : workingRoot_(std::filesystem::current_path()) {}

CheckoutEngine::CheckoutEngine(std::filesystem::path workingRoot) : workingRoot_(std::move(workingRoot)) {}

void CheckoutEngine::processTree(const std::string &treeSha, const std::filesystem::path &currentDir) const {
    std::string contents = git::objects::readAndDecompressObject(treeSha);
    auto pos = contents.find('\0');
    if (pos == std::string::npos) throw std::runtime_error("Invalid tree object");
    pos += 1;

    while (pos < contents.size()) {
        auto nullPos = contents.find('\0', pos);
        if (nullPos == std::string::npos || nullPos + 21 > contents.size()) break;

        std::string_view entry(contents.data() + pos, nullPos - pos);
        auto spacePos = entry.find(' ');

        std::string mode(entry.substr(0, spacePos));
        std::string name(entry.substr(spacePos + 1));

        const unsigned char *shaBytes = reinterpret_cast<const unsigned char *>(contents.data() + nullPos + 1);
        std::stringstream ss;
        for (int i = 0; i < 20; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(shaBytes[i]);
        }
        std::string sha = ss.str();

        std::filesystem::path targetPath = currentDir / name;

        if (mode == "40000") {
            std::filesystem::create_directories(targetPath);
            processTree(sha, targetPath);
        } else {
            std::string blobContents = git::objects::readAndDecompressObject(sha);
            auto blobNullPos = blobContents.find('\0');
            if (blobNullPos != std::string::npos) {
                std::string actualData = blobContents.substr(blobNullPos + 1);

                std::ofstream out(targetPath, std::ios::binary);
                if (!out) throw std::runtime_error("Failed to create file: " + targetPath.string());
                out.write(actualData.data(), actualData.size());
                out.close();

                if (mode == "100755") {
                    std::filesystem::permissions(targetPath,
                                                 std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec,
                                                 std::filesystem::perm_options::add);
                }
            }
        }
        pos = nullPos + 1 + 20;
    }
}

void CheckoutEngine::workingTree(const std::string &commitSha) const {
    std::string commitData = git::objects::readAndDecompressObject(commitSha);
    auto nullPos = commitData.find('\0');
    if (nullPos == std::string::npos) throw std::runtime_error("Invalid commit object");
    std::string content = commitData.substr(nullPos + 1);

    size_t treePos = content.find("tree ");
    if (treePos == std::string::npos) throw std::runtime_error("No tree found in commit");

    std::string treeSha = content.substr(treePos + 5, 40);
    processTree(treeSha, workingRoot_);
}

void workingTree(const std::string &commit_sha) {
    CheckoutEngine{}.workingTree(commit_sha);
}

} // namespace git::checkout