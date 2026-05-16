#include "Brick.h"
#include "raylib.h"

// 只声明，不定义！！！
extern bool asyncLoadComplete;

int brickRows = 5;
const int brickCols = 14;
int brickW = 57;
int brickH = 28;
int bricks[10][14];
int goldenBrickI;
int goldenBrickJ;

Color brickColors[10] = {
    {180, 200, 255, 255}, {160, 240, 180, 255},
    {255, 210, 160, 255}, {255, 180, 200, 255},
    {160, 200, 240, 255}, {200, 180, 220, 255}
};

void initBricks() {
    for (int i = 0; i < brickRows; i++) {
        for (int j = 0; j < brickCols; j++) {
            bricks[i][j] = 1;
        }
    }
    goldenBrickI = GetRandomValue(0, brickRows - 1);
    goldenBrickJ = GetRandomValue(0, brickCols - 1);
    asyncLoadComplete = false; // 这里只赋值，不定义
}

bool isGoldenBrick(int i, int j) {
    return i == goldenBrickI && j == goldenBrickJ && bricks[i][j] == 1;
}

int getBrickScore(int row) {
    if (brickRows == 10) {
        if (row < 0) return 2;
        if (row > 9) row = 9;
        const int scoreTable10[10] = {20, 20, 10, 10, 8, 8, 4, 4, 2, 2};
        return scoreTable10[row];
    } else {
        if (row < 0) return 2;
        if (row > 4) row = 4;
        const int scoreTable5[5] = {20, 10, 8, 4, 2};
        return scoreTable5[row];
    }
}

void drawBricks() {
    // 去掉了 if (asyncLoadComplete) 判断，砖块会一直画出来
    for (int i = 0; i < brickRows; i++) {
        for (int j = 0; j < brickCols; j++) {
            if (bricks[i][j]) {
                Rectangle rect = {
                    (float)(j * brickW + 10),
                    (float)(i * brickH + 50),
                    (float)brickW - 2,
                    (float)brickH - 2
                };
                if (isGoldenBrick(i, j)) {
                    DrawRectangleRec(rect, GOLD);
                } else {
                    DrawRectangleRec(rect, brickColors[i % 6]);
                }
            }
        }
    }
}