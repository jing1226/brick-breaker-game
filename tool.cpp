#include "Tool.h"
#include <cmath>
#include <algorithm>

Tool CreateTool(Vector2 pos, ToolType type) {
    Tool t;
    t.type = type;
    t.position = pos;
    t.active = true;
    return t;
}

void UpdateTool(Tool *t, float dt) {
    if (!t->active) return;
    t->position.y += 100.0f * dt;
}

void DrawTool(Tool t) {
    if (!t.active) return;
    Color color;
    switch (t.type) {
        case TOOL_MULTI_BALL: color = BLUE; break;
        case TOOL_BOMB: color = RED; break;
        case TOOL_SLOW: color = GREEN; break;
        default: color = WHITE; break;
    }
    DrawCircleV(t.position, 15, color);
    DrawCircleLinesV(t.position, 15, BLACK);
}

void ApplyTool(Tool t, float *ballSpeed) {
    switch (t.type) {
        case TOOL_MULTI_BALL:
            // 多球效果（这里先占位，后续可以扩展）
            break;
        case TOOL_BOMB:
            // 炸弹效果（这里先占位，后续可以扩展）
            break;
        case TOOL_SLOW:
            *ballSpeed *= 0.5f;
            break;
    }
}

// ==================== 空间分割网格系统实现 ====================

/**
 * 创建空间网格
 * @param screenWidth 屏幕宽度
 * @param screenHeight 屏幕高度
 * @param brickRows 砖块行数
 * @param brickCols 砖块列数
 * @param brickW 砖块宽度
 * @param brickH 砖块高度
 * @param gridWidth 网格分割数（默认4×4）
 * @param gridHeight 网格分割数（默认4×4）
 */
SpatialGrid* CreateSpatialGrid(int screenWidth, int screenHeight, 
                                int brickRows, int brickCols, 
                                int brickW, int brickH,
                                int gridWidth, int gridHeight) {
    SpatialGrid* grid = new SpatialGrid();
    grid->gridWidth = gridWidth;
    grid->gridHeight = gridHeight;
    grid->brickRows = brickRows;
    grid->brickCols = brickCols;
    grid->brickW = brickW;
    grid->brickH = brickH;
    
    // 计算每个网格单元的大小
    grid->cellWidth = (float)screenWidth / gridWidth;
    grid->cellHeight = (float)screenHeight / gridHeight;
    
    // 初始化网格单元
    grid->cells.resize(gridWidth * gridHeight);
    // 初始化砖块到网格的映射表，默认未映射
    grid->brickToCell.assign(grid->brickRows * grid->brickCols, -1);
    // 初始化重用访问标记
    grid->visited.assign(grid->brickRows * grid->brickCols, 0);
    grid->visitedList.clear();
    // 预估每个单元的容纳空间以减少重分配
    int avg = std::max(1, (grid->brickRows * grid->brickCols) / (gridWidth * gridHeight));
    for (auto &cell : grid->cells) cell.brickIndices.reserve(avg);
    
    return grid;
}

/**
 * 销毁空间网格
 */
void DestroySpatialGrid(SpatialGrid* grid) {
    if (grid) {
        grid->cells.clear();
        grid->brickToCell.clear();
        delete grid;
    }
}

/**
 * 从砖块数组初始化网格（游戏启动时调用）
 */
void InitializeGridFromBricks(SpatialGrid* grid, int bricks[10][14]) {
    std::lock_guard<std::mutex> guard(grid->mtx);
    // 清空所有网格单元
    for (int i = 0; i < grid->gridWidth * grid->gridHeight; i++) {
        grid->cells[i].brickIndices.clear();
    }
    // 初始化映射表
    grid->brickToCell.assign(grid->brickRows * grid->brickCols, -1);
    // 遍历所有砖块，将其加入对应的网格单元
    for (int i = 0; i < grid->brickRows; i++) {
        for (int j = 0; j < grid->brickCols; j++) {
            if (bricks[i][j]) {
                // 计算砖块的世界坐标（中心）
                float brickCenterX = j * grid->brickW + grid->brickW / 2.0f;
                float brickCenterY = i * grid->brickH + grid->brickH / 2.0f;
                int gridX = (int)(brickCenterX / grid->cellWidth);
                int gridY = (int)(brickCenterY / grid->cellHeight);
                gridX = std::max(0, std::min(gridX, grid->gridWidth - 1));
                gridY = std::max(0, std::min(gridY, grid->gridHeight - 1));
                int cellIndex = gridY * grid->gridWidth + gridX;
                int brickIndex = i * grid->brickCols + j;
                grid->cells[cellIndex].brickIndices.push_back(brickIndex);
                grid->brickToCell[brickIndex] = cellIndex;
            }
        }
    }
}

/**
 * 更新单个砖块在网格中的位置
 * @param grid 空间网格指针
 * @param brickI 砖块行索引
 * @param brickJ 砖块列索引
 * @param isActive 砖块是否活跃（存在）
 */
