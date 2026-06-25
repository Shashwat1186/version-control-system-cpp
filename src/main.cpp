#include <iostream>
#include <string>
#include <vector>
#include "git_commands.hpp"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: mygit <command> [<args>]\n";
        return 1;
    }

    std::string command = argv[1];

    try {
        if (command == "init") {
            git::init();
        } else if (command == "cat-file") {
            if (argc == 4 && std::string(argv[2]) == "-p") {
                git::cat_file(argv[3]);
            } else {
                std::cerr << "Usage: mygit cat-file -p <hash>\n";
                return 1;
            }
        } else if (command == "hash-object") {
            if (argc == 4 && std::string(argv[2]) == "-w") {
                git::hash_object(argv[3]);
            } else {
                std::cerr << "Usage: mygit hash-object -w <file>\n";
                return 1;
            }
        } else if (command == "ls-tree") {
            if (argc == 4 && std::string(argv[2]) == "--name-only") {
                git::ls_tree_name_only(argv[3]);
            } else if (argc == 3) {
                git::ls_tree(argv[2]);
            } else {
                std::cerr << "Usage: mygit ls-tree [--name-only] <hash>\n";
                return 1;
            }
        } else if (command == "write-tree") {
            git::write_tree(".");
        } else if (command == "commit-tree") {
            if (argc >= 5) {
                std::string tree_sha = argv[2];
                std::string parent_sha = "";
                std::string message = "";

                for (int i = 3; i < argc; ++i) {
                    if (std::string(argv[i]) == "-p" && i + 1 < argc) {
                        parent_sha = argv[++i];
                    } else if (std::string(argv[i]) == "-m" && i + 1 < argc) {
                        message = argv[++i];
                    }
                }
                git::commit_tree(tree_sha, parent_sha, message);
            } else {
                std::cerr << "Usage: mygit commit-tree <tree_sha> [-p <parent_sha>] -m <message>\n";
                return 1;
            }
        } else if (command == "clone") {
            if (argc == 4) {
                git::clone(argv[2], argv[3]);
            } else {
                std::cerr << "Usage: mygit clone <url> <directory>\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown command: " << command << '\n';
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}