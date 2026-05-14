#pragma once

#include <QString>
#include <QStringList>

namespace blender::bridge {

// Result from running a bpy script via Blender subprocess
struct BridgeResult {
    int exitCode = -1;
    QString stdoutData;
    QString stderrData;
};

// Run `blender --background --python <scriptPath> -- <args>` as a subprocess.
// Returns BridgeResult with exitCode, stdout, stderr.
// If blenderExePath is empty or the path does not exist, returns exitCode=-1 immediately.
// If timeoutMs elapses before the process finishes, the process is killed.
BridgeResult runBpyScript(const QString &blenderExePath,
                           const QString &scriptPath,
                           const QStringList &args,
                           int timeoutMs = 30000);

// Convert a .blend file to glTF by running the embedded bpy export script.
// Uses a temporary script file. Returns the BridgeResult from runBpyScript.
BridgeResult convertBlendToGltf(const QString &blendPath,
                                 const QString &outGltfPath,
                                 const QString &blenderExePath);

} // namespace blender::bridge
