#pragma once
#include <QStringList>
#include <functional>
using InstallHealthCheck=std::function<bool(const QString&,const QString&)>;
bool handleInstaller(const QStringList &arguments,const InstallHealthCheck &health);
