#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Brick Breaker");

    Ball ball({400, 300}, {3, 3}, 10);
    Paddle paddle(350, 550, 100, 20);
    int score = 0;

    std::vector<Brick> bricks;
    float brickWidth = 100;
    float brickHeight = 30;
    Color brickColors[4] = {RED, GREEN, BLUE, YELLOW};
    
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 8; col++) {
            bricks.emplace_back(50 + col*120, 80 + row*40, brickWidth, brickHeight, brickColors[row]);
        }
    }

    SetTargetFPS(60);

    // 倒计时 3 2 1
    float countdownTime = 3.0f;
    while (countdownTime > 0 && !WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        int num = (int)countdownTime + 1;
        DrawText(TextFormat("%d", num), screenWidth/2 - MeasureText(TextFormat("%d", num), 100)/2, screenHeight/2 - 50, 100, BLUE);
        
        EndDrawing();
        countdownTime -= GetFrameTime();
    }

    // 游戏主循环
    while (!WindowShouldClose()) {
        ball.Move();
        ball.BounceEdge(screenWidth, screenHeight);

        // 落地 Game Over
        if (ball.position.y + ball.radius >= screenHeight) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("GAME OVER!", screenWidth/2 - MeasureText("GAME OVER!", 60)/2, screenHeight/2 - 30, 60, RED);
            EndDrawing();
            WaitTime(2);
            CloseWindow();
            return 0;
        }

        // 挡板控制
        if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(5);
        if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(5);

        // 碰挡板反弹
        if (CheckCollisionCircleRec(ball.position, ball.radius, Rectangle{paddle.x, paddle.y, paddle.width, paddle.height})) {
            ball.speed.y *= -1;
        }

        // 碰砖块加分
        for (int i = 0; i < bricks.size(); i++) {
            if (CheckCollisionCircleRec(ball.position, ball.radius, Rectangle{bricks[i].x, bricks[i].y, bricks[i].width, bricks[i].height})) {
                ball.speed.y *= -1;
                bricks.erase(bricks.begin() + i);
                score += 1;
                break;
            }
        }

        // 绘制
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        // ✅ 这里就是 score 空格 分数，无乱码
        DrawText(TextFormat("score %d", score), 20, 20, 20, BLACK);
        
        ball.Draw();
        paddle.Draw();
        for (auto& brick : bricks) brick.Draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}