#include <graphics.h>
#include <conio.h>
#include <time.h>
#include <stdio.h>

// --------------------- 全局变量 ---------------------
int ballX = 400, ballY = 300;
int ballSpeedX = 4, ballSpeedY = -4;
int paddleX = 350, paddleY = 550;
int paddleW = 100, paddleH = 15;
int score = 0;
int gameOver = 0;    // 游戏是否结束
int isPaused = 0;    // 是否暂停
int restartTimer = 0;// 失败后倒计时
int failTime = 0;    // 记录失败时间

// 砖块
int brickW = 60;
int brickH = 20;
int bricks[5][8];

// --------------------- 初始化砖块 ---------------------
void initBricks() {
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 8; j++) {
            bricks[i][j] = 1;
        }
    }
}

// --------------------- 主函数 ---------------------
int main() {
    initgraph(800, 600);
    initBricks();
    srand((unsigned)time(NULL));

    // 小球初始位置
    ballX = 400;
    ballY = 500;
    ballSpeedX = 4;
    ballSpeedY = -4;

    while (1) {
        // 清屏
        cleardevice();

        // ===================== 按键检测 =====================
        if (_kbhit()) {
            char ch = _getch();

            // 【功能1】空格键：暂停 / 继续
            if (ch == ' ') {
                if (!gameOver) { // 只有游戏没结束才能暂停
                    isPaused = !isPaused;
                }
            }

            // 【功能2】失败后 5秒内按 Enter 重新开始
            if (ch == 13 && gameOver) { // 13 = Enter 键
                if (clock() - failTime <= 5000) { // 5秒内
                    // 重置游戏
                    gameOver = 0;
                    isPaused = 0;
                    score = 0;
                    initBricks();
                    ballX = 400;
                    ballY = 500;
                    ballSpeedX = 4;
                    ballSpeedY = -4;
                }
            }

            // 挡板左右移动
            if (ch == 75 && paddleX > 0) {        // 左
                paddleX -= 20;
            }
            if (ch == 77 && paddleX < 700) {    // 右
                paddleX += 20;
            }
        }

        // ===================== 暂停状态 =====================
        if (isPaused) {
            settextcolor(RED);
            settextstyle(40, 0, _T("黑体"));
            outtextxy(300, 250, _T("已暂停"));
            outtextxy(220, 300, _T("按空格键继续游戏"));
            Sleep(50);
            continue; // 跳过后面的逻辑
        }

        // ===================== 游戏结束逻辑 =====================
        if (ballY > 580 && !gameOver) {
            gameOver = 1;
            failTime = clock(); // 记录失败时间
        }

        // ===================== 游戏运行中 =====================
        if (!gameOver && !isPaused) {
            // 小球移动
            ballX += ballSpeedX;
            ballY += ballSpeedY;

            // 墙壁反弹
            if (ballX <= 10 || ballX >= 790) ballSpeedX = -ballSpeedX;
            if (ballY <= 10) ballSpeedY = -ballSpeedY;

            // 挡板碰撞
            if (ballY + 10 >= paddleY &&
                ballX >= paddleX &&
                ballX <= paddleX + paddleW) {
                ballSpeedY = -ballSpeedY;
            }

            // 砖块碰撞
            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 8; j++) {
                    if (bricks[i][j] == 1) {
                        int x = j * (brickW + 5) + 50;
                        int y = i * (brickH + 5) + 50;
                        if (ballX + 10 >= x && ballX - 10 <= x + brickW &&
                            ballY + 10 >= y && ballY - 10 <= y + brickH) {
                            bricks[i][j] = 0;
                            ballSpeedY = -ballSpeedY;
                            score += 10;
                        }
                    }
                }
            }
        }

        // ===================== 绘制 =====================
        // 小球
        setfillcolor(YELLOW);
        solidcircle(ballX, ballY, 10);

        // 挡板
        setfillcolor(LIGHTBLUE);
        solidrectangle(paddleX, paddleY, paddleX + paddleW, paddleY + paddleH);

        // 砖块
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 8; j++) {
                if (bricks[i][j] == 1) {
                    int x = j * (brickW + 5) + 50;
                    int y = i * (brickH + 5) + 50;
                    setfillcolor(LIGHTGREEN);
                    solidrectangle(x, y, x + brickW, y + brickH);
                }
            }
        }

        // 分数
        char str[50];
        sprintf(str, "分数：%d", score);
        settextcolor(WHITE);
        settextstyle(25, 0, _T("黑体"));
        outtextxy(20, 20, str);

        // ===================== 游戏失败界面 =====================
        if (gameOver) {
            settextcolor(RED);
            settextstyle(40, 0, _T("黑体"));
            outtextxy(280, 250, _T("游戏失败"));

            // 倒计时
            int left = 5 - (clock() - failTime) / 1000;
            if (left < 0) left = 0;

            char tip[100];
            sprintf(tip, "%d秒内按Enter重新开始", left);
            outtextxy(200, 300, tip);
        }

        Sleep(15);
    }

    closegraph();
    return 0;
}