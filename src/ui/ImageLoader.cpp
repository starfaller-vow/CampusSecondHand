#include "ImageLoader.h"
#include "ClientConnector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

QString ensureLocalImage(const QString &relPath)
{
    if (relPath.isEmpty()) return {};

    // 只取文件名，强制落到本地 images/ 目录（双端 exe 同目录也不影响）
    const QString fname     = QFileInfo(relPath).fileName();
    const QString localPath = QStringLiteral("images/") + fname;
    if (QFile::exists(localPath)) return localPath;

    // 本地没有 → 从服务端下载
    QByteArray bytes;
    QString err;
    if (!ClientConnector::instance().downloadImage(fname, bytes, err)) {
        qWarning() << "[ImageLoader] 下载失败" << fname << ":" << err;
        return {};
    }

    QDir().mkpath(QStringLiteral("images"));
    QFile f(localPath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(bytes);
        f.close();
    } else {
        qWarning() << "[ImageLoader] 写缓存失败:" << f.errorString();
        return {};
    }
    return localPath;
}