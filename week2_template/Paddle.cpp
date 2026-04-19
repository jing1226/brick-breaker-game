#include "Paddle.h"
#include "raylib.h"

float paddleWidth = 120;
float paddleHeight = 20;
Vector2 paddlePosition = {0,0};
float paddleSpeed = 8;

void initPaddle(int sw, int sh) {
    paddlePosition = (Vector2){(sw-paddleWidth)/2.0f, sh-30};
}

void updatePaddle(int sw) {
    if (IsKeyDown(KEY_LEFT)) paddlePosition.x -= paddleSpeed;
    if (IsKeyDown(KEY_RIGHT)) paddlePosition.x += paddleSpeed;
    if (paddlePosition.x < 0) paddlePosition.x=0;
    if (paddlePosition.x > sw-paddleWidth) paddlePosition.x=sw-paddleWidth;
}

void drawPaddle() {
    DrawRectangleV(paddlePosition, (Vector2){paddleWidth,paddleHeight}, (Color){255,223,0,255});
}