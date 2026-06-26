import os

def patch_file(path, old_strs, new_strs):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    for old_s, new_s in zip(old_strs, new_strs):
        content = content.replace(old_s, new_s)
        
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

# 1. CMakeLists.txt
patch_file(
    "CMakeLists.txt",
    [
        "find_package(OpenSSL REQUIRED)",
        "find_package(CURL REQUIRED)",
        "    OpenSSL::Crypto\n    CURL::libcurl\n",
        'list(FILTER SOURCES EXCLUDE REGEX "src/git_commands\\.cpp$")'
    ],
    [
        "",
        "",
        "",
        'list(FILTER SOURCES EXCLUDE REGEX "src/git_commands\\\\.cpp$")\nlist(FILTER SOURCES EXCLUDE REGEX "src/network\\\\.cpp$")\nlist(FILTER SOURCES EXCLUDE REGEX "src/packfile\\\\.cpp$")'
    ]
)

# 2. src/main.cpp
patch_file(
    "src/main.cpp",
    [
        '        } else if (command == "clone") {\n            if (argc == 4) {\n                git::clone(argv[2], argv[3]);\n            } else {\n                std::cerr << "Usage: mygit clone <url> <directory>\\n";\n                return 1;\n            }'
    ],
    [
        '        /*} else if (command == "clone") {\n            if (argc == 4) {\n                git::clone(argv[2], argv[3]);\n            } else {\n                std::cerr << "Usage: mygit clone <url> <directory>\\n";\n                return 1;\n            }*/'
    ]
)

# 3. src/git_objects.cpp
patch_file(
    "src/git_objects.cpp",
    [
        '#include "network.hpp"',
        '#include "packfile.hpp"',
        '    static void clone(const std::string &url, const std::string &dir) {\n        fs::create_directories(dir);\n        fs::current_path(dir);\n        init();\n\n        std::string headSha = git::network::discoverRefs(url);\n        std::vector<unsigned char> packfile = git::network::fetchPackfile(url, headSha);\n        git::packfile::process(packfile);\n        git::checkout::workingTree(headSha);\n    }',
        'void clone(const std::string &url, const std::string &dir) { GitRepository::clone(url, dir); }'
    ],
    [
        '// #include "network.hpp"',
        '// #include "packfile.hpp"',
        '    /*static void clone(const std::string &url, const std::string &dir) {\n        fs::create_directories(dir);\n        fs::current_path(dir);\n        init();\n\n        std::string headSha = git::network::discoverRefs(url);\n        std::vector<unsigned char> packfile = git::network::fetchPackfile(url, headSha);\n        git::packfile::process(packfile);\n        git::checkout::workingTree(headSha);\n    }*/',
        '// void clone(const std::string &url, const std::string &dir) { GitRepository::clone(url, dir); }'
    ]
)

# 4. src/git_commands.hpp
patch_file(
    "src/git_commands.hpp",
    [
        'void clone(const std::string &url, const std::string &dir);'
    ],
    [
        '// void clone(const std::string &url, const std::string &dir);'
    ]
)

print("Patching done.")
