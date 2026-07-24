//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_OBJ_LOADER_H
#define DUSK_OBJ_LOADER_H

#include "math/Vec3.h++"
#include "model/vector_model.h++"
#include <algorithm>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

/** Options used while converting OBJ data into local vector-model data. */
struct ObjLoadOptions {
    /** Scales every OBJ vertex while loading. */
    float scale = 1.f;

    /** Adds an offset to every loaded vertex after scaling. */
    Vec3 offset;

    /** Flips OBJ coordinates when adapting models from opposite directions. */
    bool flipZ = false;
    bool flipX = false;
    bool flipY = false;
};

/** Converts OBJ vertex indices into zero-based indices, including negative relative indices. */
inline std::optional<int> parseObjIndex(const std::string& token, int vertexCount)
{
    if (token.empty())
        return std::nullopt;

    const std::size_t slash = token.find('/');
    const std::string indexText = token.substr(0, slash);

    if (indexText.empty())
        return std::nullopt;

    int objIndex = 0;
    std::istringstream stream(indexText);
    stream >> objIndex;

    if (!stream || objIndex == 0)
        return std::nullopt;

    const int zeroBased = objIndex > 0
        ? objIndex - 1
        : vertexCount + objIndex;

    if (zeroBased < 0 || zeroBased >= vertexCount)
        return std::nullopt;

    return zeroBased;
}

/** Adds one unique undirected edge to a vector model. */
inline void addObjEdge(VectorModel& model, std::set<std::pair<int, int>>& edges, int a, int b)
{
    if (a == b)
        return;

    const auto edge = std::minmax(a, b);

    if (!edges.insert(edge).second)
        return;

    model.lines.push_back({edge.first, edge.second, false});
}

/** Reads OBJ text from a stream and converts vertices/faces/lines into a wire model. */
inline std::optional<VectorModel> loadObjStreamAsVectorModel(
    std::istream& input,
    const ObjLoadOptions& options = {}
)
{
    VectorModel model;
    std::set<std::pair<int, int>> edges;
    std::string line;

    while (std::getline(input, line))
    {
        std::istringstream lineStream(line);
        std::string tag;
        lineStream >> tag;

        if (tag == "v")
        {
            Vec3 vertex;
            lineStream >> vertex.x >> vertex.y >> vertex.z;

            if (!lineStream)
                continue;

            if (options.flipZ)
                vertex.z = -vertex.z;

            if (options.flipY)
                vertex.y = -vertex.y;

            if (options.flipX)
                vertex.x = -vertex.x;

            model.vertices.push_back(vertex * options.scale + options.offset);
        }
        else if (tag == "f" || tag == "l")
        {
            std::vector<int> indices;
            std::string token;

            while (lineStream >> token)
            {
                if (const auto index = parseObjIndex(token, static_cast<int>(model.vertices.size())))
                    indices.push_back(*index);
            }

            for (std::size_t i = 1; i < indices.size(); ++i)
                addObjEdge(model, edges, indices[i - 1], indices[i]);

            if (tag == "f" && indices.size() > 2)
                addObjEdge(model, edges, indices.back(), indices.front());
        }
    }

    if (model.vertices.empty())
        return std::nullopt;

    return model;
}

/** Loads an OBJ file from disk and converts it into the engine's vector model format. */
inline std::optional<VectorModel> loadObjFileAsVectorModel(
    const std::string& path,
    const ObjLoadOptions& options = {}
)
{
    std::ifstream file(path);

    if (!file)
        return std::nullopt;

    return loadObjStreamAsVectorModel(file, options);
}

#endif //DUSK_OBJ_LOADER_H
