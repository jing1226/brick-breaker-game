#include "raylib.h"
#include "Brick.h"
#include "Paddle.h"
#include "Ball.h"
#include "powerup.h"
#include <cstring>
#include <cctype>
#include <vector>
#include <fstream>
#include <algorithm>

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

// ====================== 历史分数榜单结构 ======================
typedef struct {
    char name[11];
    int score;
    char mode[10];  // 难度：EASY/MEDIUM/HARD
} ScoreRecord;

std::vector<ScoreRecord> highScores;

// ====================== 游戏状态 ======================
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

const char* difficultyNames[] = {"EASY", "MEDIUM", "HARD"};

// ====================== 工具函数 ======================
bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int GetBrickScore(int row) {
    return brickRows - row;
}

// ====================== 分数保存/加载 ======================
bool CompareScores(const ScoreRecord &a, const ScoreRecord &b) {
    return a.score > b.score;
}

void LoadHighScores() {
    highScores.clear();
    FILE *fp = fopen("scores.txt", "r");
    if (!fp) return;

    ScoreRecord r;
    while (fscanf(fp, "%s %d %s", r.name, &r.score, r.mode) != EOF) {
        highScores.push_back(r);
    }
    fclose(fp);
    std::sort(highScores.begin(), highScores.end(), CompareScores);
}

void SaveHighScores() {
    std::sort(highScores.begin(), highScores.end(), CompareScores);
    if (highScores.size() > 10) highScores.resize(10);

    FILE *fp = fopen("scores.txt", "w");
    if (!fp) return;
    for (auto &r : highScores) {
        fprintf(fp, "%s %d %s\n", r.name, r.score, r.mode);
    }
    fclose(fp);
}

void AddCurrentScore() {
    ScoreRecord r;
    strcpy(r.name, username);
    r.score = score;
    strcpy(r.mode, difficultyNames[selectedDifficulty]);
    highScores.push_back(r);
    SaveHighScores();
}

