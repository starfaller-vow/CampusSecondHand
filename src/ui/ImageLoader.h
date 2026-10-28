#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include <QString>

// ============================================================
// 客户端图片加载工具（M5）
// 商品图片路径形如 "images/xxx.jpg"（服务端保存的相对路径），
// 本函数保证本地存在该图：
// · 本地已有 → 直接返回原路径
// · 本地没有 → 向服务端下载并缓存到本地 images/，失败返回空串
// 返回空串时调用方显示占位图
// ============================================================
QString ensureLocalImage(const QString &relPath);

#endif // IMAGELOADER_H