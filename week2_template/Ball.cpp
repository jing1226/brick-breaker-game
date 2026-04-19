#include "Ball.h"
#include "raylib.h"

Vector2 ballPosition = { 0 };
Vector2 ballVelocity = { 0 };
float ballRadius = 10.0f;

const float gravity = 0.02f;
const float bounceFactor = 0.96f;

void resetBall(int screenWidth, int screenHeight) {
    ballPosition = (Vector2){ (float)screenWidth / 2.0f, (float)screenHeight - 100.0f };
    ballVelocity = (Vector2){ 0.0f, 0.0f };
}