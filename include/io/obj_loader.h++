//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_OBJ_LOADER_H
#define DUSK_OBJ_LOADER_H

#include "math/Vec3.h++"
#include "model/vector_model.h++"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

/** Options used while converting OBJ data into local vector-model data. */
struct ObjLoadOptions {
    /** Scales every OBJ vertex after flips and rotation. */
    float scale = 1.f;

    /** Adds an offset after all import transforms and optional origin centering. */
    Vec3 offset;

    /** Rotates every OBJ vertex around local X/Y/Z axes in degrees before scaling. */
    Vec3 rotationDegrees;

    /** Recenters the model around its transformed bounding-box center. */
    bool centerOnOrigin = true;

    /** Flips OBJ coordinates when adapting models from opposite directions. */
    bool flipZ = false;
    bool flipX = false;
    bool flipY = false;
};

/** Returns true when coordinate flips mirror the model and invert face winding. */
inline bool reversesObjWinding(const ObjLoadOptions& options)
{
    const int flippedAxes =
        (options.flipX ? 1 : 0) +
        (options.flipY ? 1 : 0) +
        (options.flipZ ? 1 : 0);

    return flippedAxes % 2 != 0;
}

/** Converts degrees into radians for import-time model rotation. */
inline float degreesToRadians(float degrees)
{
    constexpr float pi = 3.14159265358979323846f;
    return degrees * pi / 180.f;
}

/** Rotates a local OBJ vertex around X, then Y, then Z. */
inline Vec3 rotateObjVertex(Vec3 vertex, const Vec3& rotationDegrees)
{
    const float rotationX = degreesToRadians(rotationDegrees.x);
    const float sinX = std::sin(rotationX);
    const float cosX = std::cos(rotationX);
    vertex =
    {
        vertex.x,
        cosX * vertex.y - sinX * vertex.z,
        sinX * vertex.y + cosX * vertex.z
    };

    const float rotationY = degreesToRadians(rotationDegrees.y);
    const float sinY = std::sin(rotationY);
    const float cosY = std::cos(rotationY);
    vertex =
    {
        cosY * vertex.x + sinY * vertex.z,
        vertex.y,
        -sinY * vertex.x + cosY * vertex.z
    };

    const float rotationZ = degreesToRadians(rotationDegrees.z);
    const float sinZ = std::sin(rotationZ);
    const float cosZ = std::cos(rotationZ);
    return
    {
        cosZ * vertex.x - sinZ * vertex.y,
        sinZ * vertex.x + cosZ * vertex.y,
        vertex.z
    };
}

/** Applies per-vertex import transforms before model-level centering and offset. */
inline Vec3 transformObjVertex(Vec3 vertex, const ObjLoadOptions& options)
{
    if (options.flipZ)
        vertex.z = -vertex.z;

    if (options.flipY)
        vertex.y = -vertex.y;

    if (options.flipX)
        vertex.x = -vertex.x;

    vertex = rotateObjVertex(vertex, options.rotationDegrees);
    return vertex * options.scale;
}

/** Recenters a transformed vector model so its bounding box surrounds local origin. */
inline void centerModelOnOrigin(VectorModel& model)
{
    if (model.vertices.empty())
        return;

    Vec3 minimum = model.vertices.front();
    Vec3 maximum = model.vertices.front();

    for (const Vec3& vertex : model.vertices)
    {
        minimum.x = std::min(minimum.x, vertex.x);
        minimum.y = std::min(minimum.y, vertex.y);
        minimum.z = std::min(minimum.z, vertex.z);
        maximum.x = std::max(maximum.x, vertex.x);
        maximum.y = std::max(maximum.y, vertex.y);
        maximum.z = std::max(maximum.z, vertex.z);
    }

    const Vec3 center = (minimum + maximum) * 0.5f;

    for (Vec3& vertex : model.vertices)
        vertex -= center;
}

/** Applies a final local-space offset to every vector model vertex. */
inline void offsetModel(VectorModel& model, const Vec3& offset)
{
    for (Vec3& vertex : model.vertices)
        vertex += offset;
}

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

/** Adds one triangle face, skipping degenerate triangles and preserving corrected winding. */
inline void addObjFace(VectorModel& model, int a, int b, int c, bool reverseWinding)
{
    if (a == b || b == c || a == c)
        return;

    if (reverseWinding)
        std::swap(b, c);

    model.faces.push_back({a, b, c});
}

/** Reads OBJ text from a stream and converts vertices/faces/lines into a wire model. */
inline std::optional<VectorModel> loadObjStreamAsVectorModel(
    std::istream& input,
    const ObjLoadOptions& options = {}
)
{
    VectorModel model;
    std::set<std::pair<int, int>> edges;
    const bool reverseWinding = reversesObjWinding(options);
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

            model.vertices.push_back(transformObjVertex(vertex, options));
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
            {
                addObjEdge(model, edges, indices.back(), indices.front());

                for (std::size_t i = 2; i < indices.size(); ++i)
                    addObjFace(model, indices[0], indices[i - 1], indices[i], reverseWinding);
            }
        }
    }

    if (model.vertices.empty())
        return std::nullopt;

    if (options.centerOnOrigin)
        centerModelOnOrigin(model);

    offsetModel(model, options.offset);

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
