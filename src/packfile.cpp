#include "packfile.hpp"
#include "git_objects.hpp"
#include "utils.hpp"
#include <iostream>
#include <string>
#include <map>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <zlib.h>

namespace git::packfile {

namespace {

    struct ParsedObject {
        std::string type;
        std::string data;
    };

    // Helper to decompress a single object from a stream and return consumed bytes
    std::pair<std::string, size_t> decompressStream(const unsigned char* data, size_t max_size) {
        z_stream zs{};
        if (inflateInit(&zs) != Z_OK) throw std::runtime_error("inflateInit failed");

        zs.next_in = const_cast<Bytef*>(data);
        zs.avail_in = static_cast<uInt>(max_size);

        std::string out;
        char temp[4096];
        int ret;
        do {
            zs.next_out = reinterpret_cast<Bytef*>(temp);
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

    size_t readDeltaSize(const std::string& delta, size_t& pos) {
        size_t size = 0;
        size_t shift = 0;
        while (pos < delta.size()) {
            unsigned char c = delta[pos++];
            size |= static_cast<size_t>(c & 0x7F) << shift;
            shift += 7;
            if ((c & 0x80) == 0) break;
        }
        return size;
    }

    std::string applyDelta(const std::string& base, const std::string& delta) {
        size_t pos = 0;
        // The first two variable-length integers are the source and target sizes.
        size_t base_size = readDeltaSize(delta, pos);
        size_t target_size = readDeltaSize(delta, pos);

        if (base_size != base.size()) {
            throw std::runtime_error("Delta base size mismatch");
        }

        std::string result;
        result.reserve(target_size);

        while (pos < delta.size()) {
            unsigned char cmd = delta[pos++];
            if (cmd & 0x80) { // Copy instruction
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
            } else { // Insert instruction
                size_t size = cmd & 0x7F;
                result.append(delta, pos, size);
                pos += size;
            }
        }
        return result;
    }

    std::string getTypeName(int type) {
        switch (type) {
            case 1: return "commit";
            case 2: return "tree";
            case 3: return "blob";
            case 4: return "tag";
            default: throw std::runtime_error("Unknown object type: " + std::to_string(type));
        }
    }
} // namespace

void process(const std::vector<unsigned char>& packData) {
    if (packData.size() < 12) throw std::runtime_error("Packfile too small");

    // Verify "PACK" signature
    if (packData[0] != 'P' || packData[1] != 'A' || packData[2] != 'C' || packData[3] != 'K') {
        throw std::runtime_error("Invalid packfile signature");
    }

    // Number of objects (bytes 8-11, network byte order)
    uint32_t obj_count = (packData[8] << 24) | (packData[9] << 16) | (packData[10] << 8) | packData[11];

    size_t pos = 12; // Start immediately after header
    std::map<size_t, ParsedObject> objectsByOffset;
    std::map<std::string, ParsedObject> objectsBySha;

    for (uint32_t i = 0; i < obj_count; ++i) {
        size_t obj_offset = pos;

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
            // Standard objects (commit, tree, blob, tag)
            auto [data, consumed] = decompressStream(&packData[pos], packData.size() - pos);
            pos += consumed;

            std::string typeStr = getTypeName(type);
            objectsByOffset[obj_offset] = {typeStr, data};
            std::string sha = git::objects::writeObject(typeStr, data);
            objectsBySha[sha] = {typeStr, data};

        } else if (type == 6) { 
            // OFS_DELTA
            size_t negative_offset = 0;
            c = packData[pos++];
            negative_offset = c & 0x7F;
            while (c & 0x80) {
                negative_offset++;
                c = packData[pos++];
                negative_offset = (negative_offset << 7) + (c & 0x7F);
            }

            size_t base_offset = obj_offset - negative_offset;
            auto [delta_data, consumed] = decompressStream(&packData[pos], packData.size() - pos);
            pos += consumed;

            if (objectsByOffset.find(base_offset) == objectsByOffset.end()) {
                throw std::runtime_error("OFS_DELTA base not found in current pack");
            }
            
            const auto& base_obj = objectsByOffset[base_offset];
            std::string resolved_data = applyDelta(base_obj.data, delta_data);
            
            objectsByOffset[obj_offset] = {base_obj.type, resolved_data};
            std::string sha = git::objects::writeObject(base_obj.type, resolved_data);
            objectsBySha[sha] = {base_obj.type, resolved_data};

        } else if (type == 7) {
            // REF_DELTA
            std::stringstream sha_ss;
            for (int j = 0; j < 20; ++j) {
                sha_ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(packData[pos++]);
            }
            std::string base_sha = sha_ss.str();

            auto [delta_data, consumed] = decompressStream(&packData[pos], packData.size() - pos);
            pos += consumed;

            ParsedObject base_obj;
            if (objectsBySha.find(base_sha) != objectsBySha.end()) {
                base_obj = objectsBySha[base_sha];
            } else {
                // If base is not in memory, it might already exist in .git/objects
                std::string raw = git::objects::readAndDecompressObject(base_sha);
                size_t null_pos = raw.find('\0');
                std::string header = raw.substr(0, null_pos);
                base_obj.type = header.substr(0, header.find(' '));
                base_obj.data = raw.substr(null_pos + 1);
            }

            std::string resolved_data = applyDelta(base_obj.data, delta_data);
            
            objectsByOffset[obj_offset] = {base_obj.type, resolved_data};
            std::string sha = git::objects::writeObject(base_obj.type, resolved_data);
            objectsBySha[sha] = {base_obj.type, resolved_data};
        } else {
            throw std::runtime_error("Unsupported object type: " + std::to_string(type));
        }
    }
}

} // namespace git::packfile