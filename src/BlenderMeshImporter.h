#pragma once

#include <QString>
#include <QVector>
#include <QVector2D>
#include <QVector3D>

/*
 * BlenderMeshImporter — Sprint 16 US-BLE-1.
 *
 * namespace blender::mesh provides a single entry point loadMeshFile() that
 * dispatches by extension to a self-contained parser:
 *   - .gltf / .glb  : minimal glTF 2.0 reader (JSON + binary buffer view).
 *                     Extracts primitives[0].attributes.{POSITION, NORMAL,
 *                     TEXCOORD_0} and indices.
 *   - .obj          : minimal Wavefront OBJ reader (v / vn / vt / f).
 *                     Quads are split into 2 triangles.
 *   - .fbx / .abc   : libassimp via Importer::ReadFile() when
 *                     <assimp/Importer.hpp> is available at compile time.
 *                     When assimp is not available, returns an empty MeshData
 *                     with sourceFormat=Unknown (no exception).
 *   - other / missing : empty MeshData with sourceFormat=Unknown.
 *
 * The MeshData layout is compatible with mesh3d::TriMesh used by
 * Text3DLayer / ExtrudedMesh: vertices/normals are flat per-vertex arrays
 * and triangleIndices is a flat triplet array (size % 3 == 0).
 *
 * Failure mode: log via qWarning() and return an empty MeshData. No throws.
 */

namespace blender {
namespace mesh {

enum class SourceFormat {
    Unknown = 0,
    OBJ,
    GLTF,
    FBX,
    Alembic
};

struct MeshData {
    QVector<QVector3D> vertices;
    QVector<QVector3D> normals;
    QVector<QVector2D> uvs;
    QVector<int>       triangleIndices;   // flat triplet array (size % 3 == 0)
    QString            materialName;
    SourceFormat       sourceFormat = SourceFormat::Unknown;
};

// Load a mesh file from disk. Dispatches by extension. On any failure
// (missing file, unknown extension, parse error, optional libs unavailable)
// returns a MeshData with empty vertices/triangleIndices and an appropriate
// sourceFormat (Unknown if the extension itself is unknown / missing path).
MeshData loadMeshFile(const QString &path);

} // namespace mesh
} // namespace blender
