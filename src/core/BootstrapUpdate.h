#pragma once
#include "Core.h"
namespace hjd {
QString bootstrapExecutable(const QString &payload);
QString bootstrapSeedReceipt(const QString &seed);
void rememberBootstrap(const QString &root);
bool bootstrapCurrent(const QString &root,const QString &payload);
void beginBootstrapUpdate(const QString &root,const QString &payload);
void recoverBootstrapUpdate(const QString &root);
void commitBootstrapUpdate(const QString &root);
}
