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
        if (ret == Z_OK) { out.resize(destLen); return out; }
        if (ret == Z_BUF_ERROR) { destLen *= 2; out.resize(destLen); continue; }
        throw std::runtime_error("Zlib decompression failed");
    }
}

static std::string extractAttr(const std::string& str, size_t pos, const std::string& attr) {
    std::string search = attr + "=\"";
    size_t p = str.find(search, pos);
    if (p == std::string::npos) return {};
    p += search.size();
    size_t end = str.find('"', p);
    if (end == std::string::npos) return {};
    return str.substr(p, end - p);
}

static std::string extractContent(const std::string& xml, size_t startPos) {
    size_t gt = xml.find('>', startPos);
    if (gt == std::string::npos) return {};
    size_t close = xml.find("</", gt + 1);
    if (close == std::string::npos) return {};
    return xml.substr(gt + 1, close - gt - 1);
}

static bool parseTileLayer(const std::string& xml, size_t layerPos,
                            int width, int height, int firstGid,
                            std::vector<int>& outTiles) {
    size_t dataStart = xml.find("<data", layerPos);
    if (dataStart == std::string::npos) return false;
    size_t dataTagEnd = xml.find('>', dataStart);
    if (dataTagEnd == std::string::npos) return false;
    std::string tagContent = xml.substr(dataStart, dataTagEnd - dataStart);

    int numTiles = width * height;
    outTiles.assign(numTiles, 0);
    bool isCsv = (tagContent.find("csv") != std::string::npos);

    std::string rawData = extractContent(xml, dataStart);
    size_t closeData = xml.find("</data>", dataStart);
    if (closeData != std::string::npos)
        rawData = xml.substr(dataTagEnd + 1, closeData - dataTagEnd - 1);
    if (rawData.empty()) return false;

    if (isCsv) {
        std::string cleaned;
        for (char c : rawData) {
            if (c == ',' || c == '\n' || c == '\r') cleaned += ' ';
            else cleaned += c;
        }
        std::istringstream stream(cleaned);
        int idx = 0;
        unsigned int val;
        while (stream >> val && idx < numTiles) {
            int rawGid = static_cast<int>(val);
            int gid = rawGid & 0x1FFFFFFF;
            outTiles[idx] = (gid == 0) ? -1 : gid - firstGid;
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
            outTiles[i] = (gid == 0) ? -1 : gid - firstGid;
        }
        return true;
    }
}

static TmxObject parseObject(const std::string& objTag) {
    TmxObject obj;
    std::string xs = extractAttr(objTag, 0, "x");
    std::string ys = extractAttr(objTag, 0, "y");
    std::string ws = extractAttr(objTag, 0, "width");
    std::string hs = extractAttr(objTag, 0, "height");
    std::string nm = extractAttr(objTag, 0, "name");
    std::string tp = extractAttr(objTag, 0, "type");
    if (!xs.empty()) obj.x = std::stof(xs);
    if (!ys.empty()) obj.y = std::stof(ys);
    if (!ws.empty()) obj.width = std::stof(ws);
    if (!hs.empty()) obj.height = std::stof(hs);
    obj.name = nm;
    obj.type = tp;

    // Parse properties
    size_t propsPos = objTag.find("<properties");
    if (propsPos != std::string::npos) {
        size_t propsEnd = objTag.find("</properties>", propsPos);
        std::string propsContent = (propsEnd != std::string::npos)
            ? objTag.substr(propsPos, propsEnd - propsPos) : "";
        size_t searchPos = 0;
        while (true) {
            size_t propStart = propsContent.find("<property", searchPos);
            if (propStart == std::string::npos) break;
            std::string pn = extractAttr(propsContent, propStart, "name");
            std::string pv = extractAttr(propsContent, propStart, "value");
            if (!pn.empty())
                obj.properties[pn] = pv;
            searchPos = propStart + 9;
        }
    }
    return obj;
}

bool loadTmx(const std::string& filepath, TmxMapData& out) {
    std::ifstream file(filepath);
    if (!file) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string xml = buffer.str();

    std::string wStr = extractAttr(xml, 0, "width");
    std::string hStr = extractAttr(xml, 0, "height");
    if (wStr.empty() || hStr.empty()) return false;
    out.width = std::stoi(wStr);
    out.height = std::stoi(hStr);

    std::string gidStr = extractAttr(xml, 0, "firstgid");
    int firstGid = gidStr.empty() ? 1 : std::stoi(gidStr);

    // Parse tile layers (named)
    size_t searchPos = 0;
    while (true) {
        size_t layerPos = xml.find("<layer", searchPos);
        if (layerPos == std::string::npos) break;
        std::string layerName = extractAttr(xml, layerPos, "name");

        std::vector<int> tiles;
        if (parseTileLayer(xml, layerPos, out.width, out.height, firstGid, tiles)) {
            if (layerName == "ground")
                out.groundTiles = std::move(tiles);
            else if (layerName == "walls")
                out.wallTiles = std::move(tiles);
        }
        searchPos = layerPos + 6;
    }

    // Parse object groups
    searchPos = 0;
    while (true) {
        size_t ogPos = xml.find("<objectgroup", searchPos);
        if (ogPos == std::string::npos) break;
        std::string ogName = extractAttr(xml, ogPos, "name");

        size_t ogEnd = xml.find("</objectgroup>", ogPos);
        if (ogEnd == std::string::npos) break;
        std::string ogContent = xml.substr(ogPos, ogEnd - ogPos);

        // Parse objects in this group
        size_t objSearch = 0;
        while (true) {
            size_t objStart = ogContent.find("<object ", objSearch);
            if (objStart == std::string::npos) break;
            size_t objEnd = ogContent.find("/>", objStart);
            if (objEnd == std::string::npos || objEnd > ogContent.find(">", objStart) + 50) {
                objEnd = ogContent.find(">", objStart);
                if (objEnd == std::string::npos) break;
                objEnd = ogContent.find("</object>", objEnd);
                if (objEnd == std::string::npos) break;
                objEnd += 9;
            } else {
                objEnd += 2;
            }
            std::string objTag = ogContent.substr(objStart, objEnd - objStart);
            TmxObject obj = parseObject(objTag);

            if (ogName == "transitions")
                out.transitions.push_back(obj);
            else if (ogName == "trees")
                out.trees.push_back(obj);
            else if (ogName == "fence")
                out.fenceRects.push_back(obj);
            else if (ogName == "spawn")
                out.spawn = obj;

            objSearch = objEnd;
        }
        searchPos = ogEnd + 14;
    }

    return !out.groundTiles.empty();
}
