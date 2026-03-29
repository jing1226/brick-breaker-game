#ifndef BRICK_H
#define BRICK_H

#include "raylib.h"

class Brick {
public:
    float x, y;          // 砖块坐标
    float width, height; // 砖块大小
    Color color;         // 砖块颜色（由外部传入，不再随机）

    // 🌟 修改构造函数：新增color参数，接收外部指定的颜色
    Brick(float bx, float by, float bw, float bh, Color c);

    // 绘制砖块
    void Draw();
};

#endif