#include "Brick.h"

// 🌟 修改构造函数：使用外部传入的颜色
Brick::Brick(float bx, float by, float bw, float bh, Color c) {
    x = bx;
    y = by;
    width = bw;
    height = bh;
    color = c; // 用传入的颜色（每行固定），不再随机
}

// 绘制砖块（带黑色边框）
void Brick::Draw() {
    DrawRectangle(x, y, width, height, color);          // 砖块本体
    DrawRectangleLines(x, y, width, height, BLACK);     // 砖块边框
}