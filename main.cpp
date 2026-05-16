#include "raylib.h"
#include "Brick.h"
#include "Paddle.h"
#include "Ball.h"
#include "tool.h"
#include "resource_manager.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "enet/enet.h"
#ifdef __cplusplus
}
#endif
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <thread>
#include <mutex>
#include <chrono>
#include <cmath>
#include <vector>
#include <set>
#include <fstream>
#include <algorithm>
using namespace std;

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const int NETWORK_PORT = 1234;
const int MAX_CLIENTS = 1;
const int CHANNEL_COUNT = 1;
const char* SYNC_FILE = "/tmp/breakout_sync.dat";

int currentScore = 0;
int lives = 3;
bool gameStarted = false;
bool gameOver = false;
bool isHost = false;
bool isClient = false;
bool connectionReady = false;

std::mutex loadMutex;
bool isLoading = false;
bool loadFinished = false;
bool asyncLoadComplete = false;
float paddleRightX = SCREEN_WIDTH - 150;
const float PADDLE_SPEED = 6.0f;

enum MenuState {
    MENU_TITLE,
    MENU_USERNAME,
    MENU_DIFFICULTY,
    MENU_COUNTDOWN,
    MENU_PLAYING,
    MENU_GAMEOVER,
    MENU_LEADERBOARD
};
MenuState menuState = MENU_TITLE;

char username[16] = "";
int usernameLen = 0;
bool usernameValid = false;
int selectedDifficulty = 0;
bool scoreSaved = false;
bool ballTouchedPaddle = false;
float currentBallSpeed = 5.0f;
float slowBallDropSpeed = 1.5f;
float countdownTimer = 0.0f;
int countdownValue = 3;
bool difficultySpeedIncrease = false;

struct ScoreEntry {
    char name[16];
    int score;
};
vector<ScoreEntry> leaderboard;
const char* LEADERBOARD_FILE = "leaderboard.txt";

struct Particle {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float life;
    bool active;
};
static Particle particles[200];
static int particleCount = 0;

#define MAX_TRAIL 60
typedef struct {
    Vector2 pos;
    Color color;
    float life;
} TrailParticle;
TrailParticle ballTrail[MAX_TRAIL];
int trailIndex = 0;

#define MAX_BRICK_PARTICLES 512
typedef struct {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float life;
} BrickParticle;
BrickParticle brickParticles[MAX_BRICK_PARTICLES];
int brickParticleCount = 0;

int scoreTable5[5] = {20, 10, 8, 4, 2};
int scoreTable10[10] = {20, 20, 10, 10, 8, 8, 4, 4, 2, 2};

#pragma pack(push, 1)
enum PacketType : uint8_t {
    PACKET_INPUT = 1,
    PACKET_STATE = 2,
};
struct InputPacket {
    uint8_t type;
    uint8_t left;
    uint8_t right;
};
struct GameStatePacket {
    uint8_t type;
    float padLeftX;
    float padRightX;
    float ballX;
    float ballY;
    float velX;
    float velY;
    int32_t score;
    int32_t lives;
    uint8_t running;
    uint8_t gameOver;
    int32_t bricks[10][14];
};
#pragma pack(pop)

ENetHost* netHost = nullptr;
ENetPeer* netPeer = nullptr;
uint8_t clientInputLeft = 0;
uint8_t clientInputRight = 0;

ResourceManager* gResourceManager = nullptr;

SpatialGrid* spatialGrid = nullptr;
const int SPATIAL_GRID_WIDTH = 8;
const int SPATIAL_GRID_HEIGHT = 6;
bool debugDrawGrid = false;
std::mutex spatialGridPtrMutex;

float lastCollisionCheckTime = 0.0f;
float avgCollisionCheckTime = 0.0f;

void AddBallTrail(Vector2 pos) {
    float hue = fmodf((float)GetTime() * 500.0f, 360.0f);
    Color c = ColorFromHSV(hue, 0.9f, 1.0f);
    ballTrail[trailIndex] = { pos, c, 1.0f };
    trailIndex = (trailIndex + 1) % MAX_TRAIL;
}

void UpdateBallTrail(float dt) {
    for (int i = 0; i < MAX_TRAIL; i++) {
        if (ballTrail[i].life > 0) {
            ballTrail[i].life -= dt * 4.0f;
        }
    }
}

