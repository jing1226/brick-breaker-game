#ifndef BALL_H
#define BALL_H

#include "raylib.h"

extern Vector2 ballPosition;
extern Vector2 ballVelocity;
extern float ballRadius;
extern const float gravity;
extern const float bounceFactor;

void resetBall(int screenWidth, int screenHeight, float initialYVel = 0.0f);

#endif