import os

def patch_file(path, old_strs, new_strs):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    for old_s, new_s in zip(old_strs, new_strs):
        content = content.replace(old_s, new_s)
        
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

patch_file(
    "CMakeLists.txt",
    [
        "find_package(ZLIB REQUIRED)",
        "target_link_libraries(git PRIVATE\n    ZLIB::ZLIB\n)"
    ],
    [
        "find_package(ZLIB REQUIRED)\nfind_package(OpenSSL REQUIRED)",
        "target_link_libraries(git PRIVATE\n    ZLIB::ZLIB\n    OpenSSL::Crypto\n)"
    ]
)
print("Restored OpenSSL. Running cmake build...")
