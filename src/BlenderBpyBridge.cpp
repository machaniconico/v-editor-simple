#include "BlenderBpyBridge.h"

#include <QProcess>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTextStream>
#include <QDebug>

namespace blender::bridge {

// ---------------------------------------------------------------------------
// runBpyScript
// ---------------------------------------------------------------------------

BridgeResult runBpyScript(const QString &blenderExePath,
                           const QString &scriptPath,
                           const QStringList &args,
                           int timeoutMs)
{
    BridgeResult result;

    // AC-3: guard against empty or missing blender executable
    if (blenderExePath.isEmpty() || !QFileInfo::exists(blenderExePath)) {
        qWarning() << "BlenderBpyBridge: blenderExePath invalid or not found:"
                   << blenderExePath;
        result.exitCode = -1;
        return result;
    }

    // Build argument list: --background --python <script> -- <args...>
    QStringList procArgs;
    procArgs << QStringLiteral("--background")
             << QStringLiteral("--python")
             << scriptPath
             << QStringLiteral("--")
             << args;

    QProcess proc;
    proc.setProgram(blenderExePath);
    proc.setArguments(procArgs);
    proc.start();

    if (!proc.waitForFinished(timeoutMs)) {
        // Timeout: kill the process
        qWarning() << "BlenderBpyBridge: process timed out after" << timeoutMs << "ms, killing.";
        proc.kill();
        proc.waitForFinished(2000);
        result.exitCode = -1;
        result.stdoutData = QString::fromLocal8Bit(proc.readAllStandardOutput());
        result.stderrData = QString::fromLocal8Bit(proc.readAllStandardError());
        return result;
    }

    result.exitCode    = proc.exitCode();
    result.stdoutData  = QString::fromLocal8Bit(proc.readAllStandardOutput());
    result.stderrData  = QString::fromLocal8Bit(proc.readAllStandardError());
    return result;
}

// ---------------------------------------------------------------------------
// convertBlendToGltf
// ---------------------------------------------------------------------------

// Embedded bpy script that opens a .blend and exports to glTF.
static const char kExportScript[] =
    "import bpy, sys\n"
    "args = sys.argv[sys.argv.index(\"--\")+1:]\n"
    "bpy.ops.wm.open_mainfile(filepath=args[0])\n"
    "bpy.ops.export_scene.gltf(filepath=args[1])\n";

BridgeResult convertBlendToGltf(const QString &blendPath,
                                 const QString &outGltfPath,
                                 const QString &blenderExePath)
{
    // Write the embedded script to a temporary file
    QTemporaryFile tmpScript;
    tmpScript.setFileTemplate(QStringLiteral("/tmp/veditor_bpy_XXXXXX.py"));
    tmpScript.setAutoRemove(true);

    if (!tmpScript.open()) {
        qWarning() << "BlenderBpyBridge: failed to create temporary script file.";
        BridgeResult r;
        r.exitCode = -1;
        r.stderrData = QStringLiteral("Failed to create temporary bpy script file.");
        return r;
    }

    {
        QTextStream ts(&tmpScript);
        ts << kExportScript;
        ts.flush();
    }
    tmpScript.close();

    QStringList args;
    args << blendPath << outGltfPath;

    return runBpyScript(blenderExePath, tmpScript.fileName(), args);
}

} // namespace blender::bridge
