#include "utils.hpp"
#include <fstream>
#include <stdexcept>
#include <iterator>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <cmath>
#include <zlib.h>
#include <openssl/sha.h>

namespace git::utils {

std::vector<unsigned char> readFile(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open file: " + path);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::vector<unsigned char> compressZlib(const std::string &data) {
    z_stream zs{};
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) throw std::runtime_error("deflateInit failed");

    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    std::vector<unsigned char> outBuffer;
    unsigned char temp[4096];

    int ret;
    do {
        zs.next_out = temp;
        zs.avail_out = sizeof(temp);
        ret = deflate(&zs, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&zs);
            throw std::runtime_error("deflate failed");
        }
        outBuffer.insert(outBuffer.end(), temp, temp + (sizeof(temp) - zs.avail_out));
    } while (zs.avail_out == 0);

    deflateEnd(&zs);
    return outBuffer;
}

std::string decompressZlib(const std::vector<unsigned char> &data) {
    z_stream zs{};
    zs.next_in = const_cast<Bytef *>(const_cast<unsigned char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    if (inflateInit(&zs) != Z_OK) throw std::runtime_error("inflateInit failed");

    std::vector<char> outBuffer;
    char temp[4096];
    int ret;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(temp);
        zs.avail_out = sizeof(temp);

        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&zs);
            throw std::runtime_error("inflate failed");
        }

        outBuffer.insert(outBuffer.end(), temp, temp + (sizeof(temp) - zs.avail_out));
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);
    return std::string(outBuffer.begin(), outBuffer.end());
}

std::string sha1Hex(const std::string &data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(data.data()), data.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string getCurrentTimestamp() {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto seconds = system_clock::to_time_t(now);

    std::tm local = *std::localtime(&seconds);
    std::tm utc = *std::gmtime(&seconds);

    std::time_t local_time = std::mktime(&local);
    std::time_t utc_time = std::mktime(&utc);

    long offset = static_cast<long>(std::difftime(local_time, utc_time));

    char sign = offset >= 0 ? '+' : '-';
    offset = std::abs(offset);

    int hours = offset / 3600;
    int mins = (offset % 3600) / 60;

    std::stringstream ss;
    ss << seconds << " " << sign << std::setw(2) << std::setfill('0') << hours << std::setw(2) << mins;

    return ss.str();
}

} // namespace git::utils