// ====================== 主函数 ======================
int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Brick Game");
    InitAudioDevice();
    LoadHighScores();  // 启动时加载榜单

    initPaddle(screenWidth, screenHeight);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (currentScreen == SCREEN_PLAY) {
            gameTotalTime += dt;
        }

        // ========== 用户名输入 ==========
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

        // ========== 难度选择 ==========
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

        // ========== 开始确认 ==========
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

        // ========== 倒计时 ==========
        if (currentScreen == SCREEN_COUNTDOWN) {
            countdownTimer -= dt;
            if (countdownTimer <= 0) {
                currentScreen = SCREEN_PLAY;
                ballVelocity = (Vector2){ 4.0f, -6.0f };
            }
        }

        // ========== 游戏中 ==========
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
                                powerUps.push_back(CreatePowerUp({ r.x + r.width / 2, r.y }, type));
                            }
                        }
                    }
                }
            }

            if (totalAdd > 0) {
                score += totalAdd;
                lastAddScore = totalAdd;
            }

            for (auto &p : powerUps) UpdatePowerUp(&p, dt);
            for (auto &p : powerUps) {
                if (CheckCollisionCircleRec(p.position, 15, {paddlePosition.x,paddlePosition.y,paddleWidth,paddleHeight})) {
                    ApplyPowerUp(p, &paddleWidth, &ballSpeed);
                    p.active = false;
                }
            }

            for (auto it = powerUps.begin(); it != powerUps.end();) {
                if (!it->active) it = powerUps.erase(it);
                else ++it;
            }
        }

        // ========== 游戏结束：按 S 保存分数 ==========
        if (currentScreen == SCREEN_GAME_OVER) {
            if (IsKeyPressed(KEY_ENTER)) currentScreen = SCREEN_DIFFICULTY;
            if (IsKeyPressed(KEY_SPACE)) currentScreen = SCREEN_USERNAME;
            if (IsKeyPressed(KEY_S)) AddCurrentScore(); // ✅ 按 S 保存
        }

        // ====================== 绘制 ======================
        BeginDrawing();
        ClearBackground((Color){245,245,245,255});
        int seconds = (int)gameTotalTime;

        // ========== 用户名 ==========
        if (currentScreen == SCREEN_USERNAME) {
            DrawText("ENTER USERNAME", 220, 120, 50, DARKBLUE);
            DrawRectangleRounded({240,260,320,60}, 0.2f,10,LIGHTGRAY);
            DrawText(username,250,270,30,BLACK);
            DrawText("Press ENTER to continue",230,350,30,DARKPURPLE);
        }

        // ========== 难度 ==========
        if (currentScreen == SCREEN_DIFFICULTY) {
            DrawText("SELECT DIFFICULTY",200,100,55,DARKBLUE);
            const char* opts[] = {"EASY 3 lives","MEDIUM 2 lives","HARD 1 life"};
            int ys[]={220,300,380};
            for(int i=0;i<3;i++){
                bool sel = selectedDifficulty==i;
                Color c=sel?RED:DARKGRAY;
                DrawRectangleRounded({200,ys[i]-10,400,70},0.2,10,WHITE);
                DrawRectangleRoundedLines({200,ys[i]-10,400,70},0.2,10,3,c);
                DrawText(opts[i],230,ys[i],35,c);
            }
        }

        // ========== 开始 ==========
        if (currentScreen == SCREEN_START_GAME) {
            DrawText(TextFormat("PLAYER: %s",username),250,150,40,DARKBLUE);
            DrawText("READY TO PLAY",230,280,60,RED);
            DrawText("PRESS ENTER TO START",200,400,40,DARKPURPLE);
        }

        // ========== 倒计时 ==========
        if (currentScreen == SCREEN_COUNTDOWN) {
            drawBricks(); drawPaddle();
            DrawCircleV(ballPosition, ballRadius, RED);
            DrawText(TextFormat("%d",(int)countdownTimer+1),screenWidth/2-25,screenHeight/2-40,100,RED);
        }

        // ========== 游戏中 ==========
        if (currentScreen == SCREEN_PLAY || currentScreen == SCREEN_GAME_OVER) {
            drawBricks(); drawPaddle();
            DrawCircleV(ballPosition, ballRadius, RED);
            for(auto &p:powerUps) DrawPowerUp(p);

            DrawText(TextFormat("SCORE: %d",score),20,20,32,BLACK);
            DrawText("LIVES:",630,20,32,BLACK);
            for(int i=0;i<lives;i++) DrawCircle(710+i*28,35,10,RED);
            if(lastAddScore>0 && currentScreen==SCREEN_PLAY)
                DrawText(TextFormat("+%d",lastAddScore),screenWidth/2-20,100,40,GREEN);

            // ✅ 时间：砖块下方左侧 + 带 s
            DrawText(TextFormat("TIME: %ds",seconds),20,480,32,BLACK);
        }

        // ========== 游戏结束界面 + 历史榜单 ==========
        if (currentScreen == SCREEN_GAME_OVER) {
            DrawRectangleRounded({230,170,340,280},0.2,10,WHITE);
            DrawText("GAME OVER",260,180,60,RED);
            DrawText(TextFormat("SCORE: %d",score),300,250,30,DARKPURPLE);
            DrawText(TextFormat("TIME: %ds",seconds),300,280,30,DARKPURPLE);
            DrawText("PRESS S = SAVE SCORE",250,320,25,DARKGREEN);
            DrawText("ENTER = PLAY AGAIN",250,350,25,DARKGRAY);
            DrawText("SPACE = NEW PLAYER",240,380,25,DARKGRAY);

            // ✅ 显示历史前10榜单
            DrawText("HIGH SCORES",500,170,30,DARKBLUE);
            for(int i=0;i<(int)highScores.size() && i<10;i++){
                auto &r=highScores[i];
                DrawText(TextFormat("%d. %s",i+1,r.name),510,210+i*25,20,BLACK);
                DrawText(TextFormat("%d",r.score),650,210+i*25,20,RED);
                DrawText(r.mode,710,210+i*25,20,DARKPURPLE);
            }
        }

        EndDrawing();
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}