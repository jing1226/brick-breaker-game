#ifndef PADDLE_H
#define PADDLE_H

#include "raylib.h"

extern float paddleWidth;
extern float paddleHeight;
extern Vector2 paddlePosition;
extern float paddleSpeed;

void initPaddle(int sw, int sh);
void updatePaddle(int sw);
void drawPaddle();

#endif