void UpdateGridCell(SpatialGrid* grid, int brickI, int brickJ, bool isActive) {
    std::lock_guard<std::mutex> guard(grid->mtx);
    int brickIndex = brickI * grid->brickCols + brickJ;
    // 计算砖块的目标单元（基于当前位置）
    float brickCenterX = brickJ * grid->brickW + grid->brickW / 2.0f;
    float brickCenterY = brickI * grid->brickH + grid->brickH / 2.0f;
    int targetGridX = (int)(brickCenterX / grid->cellWidth);
    int targetGridY = (int)(brickCenterY / grid->cellHeight);
    targetGridX = std::max(0, std::min(targetGridX, grid->gridWidth - 1));
    targetGridY = std::max(0, std::min(targetGridY, grid->gridHeight - 1));
    int targetCell = targetGridY * grid->gridWidth + targetGridX;
    int prevCell = -1;
    if ((size_t)brickIndex < grid->brickToCell.size()) prevCell = grid->brickToCell[brickIndex];

    if (isActive) {
        // 如果之前映射在别的单元，先移除
        if (prevCell != -1 && prevCell != targetCell) {
            auto &oldIndices = grid->cells[prevCell].brickIndices;
            auto it = std::find(oldIndices.begin(), oldIndices.end(), brickIndex);
            if (it != oldIndices.end()) oldIndices.erase(it);
        }
        // 添加到目标单元（若不存在）
        auto &indices = grid->cells[targetCell].brickIndices;
        if (std::find(indices.begin(), indices.end(), brickIndex) == indices.end()) {
            indices.push_back(brickIndex);
        }
        grid->brickToCell[brickIndex] = targetCell;
    } else {
        // 移除砖块（从之前映射的单元中）
        if (prevCell != -1 && prevCell < (int)grid->cells.size()) {
            auto &indices = grid->cells[prevCell].brickIndices;
            // 快速移除：交换并弹出
            for (size_t k = 0; k < indices.size(); ++k) {
                if (indices[k] == brickIndex) {
                    indices[k] = indices.back();
                    indices.pop_back();
                    break;
                }
            }
        } else {
            auto &indices = grid->cells[targetCell].brickIndices;
            for (size_t k = 0; k < indices.size(); ++k) {
                if (indices[k] == brickIndex) {
                    indices[k] = indices.back();
                    indices.pop_back();
                    break;
                }
            }
        }
        grid->brickToCell[brickIndex] = -1;
    }
}

/**
 * 获取球周围范围内的所有砖块
 * 包括球所在网格及相邻8个网格（边界情况处理）
 * @param grid 空间网格指针
 * @param ballPos 球的位置
 * @param radius 球的半径
 * @return 返回砖块坐标对的向量 (i, j)
 */
void GetBricksInRadius(SpatialGrid* grid, Vector2 ballPos, float radius, std::vector<std::pair<int,int>>& out) {
    out.clear();
    std::lock_guard<std::mutex> guard(grid->mtx);
    // 确定球所在的主网格单元
    int centerGridX = (int)(ballPos.x / grid->cellWidth);
    int centerGridY = (int)(ballPos.y / grid->cellHeight);
    centerGridX = std::max(0, std::min(centerGridX, grid->gridWidth - 1));
    centerGridY = std::max(0, std::min(centerGridY, grid->gridHeight - 1));
    // 获取可能与球碰撞的网格范围（包括相邻网格）
    int minGridX = std::max(0, centerGridX - 1);
    int maxGridX = std::min(grid->gridWidth - 1, centerGridX + 1);
    int minGridY = std::max(0, centerGridY - 1);
    int maxGridY = std::min(grid->gridHeight - 1, centerGridY + 1);
    // 收集所有可能的砖块（使用 visited 标记避免重复，并记录以便清理）
    for (int gy = minGridY; gy <= maxGridY; gy++) {
        for (int gx = minGridX; gx <= maxGridX; gx++) {
            int cellIndex = gy * grid->gridWidth + gx;
            const auto& indices = grid->cells[cellIndex].brickIndices;
            for (int idx : indices) {
                if (idx < 0 || idx >= (int)grid->visited.size()) continue;
                if (!grid->visited[idx]) {
                    grid->visited[idx] = 1;
                    grid->visitedList.push_back(idx);
                    int i = idx / grid->brickCols;
                    int j = idx % grid->brickCols;
                    out.push_back({i, j});
                }
            }
        }
    }
    // 清理 visited 标记
    for (int idx : grid->visitedList) grid->visited[idx] = 0;
    grid->visitedList.clear();
}

/**
 * 调试绘制网格（用于验证网格系统）
 */
void DebugDrawGrid(SpatialGrid* grid) {
    std::lock_guard<std::mutex> guard(grid->mtx);
    // 绘制网格线
    for (int i = 0; i <= grid->gridWidth; i++) {
        float x = i * grid->cellWidth;
        DrawLineV({x, 0}, {x, grid->gridHeight * grid->cellHeight}, 
                  {200, 200, 200, 100});
    }
    for (int j = 0; j <= grid->gridHeight; j++) {
        float y = j * grid->cellHeight;
        DrawLineV({0, y}, {grid->gridWidth * grid->cellWidth, y}, 
                  {200, 200, 200, 100});
    }
}