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
    
    return grid;
}

/**
 * 销毁空间网格
 */
void DestroySpatialGrid(SpatialGrid* grid) {
    if (grid) {
        grid->cells.clear();
        delete grid;
    }
}

/**
 * 从砖块数组初始化网格（游戏启动时调用）
 */
void InitializeGridFromBricks(SpatialGrid* grid, int bricks[10][14]) {
    // 清空所有网格单元
    for (int i = 0; i < grid->gridWidth * grid->gridHeight; i++) {
        grid->cells[i].brickIndices.clear();
    }
    
    // 遍历所有砖块，将其加入对应的网格单元
    for (int i = 0; i < grid->brickRows; i++) {
        for (int j = 0; j < grid->brickCols; j++) {
            if (bricks[i][j]) {
                UpdateGridCell(grid, i, j, true);
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
    // 计算砖块的世界坐标（中心）
    float brickCenterX = brickJ * grid->brickW + grid->brickW / 2.0f;
    float brickCenterY = brickI * grid->brickH + grid->brickH / 2.0f;
    
    // 计算砖块所属的网格单元
    int gridX = (int)(brickCenterX / grid->cellWidth);
    int gridY = (int)(brickCenterY / grid->cellHeight);
    
    // 边界检查
    gridX = std::max(0, std::min(gridX, grid->gridWidth - 1));
    gridY = std::max(0, std::min(gridY, grid->gridHeight - 1));
    
    int cellIndex = gridY * grid->gridWidth + gridX;
    int brickIndex = brickI * grid->brickCols + brickJ;
    
    if (isActive) {
        // 添加砖块到网格单元（避免重复）
        auto& indices = grid->cells[cellIndex].brickIndices;
        auto it = std::find(indices.begin(), indices.end(), brickIndex);
        if (it == indices.end()) {
            indices.push_back(brickIndex);
        }
    } else {
        // 从网格单元中移除砖块
        auto& indices = grid->cells[cellIndex].brickIndices;
        auto it = std::find(indices.begin(), indices.end(), brickIndex);
        if (it != indices.end()) {
            indices.erase(it);
        }
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
std::vector<std::pair<int,int>> GetBricksInRadius(SpatialGrid* grid, Vector2 ballPos, float radius) {
    std::vector<std::pair<int,int>> result;
    
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
    
    // 收集所有可能的砖块
    std::set<int> brickSet;  // 使用set避免重复
    for (int gy = minGridY; gy <= maxGridY; gy++) {
        for (int gx = minGridX; gx <= maxGridX; gx++) {
            int cellIndex = gy * grid->gridWidth + gx;
            const auto& indices = grid->cells[cellIndex].brickIndices;
            for (int idx : indices) {
                brickSet.insert(idx);
            }
        }
    }
    
    // 转换为坐标对
    for (int idx : brickSet) {
        int i = idx / grid->brickCols;
        int j = idx % grid->brickCols;
        result.push_back({i, j});
    }
    
    return result;
}

/**
 * 调试绘制网格（用于验证网格系统）
 */
void DebugDrawGrid(SpatialGrid* grid) {
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