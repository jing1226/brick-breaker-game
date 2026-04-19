#include "raylib.h"
#include "Brick.h"
#include "Paddle.h"
#include "Ball.h"
#include "powerup.h"
#include <cstring>
#include <cctype>
#include <vector>

const int screenWidth = 800;
const int screenHeight = 600;

int score = 0;
int lives = 3;
int lastAddScore = 0;

char username[11] = { 0 };
int userNameLen = 0;

float gameTotalTime = 0.0f;

float ballSpeed = 1.0f;
std::vector<PowerUp> powerUps;

typedef enum {
    SCREEN_USERNAME,
    SCREEN_DIFFICULTY,
    SCREEN_START_GAME,
    SCREEN_COUNTDOWN,
    SCREEN_PLAY,
    SCREEN_GAME_OVER
} GameScreen;

GameScreen currentScreen = SCREEN_USERNAME;
int selectedDifficulty = 0;
float countdownTimer = 0.0f;

bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int GetBrickScore(int row) {
    return brickRows - row;
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Brick Game");
    InitAudioDevice();

    initPaddle(screenWidth, screenHeight);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (currentScreen == SCREEN_PLAY) {
            gameTotalTime += dt;
        }

        if (currentScreen == SCREEN_USERNAME) {
            int key = GetCharPressed();
            while (key > 0) {
                if (isAlpha(key) && userNameLen < 10) {
                    username[userNameLen++] = (char)key;
                    username[userNameLen] = 0;
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && userNameLen > 0)
                username[--userNameLen] = 0;

            if (IsKeyPressed(KEY_ENTER) && userNameLen > 0) {
                currentScreen = SCREEN_DIFFICULTY;
                selectedDifficulty = 0;
            }
        }

        if (currentScreen == SCREEN_DIFFICULTY) {
            if (IsKeyPressed(KEY_DOWN)) selectedDifficulty = (selectedDifficulty + 1) % 3;
            if (IsKeyPressed(KEY_UP)) selectedDifficulty = (selectedDifficulty - 1 + 3) % 3;

            if (IsKeyPressed(KEY_ENTER)) {
                gameTotalTime = 0;
                if (selectedDifficulty == 0) { lives = 3; brickRows = 5; }
                if (selectedDifficulty == 1) { lives = 2; brickRows = 5; }
                if (selectedDifficulty == 2) { lives = 1; brickRows = 6; }
                initBricks();
                powerUps.clear();
                currentScreen = SCREEN_START_GAME;
            }
        }

        if (currentScreen == SCREEN_START_GAME) {
            if (IsKeyPressed(KEY_ENTER)) {
                currentScreen = SCREEN_COUNTDOWN;
                countdownTimer = 3.0f;
                resetBall(screenWidth, screenHeight);
                score = 0;
                lastAddScore = 0;
                paddleWidth = 120.0f;
                ballSpeed = 1.0f;
            }
        }

        if (currentScreen == SCREEN_COUNTDOWN) {
            countdownTimer -= dt;
            if (countdownTimer <= 0) {
                currentScreen = SCREEN_PLAY;
                ballVelocity = (Vector2){ 4.0f, -6.0f };
            }
        }

        if (currentScreen == SCREEN_PLAY) {
            updatePaddle(screenWidth);
            ballVelocity.y += gravity;
            ballPosition.x += ballVelocity.x * ballSpeed;
            ballPosition.y += ballVelocity.y * ballSpeed;

            if (ballPosition.x - ballRadius <= 0 || ballPosition.x + ballRadius >= screenWidth)
                ballVelocity.x *= -1;

            if (ballPosition.y - ballRadius <= 0) {
                ballVelocity.y *= -bounceFactor;
                ballPosition.y = ballRadius;
            }

            if (ballPosition.y + ballRadius >= screenHeight) {
                lives--;
                if (lives <= 0) {
                    currentScreen = SCREEN_GAME_OVER;
                } else {
                    currentScreen = SCREEN_COUNTDOWN;
                    countdownTimer = 3.0f;
                    resetBall(screenWidth, screenHeight);
                    paddleWidth = 120.0f;
                    ballSpeed = 1.0f;
                }
            }

            Rectangle pad = { paddlePosition.x, paddlePosition.y, paddleWidth, paddleHeight };
            if (CheckCollisionCircleRec(ballPosition, ballRadius, pad) && ballVelocity.y > 0) {
                ballVelocity.y *= -bounceFactor;
                ballPosition.y = paddlePosition.y - ballRadius;
            }

            int totalAdd = 0;
            for (int i = 0; i < brickRows; i++) {
                for (int j = 0; j < brickCols; j++) {
                    if (bricks[i][j] == 1) {
                        Rectangle r = {
                            (float)j * brickW,
                            (float)i * brickH,
                            (float)brickW - 2,
                            (float)brickH - 2
                        };
                        if (CheckCollisionCircleRec(ballPosition, ballRadius, r)) {
                            bricks[i][j] = 0;
                            totalAdd += GetBrickScore(i);
                            ballVelocity.y *= -1;

                            if (GetRandomValue(0, 100) < 30) {
                                PowerUpType type = (PowerUpType)GetRandomValue(0, 2);
                                powerUps.push_back(CreatePowerUp((Vector2){ r.x + r.width / 2, r.y }, type));
                            }
                        }
                    }
                }
            }

            if (totalAdd > 0) {
                score += totalAdd;
                lastAddScore = totalAdd;
            }

            for (auto &p : powerUps) {
                UpdatePowerUp(&p, dt);
                if (CheckCollisionCircleRec(p.position, 15, pad)) {
                    ApplyPowerUp(p, &paddleWidth, &ballSpeed);
                    p.active = false;
                }
                if (p.position.y > screenHeight) {
                    p.active = false;
                }
            }

            for (auto it = powerUps.begin(); it != powerUps.end();) {
                if (!it->active) it = powerUps.erase(it);
                else ++it;
            }
        }

        if (currentScreen == SCREEN_GAME_OVER) {
            if (IsKeyPressed(KEY_ENTER)) currentScreen = SCREEN_DIFFICULTY;
            if (IsKeyPressed(KEY_SPACE)) currentScreen = SCREEN_USERNAME;
        }

        BeginDrawing();
        ClearBackground((Color){ 245, 245, 245, 255 });

        int seconds = (int)gameTotalTime;

        if (currentScreen == SCREEN_USERNAME) {
            DrawText("ENTER USERNAME", 220, 120, 50, DARKBLUE);
            DrawRectangleRounded((Rectangle){ 240, 260, 320, 60 }, 0.2f, 10, LIGHTGRAY);
            DrawText(username, 250, 270, 30, BLACK);
            DrawText("Press ENTER to continue", 230, 350, 30, DARKPURPLE);
        }

        if (currentScreen == SCREEN_DIFFICULTY) {
            DrawText("SELECT DIFFICULTY", 200, 100, 55, DARKBLUE);
            const char* opts[] = {
                "EASY    3 lives",
                "MEDIUM  2 lives",
                "HARD    1 life"
            };
            int ys[] = { 220, 300, 380 };
            for (int i = 0; i < 3; i++) {
                bool sel = (selectedDifficulty == i);
                Color c = sel ? RED : DARKGRAY;
                DrawRectangleRounded((Rectangle){ 200.0f, (float)ys[i]-10, 400, 70 }, 0.2f, 10, WHITE);
                DrawRectangleRoundedLines((Rectangle){ 200.0f, (float)ys[i]-10, 400, 70 }, 0.2f, 10, 3, c);
                DrawText(opts[i], 230, ys[i], 35, c);
            }
        }

        if (currentScreen == SCREEN_START_GAME) {
            DrawText(TextFormat("PLAYER: %s", username), 250, 150, 40, DARKBLUE);
            DrawText("READY TO PLAY", 230, 280, 60, RED);
            DrawText("PRESS ENTER TO START", 200, 400, 40, DARKPURPLE);
        }

        if (currentScreen == SCREEN_COUNTDOWN) {
            drawBricks();
            drawPaddle();
            DrawCircleV(ballPosition, ballRadius, RED);
            DrawText(TextFormat("%d", (int)countdownTimer + 1), screenWidth/2-25, screenHeight/2-40, 100, RED);
        }

        if (currentScreen == SCREEN_PLAY || currentScreen == SCREEN_GAME_OVER) {
            drawBricks();
            drawPaddle();
            DrawCircleV(ballPosition, ballRadius, RED);

            for (auto &p : powerUps) DrawPowerUp(p);

            DrawText(TextFormat("SCORE: %d", score), 20, 20, 32, BLACK);
            DrawText("LIVES:", 630, 20, 32, BLACK);
            for (int i = 0; i < lives; i++) DrawCircle(710 + i*28, 35, 10, RED);
            if (lastAddScore > 0 && currentScreen == SCREEN_PLAY)
                DrawText(TextFormat("+%d", lastAddScore), screenWidth/2 - 20, 100, 40, GREEN);

            // 时间：砖块下方靠左，带 s 单位
            DrawText(TextFormat("TIME: %ds", seconds), 20, 480, 32, BLACK);
        }

        if (currentScreen == SCREEN_GAME_OVER) {
            DrawRectangleRounded((Rectangle){ 230, 190, 340, 220 }, 0.2f, 10, WHITE);
            DrawText("GAME OVER", 260, 210, 65, RED);
            DrawText(TextFormat("SCORE: %d", score), 300, 300, 40, DARKPURPLE);
            DrawText(TextFormat("TIME: %ds", seconds), 300, 350, 40, DARKPURPLE);
            DrawText("ENTER = PLAY AGAIN", 270, 410, 25, DARKGRAY);
            DrawText("SPACE = NEW PLAYER", 260, 440, 25, DARKGRAY);
        }

        EndDrawing();
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}