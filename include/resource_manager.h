#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "raylib.h"
#include <string>
#include <unordered_map>
#include <mutex>

// 线程安全的资源管理（目前缓存 Texture2D）
struct ResourceManager {
    std::mutex mtx;
    std::unordered_map<std::string, Texture2D> textures;
};

ResourceManager* InitResourceManager();
void DestroyResourceManager(ResourceManager* mgr);
Texture2D LoadTextureCached(ResourceManager* mgr, const std::string& path);
void UnloadAllResources(ResourceManager* mgr);
// 异步预加载纹理（后台线程加载并缓存）
void PreloadTextureAsync(ResourceManager* mgr, const std::string& path);
// 查询纹理是否已加载
bool IsTextureLoaded(ResourceManager* mgr, const std::string& path);
// 获取已加载纹理（若不存在返回{}）
Texture2D GetTexture(ResourceManager* mgr, const std::string& path);

#endif
