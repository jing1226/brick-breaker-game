#include "Brick.h"
#include "raylib.h"

int brickRows = 5;
const int brickCols = 14;
int brickW = 57;
int brickH = 28;
int bricks[10][14];
int goldenBrickI;
int goldenBrickJ;

Color brickColors[10] = {
    {180,200,255,255}, {160,240,180,255},
    {255,210,160,255}, {255,180,200,255},
    {160,200,240,255}, {200,180,220,255}
};

void initBricks() {
    for (int i=0; i<brickRows; i++)
        for (int j=0; j<brickCols; j++)
            bricks[i][j] = 1;

    goldenBrickI = GetRandomValue(0, brickRows-1);
    goldenBrickJ = GetRandomValue(0, brickCols-1);
}

bool isGoldenBrick(int i, int j) {
    return (i == goldenBrickI && j == goldenBrickJ && bricks[i][j]==1);
}

int getBrickScore(int row) {
    return brickRows - row;
}

void drawBricks() {
    for (int i=0; i<brickRows; i++) {
        for (int j=0; j<brickCols; j++) {
            if (bricks[i][j]==1) {
                int px = j*brickW;
                int py = i*brickH;
                if (isGoldenBrick(i,j)) {
                    DrawRectangle(px,py,brickW-2,brickH-2,(Color){255,223,0,255});
                    DrawRectangleLinesEx((Rectangle){(float)px,(float)py,(float)(brickW-2),(float)(brickH-2)},3,YELLOW);
                } else {
                    DrawRectangle(px,py,brickW-2,brickH-2,brickColors[i]);
                }
            }
        }
    }
}