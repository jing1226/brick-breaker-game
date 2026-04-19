#ifndef BRICK_H
#define BRICK_H

#include "raylib.h"

extern int brickRows;
extern const int brickCols;
extern int brickW;
extern int brickH;
extern int bricks[10][14];
extern int goldenBrickI;
extern int goldenBrickJ;
extern Color brickColors[10];

void initBricks();
void drawBricks();
bool isGoldenBrick(int i, int j);
int getBrickScore(int row);

#endif