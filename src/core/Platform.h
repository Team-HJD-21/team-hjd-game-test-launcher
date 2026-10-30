#pragma once
#include <QString>
#include <QtGlobal>

namespace hjd {
inline QString clientExecutable() {
#ifdef Q_OS_WIN
    return QStringLiteral("TeamHJDClient.exe");
#else
    return QStringLiteral("TeamHJDClient.app/Contents/MacOS/TeamHJDClient");
#endif
}
inline QString platformId() {
#ifdef Q_OS_WIN
    return QStringLiteral("windows-x64");
#elif defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("macos-arm64");
#else
    return QStringLiteral("macos-x64");
#endif
}
inline QString platformLabel() {
#ifdef Q_OS_WIN
    return QStringLiteral("Windows x64");
#elif defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("macOS · Apple Silicon");
#else
    return QStringLiteral("macOS · Intel");
#endif
}
inline QString launcherPayloadName() {
#ifdef Q_OS_WIN
    return QStringLiteral("TeamHJD-LauncherPayload-Windows-x64.zip");
#elif defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("TeamHJD-LauncherPayload-macOS-arm64.zip");
#else
    return QStringLiteral("TeamHJD-LauncherPayload-macOS-x64.zip");
#endif
}
}
