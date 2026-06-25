#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace git::branch {

class Branch {
private:
    std::string name;
    std::filesystem::path git_dir;

    std::string resolve_head_commit() const;

public:
    explicit Branch(const std::string& branch_name);

    std::string get_name() const;
    void create_from_head() const;

    // Static utilities for branch management
    static std::vector<std::string> get_all();
    static std::string get_current();
};

} // namespace git::branch