#include "Paddle.h"

// 构造函数实现
Paddle::Paddle(float px, float py, float pw, float ph) {
    x = px;
    y = py;
    width = pw;
    height = ph;
}

// 绘制挡板
void Paddle::Draw() {
    DrawRectangle(x, y, width, height, BLUE);
}

// 向左移动
void Paddle::MoveLeft(float speed) {
    if (x > 0) { // 不超出左边界
        x -= speed;
    }
}

// 向右移动
void Paddle::MoveRight(float speed) {
    if (x + width < 800) { // 不超出右边界（屏幕宽800）
        x += speed;
    }
}