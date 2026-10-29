#include "resource_manager.h"
#include "raylib.h"
#include <iostream>
#include <thread>

ResourceManager* InitResourceManager() {
    ResourceManager* mgr = new ResourceManager();
    return mgr;
}

void DestroyResourceManager(ResourceManager* mgr) {
    if (!mgr) return;
    UnloadAllResources(mgr);
    delete mgr;
}

Texture2D LoadTextureCached(ResourceManager* mgr, const std::string& path) {
    if (!mgr) return LoadTexture(path.c_str());
    std::lock_guard<std::mutex> guard(mgr->mtx);
    auto it = mgr->textures.find(path);
    if (it != mgr->textures.end()) {
        return it->second;
    }
    Texture2D tex = LoadTexture(path.c_str());
    mgr->textures.emplace(path, tex);
    return tex;
}

void UnloadAllResources(ResourceManager* mgr) {
    if (!mgr) return;
    std::lock_guard<std::mutex> guard(mgr->mtx);
    for (auto &p : mgr->textures) {
        UnloadTexture(p.second);
    }
    mgr->textures.clear();
}

void PreloadTextureAsync(ResourceManager* mgr, const std::string& path) {
    if (!mgr) return;
    // 如果已存在则直接返回
    {
        std::lock_guard<std::mutex> guard(mgr->mtx);
        if (mgr->textures.find(path) != mgr->textures.end()) return;
    }
    // 后台线程加载纹理并缓存
    std::thread([mgr, path]() {
        Texture2D tex = LoadTexture(path.c_str());
        std::lock_guard<std::mutex> guard(mgr->mtx);
        // 可能在此期间已被加载，检查后插入
        if (mgr->textures.find(path) == mgr->textures.end()) {
            mgr->textures.emplace(path, tex);
        } else {
            // 如果已存在，释放新加载的纹理以避免泄漏
            UnloadTexture(tex);
        }
    }).detach();
}

bool IsTextureLoaded(ResourceManager* mgr, const std::string& path) {
    if (!mgr) return false;
    std::lock_guard<std::mutex> guard(mgr->mtx);
    return mgr->textures.find(path) != mgr->textures.end();
}

Texture2D GetTexture(ResourceManager* mgr, const std::string& path) {
    if (!mgr) return Texture2D{};
    std::lock_guard<std::mutex> guard(mgr->mtx);
    auto it = mgr->textures.find(path);
    if (it != mgr->textures.end()) return it->second;
    return Texture2D{};
}
