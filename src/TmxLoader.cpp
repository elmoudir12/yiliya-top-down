#include "TmxLoader.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <zlib.h>
#include <stdexcept>
#include <algorithm>

static const std::string base64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::vector<uint8_t> decodeBase64(const std::string& input) {
    std::string cleaned;
    for (char c : input) {
        if (!isspace(c)) cleaned += c;
    }

    std::vector<uint8_t> out;
    out.reserve(cleaned.size() * 3 / 4);

    int val = 0, valb = -8;
    for (char c : cleaned) {
        if (c == '=') break;
        size_t pos = base64Chars.find(c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + static_cast<int>(pos);
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

static std::vector<uint8_t> decompressZlib(const std::vector<uint8_t>& data) {
    uLongf destLen = data.size() * 10;
    std::vector<uint8_t> out(destLen);

    while (true) {
        int ret = uncompress(out.data(), &destLen, data.data(), data.size());
        if (ret == Z_OK) {
            out.resize(destLen);
            return out;
        }
        if (ret == Z_BUF_ERROR) {
            destLen *= 2;
            out.resize(destLen);
            continue;
        }
        throw std::runtime_error("Zlib decompression failed");
    }
}

static std::string extractTagAttr(const std::string& xml, const std::string& tagStart,
                                   const std::string& attr) {
    size_t pos = xml.find(tagStart);
    if (pos == std::string::npos) return {};

    std::string search = attr + "=\"";
    pos = xml.find(search, pos);
    if (pos == std::string::npos) return {};

    pos += search.size();
    size_t end = xml.find('"', pos);
    if (end == std::string::npos) return {};

    return xml.substr(pos, end - pos);
}

static std::string extractBetween(const std::string& xml, const std::string& openTag,
                                   const std::string& closeTag, size_t startPos) {
    size_t open = xml.find(openTag, startPos);
    if (open == std::string::npos) return {};

    size_t contentStart = xml.find('>', open) + 1;
    if (contentStart == std::string::npos) return {};

    size_t close = xml.find(closeTag, contentStart);
    if (close == std::string::npos) return {};

    return xml.substr(contentStart, close - contentStart);
}

bool loadTmx(const std::string& filepath, TmxMapData& out) {
    std::ifstream file(filepath);
    if (!file) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string xml = buffer.str();

    std::string wStr = extractTagAttr(xml, "<map", "width");
    std::string hStr = extractTagAttr(xml, "<map", "height");
    if (wStr.empty() || hStr.empty()) return false;

    out.width = std::stoi(wStr);
    out.height = std::stoi(hStr);

    std::string gidStr = extractTagAttr(xml, "<tileset", "firstgid");
    int firstGid = gidStr.empty() ? 1 : std::stoi(gidStr);

    // Find ground layer (name="ground" or id="1")
    size_t groundPos = xml.find("name=\"ground\"");
    if (groundPos == std::string::npos) {
        groundPos = xml.find("id=\"1\"");
    }

    // Check if CSV or base64 encoding
    size_t dataTagStart = xml.find("<data", groundPos);
    bool isCsv = false;
    if (dataTagStart != std::string::npos) {
        size_t dataTagClose = xml.find('>', dataTagStart);
        if (dataTagClose != std::string::npos) {
            std::string tagContent = xml.substr(dataTagStart, dataTagClose - dataTagStart);
            isCsv = (tagContent.find("csv") != std::string::npos);
        }
    }

    std::string groundData = extractBetween(xml, "<data", "</data>", groundPos);
    if (groundData.empty()) return false;

    int numTiles = out.width * out.height;
    out.groundTiles.resize(numTiles);

    if (isCsv) {
        // Parse CSV format
        std::string cleaned;
        for (char c : groundData) {
            if (c == ',' || c == '\n' || c == '\r') {
                cleaned += ' ';
            } else {
                cleaned += c;
            }
        }
        std::istringstream stream(cleaned);
        int idx = 0;
        int val;
        while (stream >> val && idx < numTiles) {
            int gid = val;
            if (gid == 0) {
                out.groundTiles[idx] = 324;
            } else {
                out.groundTiles[idx] = gid - firstGid;
            }
            ++idx;
        }
        if (idx != numTiles) return false;
    } else {
        // Base64+zlib format
        auto decoded = decodeBase64(groundData);
        auto decompressed = decompressZlib(decoded);
        if (static_cast<int>(decompressed.size()) < numTiles * 4) return false;
        const uint32_t* gids = reinterpret_cast<const uint32_t*>(decompressed.data());
        for (int i = 0; i < numTiles; ++i) {
            int gid = static_cast<int>(gids[i]);
            if (gid == 0) {
                out.groundTiles[i] = 324;
            } else {
                out.groundTiles[i] = gid - firstGid;
            }
        }
    }

    // Collision: derive from ground tiles
    // tile 324 (void) and tile 96 (wall) are blocked
    out.collisionTiles.resize(numTiles);
    for (int i = 0; i < numTiles; ++i) {
        int t = out.groundTiles[i];
        out.collisionTiles[i] = (t == 324 || t == 96) ? 1 : 0;
    }

    return true;
}
