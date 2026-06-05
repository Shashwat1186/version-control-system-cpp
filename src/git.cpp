#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>
#include <openssl/evp.h> // 1. Change this header from <openssl/sha.h>

void hash_object(std::string filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open: " << filename << '\n'; // 2. Fixed 'path' to 'filename'
        return;
    }

    // 3. Create and initialize the modern OpenSSL 3.0 Context
    EVP_MD_CTX* mdContext = EVP_MD_CTX_new();
    if (!mdContext) {
        std::cerr << "Failed to create EVP context\n";
        return;
    }

    // 4. Fetch the SHA-1 algorithm and initialize the context with it
    if (EVP_DigestInit_ex(mdContext, EVP_sha1(), nullptr) != 1) {
        std::cerr << "Failed to initialize SHA-1 digest\n";
        EVP_MD_CTX_free(mdContext);
        return;
    }

    char buffer[4096];

    // 5. Streaming loop remains practically identical
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        EVP_DigestUpdate(mdContext, buffer, file.gcount());
    }

    // 6. Finalize and fetch the hash bytes
    unsigned char hashBytes[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    EVP_DigestFinal_ex(mdContext, hashBytes, &hashLen);

    // 7. Clean up the allocated context memory
    EVP_MD_CTX_free(mdContext);
    file.close();

    // 8. Convert raw bytes to standard Git Hex string format
    std::stringstream ss;
    for (unsigned int i = 0; i < hashLen; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hashBytes[i];
    }
    std::string hashString = ss.str();

    // Print the final 40-character SHA-1 string
    std::cout << hashString << std::endl;
}