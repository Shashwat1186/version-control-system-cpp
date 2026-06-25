#pragma once
#include <string>
#include <vector>

namespace git {

    // Represents a Git Branch entity
    class Branch {
    private:
        std::string name;
        std::string commit_hash;

    public:
        // Constructors
        Branch(const std::string& branch_name);
        Branch(const std::string& branch_name, const std::string& hash);

        // Getters
        std::string get_name() const;
        std::string get_commit_hash() const;

        // Behaviors
        void create_from_head() const; // Reads HEAD and creates .git/refs/heads/<name>
        
        // Static utilities for branch management
        static std::vector<Branch> get_all();
        static Branch get_current();
    };

    // Manages the state of the local working directory
    class Worktree {
    private:
        std::string root_path;

        // Helper methods for checkout operations
        void update_head(const std::string& target_branch);
        void clear_working_directory(); // Deletes tracked files (avoids touching .git)
        void restore_tree(const std::string& tree_hash, const std::string& current_dir);

    public:
        Worktree(const std::string& path = ".");
        
        // Executes the full checkout pipeline: 
        // 1. Resolve target -> 2. Update HEAD -> 3. Clear dir -> 4. Traverse & write new tree
        void checkout(const std::string& target);
    };

} // namespace git