void DrawBallTrail() {
    for (int i = 0; i < MAX_TRAIL; i++) {
        if (ballTrail[i].life > 0) {
            float radius = ballRadius * 0.85f * ballTrail[i].life;
            DrawCircleV(ballTrail[i].pos, radius, Fade(ballTrail[i].color, ballTrail[i].life * 0.6f));
        }
    }
}

void AddBrickExplosion(Vector2 pos, Color color) {
    for (int k = 0; k < 12; k++) {
        if (brickParticleCount >= MAX_BRICK_PARTICLES) break;
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float speed = GetRandomValue(80, 200) / 60.0f;
        brickParticles[brickParticleCount++] = {
            pos,
            { cosf(angle)*speed, sinf(angle)*speed },
            color,
            0.6f
        };
    }
}

void UpdateBrickParticles(float dt) {
    for (int i = 0; i < brickParticleCount; i++) {
        if (brickParticles[i].life > 0) {
            brickParticles[i].life -= dt;
            brickParticles[i].pos.x += brickParticles[i].vel.x * dt * 60.0f;
            brickParticles[i].pos.y += brickParticles[i].vel.y * dt * 60.0f;
            brickParticles[i].vel.y += 0.2f * dt * 60.0f;
        }
    }
}

void DrawBrickParticles() {
    for (int i = 0; i < brickParticleCount; i++) {
        if (brickParticles[i].life > 0) {
            DrawPixelV(brickParticles[i].pos, Fade(brickParticles[i].color, brickParticles[i].life));
        }
    }
}

void DrawCenteredText(const char* text, int y, int fontSize, Color color) {
    int w = MeasureText(text, fontSize);
    DrawText(text, (SCREEN_WIDTH - w) / 2, y, fontSize, color);
}

bool isValidUsernameChar(int key) {
    if (key >= '0' && key <= '9') return true;
    if (key >= 'A' && key <= 'Z') return true;
    if (key >= 'a' && key <= 'z') return true;
    return false;
}

void loadLeaderboard() {
    leaderboard.clear();
    ifstream f(LEADERBOARD_FILE);
    if (!f.is_open()) return;
    while (!f.eof() && leaderboard.size() < 100) {
        ScoreEntry entry;
        if (f >> entry.name >> entry.score) {
            leaderboard.push_back(entry);
        } else break;
    }
    f.close();
    sort(leaderboard.begin(), leaderboard.end(), [](const ScoreEntry& a, const ScoreEntry& b) {
        return a.score > b.score;
    });
    if (leaderboard.size() > 10) leaderboard.resize(10);
}

void writeLeaderboard() {
    ofstream f(LEADERBOARD_FILE, ios::trunc);
    if (!f.is_open()) return;
    for (auto& e : leaderboard) {
        f << e.name << " " << e.score << endl;
    }
    f.close();
}

void addScoreToLeaderboard(const char* name, int score) {
    ScoreEntry e;
    strncpy(e.name, name, 15);
    e.name[15] = 0;
    e.score = score;
    leaderboard.push_back(e);
    sort(leaderboard.begin(), leaderboard.end(), [](const ScoreEntry& a, const ScoreEntry& b) {
        return a.score > b.score;
    });
    if (leaderboard.size() > 10) leaderboard.resize(10);
    writeLeaderboard();
}

int GetBrickScoreByRow(int row) {
    if (brickRows == 10) {
        if (row < 0) return 2;
        if (row > 9) row = 9;
        return scoreTable10[row];
    }
    if (row < 0) return 2;
    if (row > 4) row = 4;
    return scoreTable5[row];
}

void spawnBrickParticles(int row, int col, Color color) {
    int cnt = 12;
    for (int i = 0; i < cnt; i++) {
        if (particleCount >= 200) break;
        auto& p = particles[particleCount++];
        p.active = true;
        p.position = { (float)(col*brickW + brickW/2), (float)(row*brickH + brickH/2) };
        float ang = GetRandomValue(0, 360) * DEG2RAD;
        float sp = GetRandomValue(50, 120) / 60.0f;
        p.velocity = { cosf(ang)*sp, sinf(ang)*sp - 1.0f };
        p.color = color;
        p.life = 0.4f + GetRandomValue(0, 20) / 100.0f;
    }
}

