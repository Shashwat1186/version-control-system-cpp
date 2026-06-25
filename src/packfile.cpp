#include "packfile.hpp"
#include "git_objects.hpp"

#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

namespace git::packfile {

namespace {

class PackfileProcessor {
public:
    void process(const std::vector<unsigned char> &packData) const {
        if (packData.size() < 12) throw std::runtime_error("Packfile too small");

        if (packData[0] != 'P' || packData[1] != 'A' || packData[2] != 'C' || packData[3] != 'K') {
            throw std::runtime_error("Invalid packfile signature");
        }

        uint32_t objectCount = (static_cast<uint32_t>(packData[8]) << 24) |
                               (static_cast<uint32_t>(packData[9]) << 16) |
                               (static_cast<uint32_t>(packData[10]) << 8) |
                               static_cast<uint32_t>(packData[11]);

        size_t pos = 12;
        std::map<size_t, ParsedObject> objectsByOffset;
        std::map<std::string, ParsedObject> objectsBySha;

        for (uint32_t i = 0; i < objectCount; ++i) {
            size_t objectOffset = pos;

            unsigned char c = packData[pos++];
            int type = (c >> 4) & 7;
            size_t size = c & 15;
            size_t shift = 4;

            while (c & 0x80) {
                c = packData[pos++];
                size |= static_cast<size_t>(c & 0x7F) << shift;
                shift += 7;
            }

            if (type >= 1 && type <= 4) {
                auto [data, consumed] = decompressStream(&packData[pos], packData.size() - pos);
                pos += consumed;

                std::string typeStr = getTypeName(type);
                objectsByOffset[objectOffset] = {typeStr, data};
                std::string sha = git::objects::writeObject(typeStr, data);
                objectsBySha[sha] = {typeStr, data};
            } else if (type == 6) {
                size_t negativeOffset = 0;
                c = packData[pos++];
                negativeOffset = c & 0x7F;
                while (c & 0x80) {
                    negativeOffset++;
                    c = packData[pos++];
                    negativeOffset = (negativeOffset << 7) + (c & 0x7F);
                }

                size_t baseOffset = objectOffset - negativeOffset;
                auto [deltaData, consumed] = decompressStream(&packData[pos], packData.size() - pos);
                pos += consumed;

                if (objectsByOffset.find(baseOffset) == objectsByOffset.end()) {
                    throw std::runtime_error("OFS_DELTA base not found in current pack");
                }

                const auto &baseObject = objectsByOffset[baseOffset];
                std::string resolvedData = applyDelta(baseObject.data, deltaData);

                objectsByOffset[objectOffset] = {baseObject.type, resolvedData};
                std::string sha = git::objects::writeObject(baseObject.type, resolvedData);
                objectsBySha[sha] = {baseObject.type, resolvedData};
            } else if (type == 7) {
                std::stringstream shaStream;
                for (int j = 0; j < 20; ++j) {
                    shaStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(packData[pos++]);
                }
                std::string baseSha = shaStream.str();

                auto [deltaData, consumed] = decompressStream(&packData[pos], packData.size() - pos);
                pos += consumed;

                ParsedObject baseObject;
                if (objectsBySha.find(baseSha) != objectsBySha.end()) {
                    baseObject = objectsBySha[baseSha];
                } else {
                    std::string raw = git::objects::readAndDecompressObject(baseSha);
                    size_t nullPos = raw.find('\0');
                    std::string header = raw.substr(0, nullPos);
                    baseObject.type = header.substr(0, header.find(' '));
                    baseObject.data = raw.substr(nullPos + 1);
                }

                std::string resolvedData = applyDelta(baseObject.data, deltaData);

                objectsByOffset[objectOffset] = {baseObject.type, resolvedData};
                std::string sha = git::objects::writeObject(baseObject.type, resolvedData);
                objectsBySha[sha] = {baseObject.type, resolvedData};
            } else {
                throw std::runtime_error("Unsupported object type: " + std::to_string(type));
            }
        }
    }

private:
    struct ParsedObject {
        std::string type;
        std::string data;
    };

    static std::pair<std::string, size_t> decompressStream(const unsigned char *data, size_t maxSize) {
        z_stream zs{};
        if (inflateInit(&zs) != Z_OK) throw std::runtime_error("inflateInit failed");

        zs.next_in = const_cast<Bytef *>(data);
        zs.avail_in = static_cast<uInt>(maxSize);

        std::string out;
        char temp[4096];
        int ret;
        do {
            zs.next_out = reinterpret_cast<Bytef *>(temp);
            zs.avail_out = sizeof(temp);

            ret = inflate(&zs, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                inflateEnd(&zs);
                throw std::runtime_error("inflate failed during packfile parsing");
            }
            out.append(temp, sizeof(temp) - zs.avail_out);
        } while (ret != Z_STREAM_END);

        size_t consumed = zs.total_in;
        inflateEnd(&zs);
        return {out, consumed};
    }

    static size_t readDeltaSize(const std::string &delta, size_t &pos) {
        size_t size = 0;
        size_t shift = 0;
        while (pos < delta.size()) {
            unsigned char c = static_cast<unsigned char>(delta[pos++]);
            size |= static_cast<size_t>(c & 0x7F) << shift;
            shift += 7;
            if ((c & 0x80) == 0) break;
        }
        return size;
    }

    static std::string applyDelta(const std::string &base, const std::string &delta) {
        size_t pos = 0;
        size_t baseSize = readDeltaSize(delta, pos);
        size_t targetSize = readDeltaSize(delta, pos);

        if (baseSize != base.size()) {
            throw std::runtime_error("Delta base size mismatch");
        }

        std::string result;
        result.reserve(targetSize);

        while (pos < delta.size()) {
            unsigned char cmd = static_cast<unsigned char>(delta[pos++]);

            if (cmd & 0x80) {
                size_t offset = 0;
                size_t size = 0;
                if (cmd & 0x01) offset |= static_cast<size_t>(static_cast<unsigned char>(delta[pos++]));
                if (cmd & 0x02) offset |= static_cast<size_t>(static_cast<unsigned char>(delta[pos++])) << 8;
                if (cmd & 0x04) offset |= static_cast<size_t>(static_cast<unsigned char>(delta[pos++])) << 16;
                if (cmd & 0x08) offset |= static_cast<size_t>(static_cast<unsigned char>(delta[pos++])) << 24;
                if (cmd & 0x10) size |= static_cast<size_t>(static_cast<unsigned char>(delta[pos++]));
                if (cmd & 0x20) size |= static_cast<size_t>(static_cast<unsigned char>(delta[pos++])) << 8;
                if (cmd & 0x40) size |= static_cast<size_t>(static_cast<unsigned char>(delta[pos++])) << 16;

                if (size == 0) size = 0x10000;
                if (offset > base.size() || offset + size > base.size()) {
                    throw std::runtime_error("Delta copy out of bounds");
                }

                result.append(base, offset, size);
            } else {
                size_t insertSize = cmd & 0x7F;
                result.append(delta, pos, insertSize);
                pos += insertSize;
            }
        }

        return result;
    }

    static std::string getTypeName(int type) {
        switch (type) {
            case 1: return "commit";
            case 2: return "tree";
            case 3: return "blob";
            case 4: return "tag";
            default: throw std::runtime_error("Unknown object type: " + std::to_string(type));
        }
    }
};

} // namespace

void process(const std::vector<unsigned char> &packData) {
    PackfileProcessor{}.process(packData);
}

} // namespace git::packfile