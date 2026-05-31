#ifndef TOOL_H
#define TOOL_H

#include "raylib.h"
#include <vector>

typedef enum {
    TOOL_MULTI_BALL,
    TOOL_BOMB,
    TOOL_SLOW
} ToolType;

typedef struct {
    ToolType type;
    Vector2 position;
    bool active;
} Tool;

Tool CreateTool(Vector2 pos, ToolType type);
void UpdateTool(Tool *t, float dt);
void DrawTool(Tool t);
void ApplyTool(Tool t, float *ballSpeed);

// ==================== 空间分割网格系统 ====================
// 定义网格单元的数据结构
typedef struct {
    std::vector<int> brickIndices;  // 存储该网格单元中砖块的索引对 (i*brickCols + j)
} GridCell;

// 定义空间网格管理器
typedef struct {
    int gridWidth;              // 网格列数
    int gridHeight;             // 网格行数
    float cellWidth;            // 每个网格单元的像素宽度
    float cellHeight;           // 每个网格单元的像素高度
    std::vector<GridCell> cells;  // 网格单元数组 (一维存储，行优先)
    int brickRows;              // 砖块总行数
    int brickCols;              // 砖块总列数
    int brickW;                 // 砖块宽度
    int brickH;                 // 砖块高度
} SpatialGrid;

// 函数声明
SpatialGrid* CreateSpatialGrid(int screenWidth, int screenHeight, 
                                int brickRows, int brickCols, 
                                int brickW, int brickH,
                                int gridWidth = 4, int gridHeight = 4);
void DestroySpatialGrid(SpatialGrid* grid);
void InitializeGridFromBricks(SpatialGrid* grid, int bricks[10][14]);
void UpdateGridCell(SpatialGrid* grid, int brickI, int brickJ, bool isActive);
std::vector<std::pair<int,int>> GetBricksInRadius(SpatialGrid* grid, Vector2 ballPos, float radius);
void DebugDrawGrid(SpatialGrid* grid);  // 调试用：绘制网格线

#endif