void updateParticles(float dt) {
    for (int i = 0; i < particleCount; i++) {
        auto& p = particles[i];
        if (!p.active) continue;
        p.position.x += p.velocity.x * dt * 60;
        p.position.y += p.velocity.y * dt * 60;
        p.velocity.y += gravity * 30 * dt;
        p.life -= dt;
        if (p.life <= 0) p.active = false;
    }
}

void drawParticles() {
    for (int i = 0; i < particleCount; i++) {
        auto& p = particles[i];
        if (p.active) DrawPixelV(p.position, p.color);
    }
}

void startCountdown() {
    menuState = MENU_COUNTDOWN;
    gameOver = false;
    gameStarted = false;
    ballTouchedPaddle = false;
    currentBallSpeed = slowBallDropSpeed;
    countdownTimer = 0;
    countdownValue = 3;
}

void setDifficultyOptions() {
    if (selectedDifficulty == 0) {
        lives = 2;
        difficultySpeedIncrease = false;
        brickRows = 5;
    } else if (selectedDifficulty == 1) {
        lives = 2;
        difficultySpeedIncrease = true;
        brickRows = 5;
    } else if (selectedDifficulty == 2) {
        lives = 1;
        difficultySpeedIncrease = false;
        brickRows = 5;
    } else {
        lives = 3;
        difficultySpeedIncrease = false;
        brickRows = 10;
    }
}

void ResetGame() {
    currentScore = 0;
    gameOver = false;
    gameStarted = false;
    scoreSaved = false;
    setDifficultyOptions();
    initBricks();
    {
        std::lock_guard<std::mutex> g(spatialGridPtrMutex);
        if (spatialGrid) { DestroySpatialGrid(spatialGrid); spatialGrid = nullptr; }
        spatialGrid = CreateSpatialGrid(SCREEN_WIDTH, SCREEN_HEIGHT, brickRows, brickCols, brickW, brickH, SPATIAL_GRID_WIDTH, SPATIAL_GRID_HEIGHT);
        InitializeGridFromBricks(spatialGrid, bricks);
    }
    initPaddle(SCREEN_WIDTH, SCREEN_HEIGHT);
    resetBall(SCREEN_WIDTH, SCREEN_HEIGHT, slowBallDropSpeed);
    paddleRightX = SCREEN_WIDTH - 150;
    brickParticleCount = 0;
    trailIndex = 0;
    for (int i = 0; i < MAX_TRAIL; i++) ballTrail[i].life = 0;
}

void writeState() {
    FILE* f = fopen(SYNC_FILE, "wb");
    if (!f) return;
    GameStatePacket p;
    p.type = PACKET_STATE;
    p.padLeftX = paddlePosition.x;
    p.padRightX = paddleRightX;
    p.ballX = ballPosition.x;
    p.ballY = ballPosition.y;
    p.velX = ballVelocity.x;
    p.velY = ballVelocity.y;
    p.score = currentScore;
    p.lives = lives;
    p.running = gameStarted ? 1 : 0;
    p.gameOver = gameOver ? 1 : 0;
    for (int i = 0; i < 10; i++) for (int j = 0; j < 14; j++) p.bricks[i][j] = bricks[i][j];
    fwrite(&p, sizeof(p), 1, f);
    fclose(f);
}

void writeInput() {
    FILE* f = fopen("/tmp/breakout_input.dat", "wb");
    if (!f) return;
    InputPacket p;
    p.type = PACKET_INPUT;
    p.left = IsKeyDown(KEY_LEFT) ? 1 : 0;
    p.right = IsKeyDown(KEY_RIGHT) ? 1 : 0;
    fwrite(&p, sizeof(p), 1, f);
    fclose(f);
}

