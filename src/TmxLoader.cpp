#include "TmxLoader.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <zlib.h>
#include <stdexcept>
#include <algorithm>
#include <cctype>

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

static bool parseLayerData(const std::string& xml, size_t layerPos,
                            int width, int height, int firstGid,
                            std::vector<int>& outTiles) {
    size_t dataStart = xml.find("<data", layerPos);
    if (dataStart == std::string::npos) return false;

    // Check encoding
    size_t dataTagEnd = xml.find('>', dataStart);
    if (dataTagEnd == std::string::npos) return false;
    std::string tagContent = xml.substr(dataStart, dataTagEnd - dataStart);

    int numTiles = width * height;
    outTiles.assign(numTiles, 324);

    bool isCsv = (tagContent.find("csv") != std::string::npos);

    std::string rawData = extractBetween(xml, "<data", "</data>", layerPos);
    if (rawData.empty()) return false;

    if (isCsv) {
        std::string cleaned;
        for (char c : rawData) {
            if (c == ',' || c == '\n' || c == '\r') {
                cleaned += ' ';
            } else {
                cleaned += c;
            }
        }
        std::istringstream stream(cleaned);
        int idx = 0;
        unsigned int val;
        while (stream >> val && idx < numTiles) {
            int rawGid = static_cast<int>(val);
            int gid = rawGid & 0x1FFFFFFF;
            if (gid == 0) {
                outTiles[idx] = 324;
            } else {
                outTiles[idx] = gid - firstGid;
            }
            ++idx;
        }
        return idx == numTiles;
    } else {
        auto decoded = decodeBase64(rawData);
        auto decompressed = decompressZlib(decoded);
        if (static_cast<int>(decompressed.size()) < numTiles * 4) return false;
        const uint32_t* gids = reinterpret_cast<const uint32_t*>(decompressed.data());
        for (int i = 0; i < numTiles; ++i) {
            int gid = static_cast<int>(gids[i] & 0x1FFFFFFF);
            if (gid == 0) {
                outTiles[i] = 324;
            } else {
                outTiles[i] = gid - firstGid;
            }
        }
        return true;
    }
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

    int numTiles = out.width * out.height;

    // Parse all tile layers
    out.tileLayers.clear();
    size_t searchPos = 0;
    while (true) {
        size_t layerPos = xml.find("<layer", searchPos);
        if (layerPos == std::string::npos) break;

        std::vector<int> layerTiles;
        if (parseLayerData(xml, layerPos, out.width, out.height, firstGid, layerTiles)) {
            out.tileLayers.push_back(std::move(layerTiles));
        }

        searchPos = layerPos + 6;
    }

    if (out.tileLayers.empty()) return false;

    // First layer is ground
    out.groundTiles = out.tileLayers[0];

    // Collision: derive from ground tiles
    out.collisionTiles.resize(numTiles);
    for (int i = 0; i < numTiles; ++i) {
        int t = out.groundTiles[i];
        out.collisionTiles[i] = (t == 324 || t == 96 || t == 289 || t == 307) ? 1 : 0;
    }

    // Parse object groups for collision rectangles
    out.collisionRects.clear();
    searchPos = 0;
    while (true) {
        size_t objPos = xml.find("<objectgroup", searchPos);
        if (objPos == std::string::npos) break;

        size_t objGroupEnd = xml.find("</objectgroup>", objPos);
        if (objGroupEnd == std::string::npos) break;

        std::string groupContent = xml.substr(objPos, objGroupEnd - objPos);

        // Parse optional offset on object group
        float groupOffX = 0.0f, groupOffY = 0.0f;
        std::string offXStr = extractTagAttr(groupContent, "<objectgroup", "offsetx");
        std::string offYStr = extractTagAttr(groupContent, "<objectgroup", "offsety");
        if (!offXStr.empty()) groupOffX = std::stof(offXStr);
        if (!offYStr.empty()) groupOffY = std::stof(offYStr);

        // Parse each object in this group
        size_t objSearch = 0;
        while (true) {
            size_t objStart = groupContent.find("<object", objSearch);
            if (objStart == std::string::npos) break;

            size_t objEnd = groupContent.find("/>", objStart);
            if (objEnd == std::string::npos) {
                objEnd = groupContent.find(">", objStart);
                if (objEnd == std::string::npos) break;
                objEnd = groupContent.find("</object>", objEnd);
                if (objEnd == std::string::npos) break;
            }

            std::string objTag = groupContent.substr(objStart, objEnd - objStart + 2);

            // Extract x, y, width, height
            std::string xStr = extractTagAttr(objTag, "<object", "x");
            std::string yStr = extractTagAttr(objTag, "<object", "y");
            std::string wStr2 = extractTagAttr(objTag, "<object", "width");
            std::string hStr2 = extractTagAttr(objTag, "<object", "height");

            float x = xStr.empty() ? 0.0f : std::stof(xStr);
            float y = yStr.empty() ? 0.0f : std::stof(yStr);
            float w = wStr2.empty() ? 32.0f : std::stof(wStr2);
            float h = hStr2.empty() ? 32.0f : std::stof(hStr2);

            out.collisionRects.push_back({x + groupOffX, y + groupOffY, w, h});

            objSearch = objEnd + 2;
        }

        searchPos = objGroupEnd + 14;
    }

    return true;
}
