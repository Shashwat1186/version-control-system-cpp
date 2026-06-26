#include "branch.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace git::branch {

Branch::Branch(const std::string& branch_name) 
    : name(branch_name), git_dir(std::filesystem::current_path() / ".git") {}

std::string Branch::get_name() const {
    return name;
}

std::string Branch::resolve_head_commit() const {
    std::ifstream head_file(git_dir / "HEAD");
    if (!head_file) throw std::runtime_error("Cannot read .git/HEAD");

    std::string content;
    std::getline(head_file, content);

    // If HEAD points to a ref (e.g., "ref: refs/heads/master")
    if (content.rfind("ref: ", 0) == 0) {
        std::string ref_path = content.substr(5);
        std::ifstream ref_file(git_dir / ref_path);
        if (!ref_file) throw std::runtime_error("Cannot read reference: " + ref_path);
        
        std::string commit_sha;
        std::getline(ref_file, commit_sha);
        return commit_sha;
    }
    
    // If HEAD is detached, it directly contains the commit SHA
    return content; 
}

void Branch::create_from_head() const {
    std::string target_commit = resolve_head_commit();

    std::filesystem::path branch_path = git_dir / "refs" / "heads" / name;
    
    if (std::filesystem::exists(branch_path)) {
        throw std::runtime_error("A branch named '" + name + "' already exists.");
    }

    std::filesystem::create_directories(branch_path.parent_path());

    std::ofstream out(branch_path);
    if (!out) throw std::runtime_error("Failed to create branch file.");
    
    out << target_commit << '\n';
}

std::vector<std::string> Branch::get_all() {
    std::vector<std::string> branches;
    std::filesystem::path heads_dir = std::filesystem::current_path() / ".git" / "refs" / "heads";

    if (!std::filesystem::exists(heads_dir)) return branches;

    for (const auto& entry : std::filesystem::directory_iterator(heads_dir)) {
        if (entry.is_regular_file()) {
            branches.push_back(entry.path().filename().string());
        }
    }
    return branches;
}

std::string Branch::get_current() {
    std::ifstream head_file(std::filesystem::current_path() / ".git" / "HEAD");
    if (!head_file) return "";

    std::string content;
    std::getline(head_file, content);

    if (content.rfind("ref: refs/heads/", 0) == 0) {
        return content.substr(16); // Extract branch name after "ref: refs/heads/"
    }
    return "(detached HEAD)";
}

} // namespace git::branch