void sendStateToClient() {
    if (!isHost || !connectionReady || !netPeer) { writeState(); return; }
    GameStatePacket p;
    p.type = PACKET_STATE;
    p.padLeftX = paddlePosition.x;
    p.padRightX = paddleRightX;
    p.ballX = ballPosition.x;
    p.ballY = ballPosition.y;
    p.velX = ballVelocity.x;
    p.velY = ballVelocity.y;
    p.score = currentScore;
    p.lives = lives;
    p.running = gameStarted ? 1 : 0;
    p.gameOver = gameOver ? 1 : 0;
    for (int i = 0; i < 10; i++) for (int j = 0; j < 14; j++) p.bricks[i][j] = bricks[i][j];
    ENetPacket* ep = enet_packet_create(&p, sizeof(p), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(netPeer, 0, ep);
    enet_host_flush(netHost);
}

void sendInputToHost() {
    if (!isClient || !connectionReady || !netPeer) { writeInput(); return; }
    InputPacket p;
    p.type = PACKET_INPUT;
    p.left = IsKeyDown(KEY_LEFT) ? 1 : 0;
    p.right = IsKeyDown(KEY_RIGHT) ? 1 : 0;
    ENetPacket* ep = enet_packet_create(&p, sizeof(p), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(netPeer, 0, ep);
    enet_host_flush(netHost);
}

void readState() {
    FILE* f = fopen(SYNC_FILE, "rb");
    if (!f) return;
    GameStatePacket p;
    if (fread(&p, sizeof(p), 1, f) == 1) {
        paddlePosition.x = p.padLeftX;
        paddleRightX = p.padRightX;
        ballPosition.x = p.ballX;
        ballPosition.y = p.ballY;
        ballVelocity.x = p.velX;
        ballVelocity.y = p.velY;
        currentScore = p.score;
        lives = p.lives;
        gameStarted = p.running != 0;
        gameOver = p.gameOver != 0;
        for (int i = 0; i < 10; i++) for (int j = 0; j < 14; j++) bricks[i][j] = p.bricks[i][j];
        {
            std::lock_guard<std::mutex> g(spatialGridPtrMutex);
            if (spatialGrid) InitializeGridFromBricks(spatialGrid, bricks);
        }
    }
    fclose(f);
}

void readInput() {
    FILE* f = fopen("/tmp/breakout_input.dat", "rb");
    if (!f) return;
    InputPacket p;
    if (fread(&p, sizeof(p), 1, f) == 1) {
        clientInputLeft = p.left;
        clientInputRight = p.right;
    }
    fclose(f);
}

void startAsyncLoad() {
    std::lock_guard<std::mutex> g(loadMutex);
    if (isLoading || loadFinished) return;
    isLoading = true;
    loadFinished = false;
    asyncLoadComplete = false;
    const string path = "resources/brick.png";
    if (gResourceManager) PreloadTextureAsync(gResourceManager, path);
    thread([path]() {
        while (gResourceManager && !IsTextureLoaded(gResourceManager, path))
            this_thread::sleep_for(chrono::milliseconds(50));
        std::lock_guard<std::mutex> g(loadMutex);
        loadFinished = true;
        isLoading = false;
        asyncLoadComplete = true;
    }).detach();
}

void processNetworkEvents() {
    if (!netHost) return;
    ENetEvent e;
    while (enet_host_service(netHost, &e, 0) > 0) {
        if (e.type == ENET_EVENT_TYPE_CONNECT) {
            netPeer = e.peer;
            connectionReady = true;
        } else if (e.type == ENET_EVENT_TYPE_RECEIVE) {
            auto d = (uint8_t*)e.packet->data;
            if (d[0] == PACKET_INPUT && isHost && e.packet->dataLength == sizeof(InputPacket)) {
                auto p = (InputPacket*)d;
                clientInputLeft = p->left;
                clientInputRight = p->right;
            }
            if (d[0] == PACKET_STATE && isClient && e.packet->dataLength == sizeof(GameStatePacket)) {
                auto p = (GameStatePacket*)d;
                paddlePosition.x = p->padLeftX;
                paddleRightX = p->padRightX;
                ballPosition.x = p->ballX;
                ballPosition.y = p->ballY;
                ballVelocity.x = p->velX;
                ballVelocity.y = p->velY;
                currentScore = p->score;
                lives = p->lives;
                gameStarted = p->running != 0;
                gameOver = p->gameOver != 0;
                if (gameStarted && menuState != MENU_PLAYING) menuState = MENU_PLAYING;
                if (gameOver) menuState = MENU_GAMEOVER;
                for (int i = 0; i < 10; i++) for (int j = 0; j < 14; j++) bricks[i][j] = p->bricks[i][j];
                {
                    std::lock_guard<std::mutex> g(spatialGridPtrMutex);
                    if (spatialGrid) InitializeGridFromBricks(spatialGrid, bricks);
                }
            }
            enet_packet_destroy(e.packet);
        } else if (e.type == ENET_EVENT_TYPE_DISCONNECT) {
            connectionReady = false;
            netPeer = nullptr;
        }
    }
}

int main(int argc, char** argv) {
    if (argc >= 2) {
        if (!strcmp(argv[1], "host")) isHost = true;
        else if (!strcmp(argv[1], "client")) isClient = true;
    }
    if (!isHost && !isClient) isHost = true;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, isHost ? "HOST (1P)" : "CLIENT (2P)");
    SetTargetFPS(144);

    gResourceManager = InitResourceManager();
    loadLeaderboard();
    ResetGame();

    if (enet_initialize() != 0) { CloseWindow(); return 0; }
    atexit(enet_deinitialize);

    ENetAddress addr;
    if (isHost) {
        enet_address_set_host(&addr, "0.0.0.0");
        addr.port = NETWORK_PORT;
        netHost = enet_host_create(&addr, MAX_CLIENTS, CHANNEL_COUNT, 0, 0);
    } else {
        netHost = enet_host_create(nullptr, 1, CHANNEL_COUNT, 0, 0);
        enet_address_set_host(&addr, "127.0.0.1");
        addr.port = NETWORK_PORT;
        netPeer = enet_host_connect(netHost, &addr, CHANNEL_COUNT, 0);
    }

    while (!WindowShouldClose()) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();
        float dt = GetFrameTime();
        int fps = GetFPS();

        processNetworkEvents();

        if (IsKeyPressed(KEY_G)) debugDrawGrid = !debugDrawGrid;
        if (IsKeyPressed(KEY_L)) startAsyncLoad();

        if (menuState == MENU_TITLE) {
            if (IsKeyPressed(KEY_SPACE)) {
                menuState = MENU_USERNAME;
                usernameLen = 0;
                username[0] = 0;
                usernameValid = false;
            }
        } else if (menuState == MENU_USERNAME) {
            int k = GetCharPressed();
            while (k > 0) {
                if (isValidUsernameChar(k) && usernameLen < 15) {
                    username[usernameLen++] = k;
                    username[usernameLen] = 0;
                }
                k = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && usernameLen > 0) username[--usernameLen] = 0;
            usernameValid = usernameLen > 0;
            if (IsKeyPressed(KEY_ENTER) && usernameValid) {
                menuState = MENU_DIFFICULTY;
                selectedDifficulty = 0;
            }
        } else if (menuState == MENU_DIFFICULTY) {
            if (IsKeyPressed(KEY_UP)) selectedDifficulty = (selectedDifficulty + 3) % 4;
            if (IsKeyPressed(KEY_DOWN)) selectedDifficulty = (selectedDifficulty + 1) % 4;
            if (IsKeyPressed(KEY_ENTER)) {
                setDifficultyOptions();
                ResetGame();
                startCountdown();
            }
        } else if (menuState == MENU_COUNTDOWN) {
            countdownTimer += dt;
            if (countdownTimer >= 1.0f) {
                countdownTimer = 0;
                countdownValue--;
                if (countdownValue < 0) {
                    menuState = MENU_PLAYING;
                    gameStarted = true;
                    currentBallSpeed = 5.0f;
                }
            }
        }
        if (menuState == MENU_GAMEOVER) {
            if (IsKeyPressed(KEY_S) && !scoreSaved) {
                if (usernameLen == 0) strcpy(username, "PLAYER");
                addScoreToLeaderboard(username, currentScore);
                scoreSaved = true;
                menuState = MENU_LEADERBOARD;
            }
        }
        if (menuState == MENU_LEADERBOARD) {
            if (IsKeyPressed(KEY_SPACE)) menuState = MENU_TITLE;
        }

        if (isClient && menuState == MENU_PLAYING) {
            sendInputToHost();
            if (!connectionReady) readState();
        }

        if (isHost && menuState == MENU_PLAYING) {
            if (IsKeyDown(KEY_A) && paddlePosition.x > 0) paddlePosition.x -= PADDLE_SPEED;
            if (IsKeyDown(KEY_D) && paddlePosition.x + paddleWidth < w) paddlePosition.x += PADDLE_SPEED;

            if (connectionReady) {
                if (clientInputLeft && paddleRightX > 0) paddleRightX -= PADDLE_SPEED;
                if (clientInputRight && paddleRightX + paddleWidth < w) paddleRightX += PADDLE_SPEED;
            } else {
                readInput();
                if (clientInputLeft && paddleRightX > 0) paddleRightX -= PADDLE_SPEED;
                if (clientInputRight && paddleRightX + paddleWidth < w) paddleRightX += PADDLE_SPEED;
            }

            if (gameStarted && !gameOver) {
                if (!ballTouchedPaddle) ballVelocity.y = slowBallDropSpeed;
                ballVelocity.y += gravity;
                ballPosition.x += ballVelocity.x;
                ballPosition.y += ballVelocity.y;
                AddBallTrail(ballPosition);

                if (ballPosition.x - ballRadius <= 0 || ballPosition.x + ballRadius >= w) ballVelocity.x *= -1;
                if (ballPosition.y - ballRadius <= 0) ballVelocity.y *= -1;

                Rectangle p1 = { paddlePosition.x, paddlePosition.y, paddleWidth, paddleHeight };
                Rectangle p2 = { paddleRightX, paddlePosition.y, paddleWidth, paddleHeight };
                if (CheckCollisionCircleRec(ballPosition, ballRadius, p1) && ballVelocity.y > 0) {
                    float hit = (ballPosition.x - paddlePosition.x) / paddleWidth - 0.5f;
                    ballVelocity.x = hit * currentBallSpeed * 1.2f;
                    ballVelocity.y = -fabs(currentBallSpeed);
                    ballTouchedPaddle = true;
                    currentScore += 10;
                }
                if (CheckCollisionCircleRec(ballPosition, ballRadius, p2) && ballVelocity.y > 0) {
                    float hit = (ballPosition.x - paddleRightX) / paddleWidth - 0.5f;
                    ballVelocity.x = hit * currentBallSpeed * 1.2f;
                    ballVelocity.y = -fabs(currentBallSpeed);
                    ballTouchedPaddle = true;
                    currentScore += 10;
                }
                if (difficultySpeedIncrease && ballTouchedPaddle) currentBallSpeed *= 1.0008f;

                bool hitBrick = false;
                auto t1 = chrono::high_resolution_clock::now();
                if (spatialGrid) {
                    vector<pair<int, int>> cand;
                    GetBricksInRadius(spatialGrid, ballPosition, ballRadius, cand);
                    for (auto& pr : cand) {
                        if (hitBrick) break;
                        int i = pr.first, j = pr.second;
                        if (i < 0 || i >= brickRows || j < 0 || j >= brickCols) continue;
                        if (bricks[i][j]) {
                            Rectangle br = { (float)j*brickW, (float)i*brickH, (float)brickW - 2, (float)brickH - 2 };
                            if (CheckCollisionCircleRec(ballPosition, ballRadius, br)) {
                                bricks[i][j] = 0;
                                UpdateGridCell(spatialGrid, i, j, false);
                                ballVelocity.y *= -1;
                                ballVelocity.x += GetRandomValue(-1, 1)*0.5f;
                                currentScore += GetBrickScoreByRow(i);
                                spawnBrickParticles(i, j, brickColors[i % 6]);
                                AddBrickExplosion({ (float)(j*brickW + brickW / 2), (float)(i*brickH + brickH / 2) }, brickColors[i % 6]);
                                hitBrick = true;
                            }
                        }
                    }
                } else {
                    for (int i = 0; i < brickRows && !hitBrick; i++) {
                        for (int j = 0; j < brickCols && !hitBrick; j++) {
                            if (bricks[i][j]) {
                                Rectangle br = { (float)j*brickW, (float)i*brickH, (float)brickW - 2, (float)brickH - 2 };
                                if (CheckCollisionCircleRec(ballPosition, ballRadius, br)) {
                                    bricks[i][j] = 0;
                                    ballVelocity.y *= -1;
                                    ballVelocity.x += GetRandomValue(-1, 1)*0.5f;
                                    currentScore += GetBrickScoreByRow(i);
                                    spawnBrickParticles(i, j, brickColors[i % 6]);
                                    AddBrickExplosion({ (float)(j*brickW + brickW / 2), (float)(i*brickH + brickH / 2) }, brickColors[i % 6]);
                                    hitBrick = true;
                                }
                            }
                        }
                    }
                }
                auto t2 = chrono::high_resolution_clock::now();
                float ms = chrono::duration<float, milli>(t2 - t1).count();
                lastCollisionCheckTime = ms;
                avgCollisionCheckTime = avgCollisionCheckTime*0.9f + ms*0.1f;

                if (ballPosition.y + ballRadius >= h) {
                    lives--;
                    if (lives <= 0) { gameOver = true; menuState = MENU_GAMEOVER; }
                    resetBall(w, h, slowBallDropSpeed);
                    ballTouchedPaddle = false;
                }
            }
            sendStateToClient();
        } else if (isClient && menuState == MENU_PLAYING) {
            if (IsKeyDown(KEY_LEFT) && paddleRightX > 0) paddleRightX -= PADDLE_SPEED;
            if (IsKeyDown(KEY_RIGHT) && paddleRightX + paddleWidth < w) paddleRightX += PADDLE_SPEED;
            sendInputToHost();
            if (!connectionReady) readState();
        }

        updateParticles(dt);
        UpdateBallTrail(dt);
        UpdateBrickParticles(dt);

        BeginDrawing();
        if (menuState == MENU_DIFFICULTY) ClearBackground(YELLOW);
        else ClearBackground(RAYWHITE);

        if (menuState == MENU_TITLE) {
            DrawCenteredText("BRICK BREAKOUT 2P", 100, 50, DARKBLUE);
            DrawCenteredText("1P: A/D | 2P: ←→", 200, 25, DARKGRAY);
            DrawCenteredText("PRESS SPACE", 320, 25, DARKGRAY);
        } else if (menuState == MENU_USERNAME) {
            DrawCenteredText("ENTER USERNAME", 100, 30, DARKBLUE);
            DrawText(username, 300, 200, 30, BLACK);
            DrawCenteredText("PRESS ENTER", 350, 20, DARKGRAY);
        } else if (menuState == MENU_DIFFICULTY) {
            DrawCenteredText("SELECT DIFFICULTY", 80, 40, DARKBLUE);
            const char* nms[] = { "EASY", "NORMAL", "HARD", "HELL" };
            for (int i = 0; i < 4; i++) {
                Color c = (i == selectedDifficulty) ? RED : BLACK;
                DrawText(nms[i], 300, 150 + i*50, 30, c);
            }
        } else if (menuState == MENU_COUNTDOWN) {
            DrawCenteredText(TextFormat("%d", countdownValue), 250, 80, RED);
        } else if (menuState == MENU_GAMEOVER) {
            DrawCenteredText("GAME OVER", 100, 50, RED);
            DrawCenteredText(TextFormat("SCORE: %d", currentScore), 180, 30, BLACK);
            DrawCenteredText("PRESS S TO SAVE", 350, 20, DARKGRAY);
        } else if (menuState == MENU_LEADERBOARD) {
            DrawCenteredText("LEADERBOARD", 60, 50, BLUE);
            for (int i = 0; i < (int)leaderboard.size(); i++) {
                DrawText(TextFormat("%d. %s - %d", i + 1, leaderboard[i].name, leaderboard[i].score), 250, 120 + i*30, 20, DARKGRAY);
            }
        } else {
            drawBricks();
            drawPaddle();
            DrawRectangle(paddleRightX, paddlePosition.y, paddleWidth, paddleHeight, RED);
            DrawBallTrail();
            DrawCircleV(ballPosition, ballRadius, MAROON);
            drawParticles();
            DrawBrickParticles();
            if (debugDrawGrid && spatialGrid) DebugDrawGrid(spatialGrid);

            DrawRectangle(0, 0, w, 40, Fade(DARKGRAY, 0.2f));
            DrawText(TextFormat("SCORE: %d", currentScore), 15, 10, 24, BLACK);
            DrawText(TextFormat("LIVES: %d", lives), w - 140, 10, 24, RED);
            Color fpsColor = (fps >= 100) ? GREEN : (fps >= 60) ? YELLOW : RED;
            DrawText(TextFormat("FPS: %d", fps), w / 2 - 40, 10, 22, fpsColor);
        }

        if (isHost) {
            DrawText("HOST 1P | A/D", 15, h - 25, 20, BLUE);
        } else {
            DrawText("CLIENT 2P | ←→", 15, h - 25, 20, RED);
        }
        DrawText("G:GRID  L:LOAD", w - 180, h - 25, 20, DARKBLUE);

        EndDrawing();
    }

    if (netHost) enet_host_destroy(netHost);
    if (gResourceManager) { DestroyResourceManager(gResourceManager); gResourceManager = nullptr; }
    if (spatialGrid) { DestroySpatialGrid(spatialGrid); spatialGrid = nullptr; }
    CloseWindow();
    return 0;
}