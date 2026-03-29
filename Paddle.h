#ifndef PADDLE_H
#define PADDLE_H

#include "raylib.h"

class Paddle {
public:
    float x, y;
    float width, height;

    // 构造函数
    Paddle(float px, float py, float pw, float ph);

    // 成员函数
    void Draw();
    void MoveLeft(float speed);
    void MoveRight(float speed);
};

#endif