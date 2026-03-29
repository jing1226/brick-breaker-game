#ifndef BALL_H
#define BALL_H

#include "raylib.h"

class Ball {
public:
    Vector2 position;
    Vector2 speed;
    float radius; // 改回小球半径

    Ball(Vector2 pos, Vector2 sp, float r);

    void Move();
    void Draw();
    void BounceEdge(int screenWidth, int screenHeight);
};

#endif