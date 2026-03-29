#include "Ball.h"

Ball::Ball(Vector2 pos, Vector2 sp, float r) {
    position = pos;
    speed = sp;
    radius = r;
}

void Ball::Move() {
    position.x += speed.x;
    position.y += speed.y;
}

// 改回画红色圆形小球
void Ball::Draw() {
    DrawCircleV(position, radius, RED);
}

// 改回小球的边界反弹逻辑
void Ball::BounceEdge(int screenWidth, int screenHeight) {
    if (position.x - radius <= 0 || position.x + radius >= screenWidth) {
        speed.x *= -1;
    }
    if (position.y - radius <= 0 || position.y + radius >= screenHeight) {
        speed.y *= -1;
    }
}