#include "checkout.hpp"
#include "git_objects.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace git::checkout {

// --- Helper Functions ---

// Converts 20-byte raw SHA-1 back to a 40-character hex string
static std::string rawBytesToHex(const std::string& raw_bytes) {
    std::ostringstream oss;
    for (unsigned char c : raw_bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return oss.str();
}

// Safely clear the working directory, skipping the .git database
static void clearWorkingDirectory(const std::filesystem::path& root) {
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.path().filename() != ".git") {
            std::filesystem::remove_all(entry.path());
        }
    }
}

// --- CheckoutEngine Implementation ---

CheckoutEngine::CheckoutEngine() : workingRoot_(std::filesystem::current_path()) {}

CheckoutEngine::CheckoutEngine(std::filesystem::path workingRoot) 
    : workingRoot_(std::move(workingRoot)) {}

void CheckoutEngine::workingTree(const std::string &commit_sha) const {
    git::objects::ObjectStore store(workingRoot_ / ".git", workingRoot_ / ".git" / "objects");

    // 1. Read and parse the commit object
    std::string commit_data;
    try {
        commit_data = store.readAndDecompressObject(commit_sha);
    } catch (const std::exception& e) {
        std::cerr << "Failed to read commit object. Ensure you passed a valid commit SHA.\n";
        throw;
    }

    // Git commit format: "commit <size>\0tree <40-char-sha>\n..."
    size_t null_pos = commit_data.find('\0');
    if (null_pos == std::string::npos) throw std::runtime_error("Invalid commit object format");

    size_t tree_pos = commit_data.find("tree ", null_pos);
    if (tree_pos == std::string::npos) throw std::runtime_error("Commit object missing tree hash");

    std::string tree_sha = commit_data.substr(tree_pos + 5, 40);

    // 2. Clear current working directory (excluding .git)
    clearWorkingDirectory(workingRoot_);

    // 3. Recursively reconstruct the tree structure
    processTree(tree_sha, workingRoot_);
    
    std::cout << "Successfully checked out commit " << commit_sha << "\n";
}

void CheckoutEngine::processTree(const std::string &treeSha, const std::filesystem::path &currentDir) const {
    git::objects::ObjectStore store(workingRoot_ / ".git", workingRoot_ / ".git" / "objects");
    std::string tree_data = store.readAndDecompressObject(treeSha);

    // Skip the header "tree <size>\0"
    size_t pos = tree_data.find('\0');
    if (pos == std::string::npos) throw std::runtime_error("Invalid tree object header");
    pos++; 

    // Parse tree entries: <mode> <name>\0<20-byte raw sha1>
    while (pos < tree_data.size()) {
        size_t space_pos = tree_data.find(' ', pos);
        if (space_pos == std::string::npos) break;
        std::string mode = tree_data.substr(pos, space_pos - pos);
        pos = space_pos + 1;

        size_t null_pos = tree_data.find('\0', pos);
        if (null_pos == std::string::npos) break;
        std::string name = tree_data.substr(pos, null_pos - pos);
        pos = null_pos + 1;

        // Extract the 20-byte raw SHA-1 hash and convert to hex for our ObjectStore
        std::string raw_sha = tree_data.substr(pos, 20);
        std::string hex_sha = rawBytesToHex(raw_sha);
        pos += 20;

        std::filesystem::path target_path = currentDir / name;

        if (mode == "40000" || mode == "040000") { 
            // It's a directory
            std::filesystem::create_directories(target_path);
            processTree(hex_sha, target_path);
        } else { 
            // It's a file blob (e.g., 100644)
            std::string blob_data = store.readAndDecompressObject(hex_sha);
            
            size_t blob_null_pos = blob_data.find('\0');
            if (blob_null_pos == std::string::npos) throw std::runtime_error("Invalid blob format");
            
            std::string file_content = blob_data.substr(blob_null_pos + 1);

            // Write in binary mode to prevent corrupting non-text files or line endings
            std::ofstream out(target_path, std::ios::binary);
            if (!out) {
                throw std::runtime_error("Failed to create file: " + target_path.string());
            }
            out << file_content;
        }
    }
}

// --- Free Function Wrapper ---

void workingTree(const std::string& commit_sha) {
    CheckoutEngine engine;
    engine.workingTree(commit_sha);
}

} // namespace git::checkout