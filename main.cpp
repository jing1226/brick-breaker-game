#include "raylib.h"
#include "Brick.h"
#include "Paddle.h"
#include "Ball.h"
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
        } else {
            break;
        }
    }
    f.close();
    sort(leaderboard.begin(), leaderboard.end(), [](const ScoreEntry &a, const ScoreEntry &b) {
        return a.score > b.score;
    });
    if (leaderboard.size() > 10) leaderboard.resize(10);
}

void writeLeaderboard() {
    ofstream f(LEADERBOARD_FILE, ios::trunc);
    if (!f.is_open()) return;
    for (auto &entry : leaderboard) {
        f << entry.name << " " << entry.score << "\n";
    }
    f.close();
}

void addScoreToLeaderboard(const char* name, int score) {
    ScoreEntry entry;
    strncpy(entry.name, name, sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
    entry.score = score;
    leaderboard.push_back(entry);
    sort(leaderboard.begin(), leaderboard.end(), [](const ScoreEntry &a, const ScoreEntry &b) {
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
    int count = 12;
    for (int i = 0; i < count; i++) {
        if (particleCount >= 200) break;
        Particle &p = particles[particleCount++];
        p.active = true;
        p.position = {(float)(col * brickW + brickW/2), (float)(row * brickH + brickH/2)};
        float angle = (float)(GetRandomValue(0, 360)) * DEG2RAD;
        float speed = GetRandomValue(50, 120) / 60.0f;
        p.velocity = {cosf(angle) * speed, sinf(angle) * speed - 1.0f};
        p.color = color;
        p.life = 0.4f + GetRandomValue(0, 20) / 100.0f;
    }
}

void updateParticles(float dt) {
    for (int i = 0; i < particleCount; i++) {
        Particle &p = particles[i];
        if (!p.active) continue;
        p.position.x += p.velocity.x * dt * 60.0f;
        p.position.y += p.velocity.y * dt * 60.0f;
        p.velocity.y += gravity * 30.0f * dt;
        p.life -= dt;
        if (p.life <= 0) p.active = false;
    }
}

void drawParticles() {
    for (int i = 0; i < particleCount; i++) {
        Particle &p = particles[i];
        if (!p.active) continue;
        DrawPixelV(p.position, p.color);
    }
}

void startCountdown() {
    menuState = MENU_COUNTDOWN;
    gameOver = false;
    gameStarted = false;
    ballTouchedPaddle = false;
    currentBallSpeed = slowBallDropSpeed;
    countdownTimer = 0.0f;
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
    initPaddle(SCREEN_WIDTH, SCREEN_HEIGHT);
    resetBall(SCREEN_WIDTH, SCREEN_HEIGHT, slowBallDropSpeed);
    paddleRightX = SCREEN_WIDTH - 150;
}

void writeState() {
    FILE* f = fopen(SYNC_FILE, "wb");
    if (!f) return;
    GameStatePacket packet;
    packet.type = PACKET_STATE;
    packet.padLeftX = paddlePosition.x;
    packet.padRightX = paddleRightX;
    packet.ballX = ballPosition.x;
    packet.ballY = ballPosition.y;
    packet.velX = ballVelocity.x;
    packet.velY = ballVelocity.y;
    packet.score = currentScore;
    packet.lives = lives;
    packet.running = (uint8_t)(gameStarted ? 1 : 0);
    packet.gameOver = (uint8_t)(gameOver ? 1 : 0);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 14; j++) {
            packet.bricks[i][j] = bricks[i][j];
        }
    }
    fwrite(&packet, sizeof(packet), 1, f);
    fclose(f);
}

void writeInput() {
    FILE* f = fopen("/tmp/breakout_input.dat", "wb");
    if (!f) return;
    InputPacket packet;
    packet.type = PACKET_INPUT;
    packet.left = IsKeyDown(KEY_LEFT) ? 1 : 0;
    packet.right = IsKeyDown(KEY_RIGHT) ? 1 : 0;
    fwrite(&packet, sizeof(packet), 1, f);
    fclose(f);
}

void sendStateToClient() {
    if (!isHost || !connectionReady || !netPeer) {
        // Fallback to file sync if network fails
        writeState();
        return;
    }

    GameStatePacket packet;
    packet.type = PACKET_STATE;
    packet.padLeftX = paddlePosition.x;
    packet.padRightX = paddleRightX;
    packet.ballX = ballPosition.x;
    packet.ballY = ballPosition.y;
    packet.velX = ballVelocity.x;
    packet.velY = ballVelocity.y;
    packet.score = currentScore;
    packet.lives = lives;
    packet.running = (uint8_t)(gameStarted ? 1 : 0);
    packet.gameOver = (uint8_t)(gameOver ? 1 : 0);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 14; j++) {
            packet.bricks[i][j] = bricks[i][j];
        }
    }

    ENetPacket* netPacket = enet_packet_create(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(netPeer, 0, netPacket);
    enet_host_flush(netHost);
}

void sendInputToHost() {
    if (!isClient || !connectionReady || !netPeer) {
        // Fallback to file sync if network fails
        writeInput();
        return;
    }

    InputPacket packet;
    packet.type = PACKET_INPUT;
    packet.left = IsKeyDown(KEY_LEFT) ? 1 : 0;
    packet.right = IsKeyDown(KEY_RIGHT) ? 1 : 0;

    ENetPacket* netPacket = enet_packet_create(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(netPeer, 0, netPacket);
    enet_host_flush(netHost);
}

void readState() {
    FILE* f = fopen(SYNC_FILE, "rb");
    if (!f) return;
    GameStatePacket packet;
    if (fread(&packet, sizeof(packet), 1, f) == 1) {
        paddlePosition.x = packet.padLeftX;
        paddleRightX = packet.padRightX;
        ballPosition.x = packet.ballX;
        ballPosition.y = packet.ballY;
        ballVelocity.x = packet.velX;
        ballVelocity.y = packet.velY;
        currentScore = packet.score;
        lives = packet.lives;
        gameStarted = packet.running != 0;
        gameOver = packet.gameOver != 0;
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 14; j++) {
                bricks[i][j] = packet.bricks[i][j];
            }
        }
    }
    fclose(f);
}

void readInput() {
    FILE* f = fopen("/tmp/breakout_input.dat", "rb");
    if (!f) return;
    InputPacket packet;
    if (fread(&packet, sizeof(packet), 1, f) == 1) {
        clientInputLeft = packet.left;
        clientInputRight = packet.right;
    }
    fclose(f);
}

bool isAsyncLoadComplete() {
    std::lock_guard<std::mutex> guard(loadMutex);
    return asyncLoadComplete;
}

void startAsyncLoad() {
    std::lock_guard<std::mutex> guard(loadMutex);
    if (isLoading || loadFinished) return;

    isLoading = true;
    loadFinished = false;

    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::lock_guard<std::mutex> guard(loadMutex);
        loadFinished = true;
        isLoading = false;
        asyncLoadComplete = true;
    }).detach();
}

void processNetworkEvents() {
    if (!netHost) return;

    ENetEvent event;
    while (enet_host_service(netHost, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                netPeer = event.peer;
                connectionReady = true;
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                if (event.packet->dataLength >= 1) {
                    const uint8_t* packetData = static_cast<const uint8_t*>(event.packet->data);
                    uint8_t type = packetData[0];
                    if (type == PACKET_INPUT && isHost && event.packet->dataLength == sizeof(InputPacket)) {
                        const InputPacket* input = reinterpret_cast<const InputPacket*>(packetData);
                        clientInputLeft = input->left;
                        clientInputRight = input->right;
                    }
                    if (type == PACKET_STATE && isClient && event.packet->dataLength == sizeof(GameStatePacket)) {
                        const GameStatePacket* statePacket = reinterpret_cast<const GameStatePacket*>(packetData);
                        paddlePosition.x = statePacket->padLeftX;
                        paddleRightX = statePacket->padRightX;
                        ballPosition.x = statePacket->ballX;
                        ballPosition.y = statePacket->ballY;
                        ballVelocity.x = statePacket->velX;
                        ballVelocity.y = statePacket->velY;
                        currentScore = statePacket->score;
                        lives = statePacket->lives;
                        gameStarted = statePacket->running != 0;
                        gameOver = statePacket->gameOver != 0;
                        if (gameStarted && menuState != MENU_PLAYING) {
                            menuState = MENU_PLAYING;
                        }
                        if (gameOver) {
                            menuState = MENU_GAMEOVER;
                        }
                        for (int i = 0; i < 10; i++) {
                            for (int j = 0; j < 14; j++) {
                                bricks[i][j] = statePacket->bricks[i][j];
                            }
                        }
                    }
                }
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                connectionReady = false;
                netPeer = nullptr;
                break;
            default:
                break;
        }
    }
}

int main(int argc, char** argv) {
    if (argc >= 2) {
        if (strcmp(argv[1], "host") == 0) {
            isHost = true;
        } else if (strcmp(argv[1], "client") == 0) {
            isClient = true;
        }
    }

    if (!isHost && !isClient) {
        isHost = true;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, isHost ? "HOST (1P)" : "CLIENT (2P)");
    SetTargetFPS(60);
    loadLeaderboard();
    ResetGame();

    if (enet_initialize() != 0) {
        DrawText("ENET INIT FAILED", 10, 100, 20, RED);
        return 0;
    }

    atexit(enet_deinitialize);

    ENetAddress address;
    if (isHost) {
        enet_address_set_host(&address, "0.0.0.0");
        address.port = NETWORK_PORT;
        netHost = enet_host_create(&address, MAX_CLIENTS, CHANNEL_COUNT, 0, 0);
        if (!netHost) {
            DrawText("HOST CREATE FAILED", 10, 100, 20, RED);
            return 0;
        }
    } else {
        netHost = enet_host_create(nullptr, 1, CHANNEL_COUNT, 0, 0);
        if (!netHost) {
            DrawText("CLIENT HOST CREATE FAILED", 10, 100, 20, RED);
            return 0;
        }
        if (argc >= 3) {
            enet_address_set_host(&address, argv[2]);
        } else {
            enet_address_set_host(&address, "127.0.0.1");
        }
        address.port = NETWORK_PORT;
        netPeer = enet_host_connect(netHost, &address, CHANNEL_COUNT, 0);
        if (!netPeer) {
            DrawText("CONNECT FAILED", 10, 100, 20, RED);
            return 0;
        }
    }

    while (!WindowShouldClose()) {
        int w = GetScreenWidth();
        int h = GetScreenHeight();

        processNetworkEvents();

        if (IsKeyPressed(KEY_L)) {
            startAsyncLoad();
        }

        bool loading = false;
        bool finished = false;
        {
            std::lock_guard<std::mutex> guard(loadMutex);
            loading = isLoading;
            finished = loadFinished;
        }

        if (menuState == MENU_TITLE) {
            if (IsKeyPressed(KEY_SPACE)) {
                menuState = MENU_USERNAME;
                usernameLen = 0;
                username[0] = '\0';
                usernameValid = false;
            }
        } else if (menuState == MENU_USERNAME) {
            int key = GetCharPressed();
            while (key > 0) {
                if (isValidUsernameChar(key) && usernameLen < 15) {
                    username[usernameLen++] = (char)key;
                    username[usernameLen] = '\0';
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && usernameLen > 0) {
                username[--usernameLen] = '\0';
            }
            usernameValid = usernameLen > 0;
            if (IsKeyPressed(KEY_ENTER) && usernameValid) {
                menuState = MENU_DIFFICULTY;
                selectedDifficulty = 0;
            }
        } else if (menuState == MENU_DIFFICULTY) {
            if (IsKeyPressed(KEY_UP)) {
                selectedDifficulty = (selectedDifficulty + 3) % 4;
            }
            if (IsKeyPressed(KEY_DOWN)) {
                selectedDifficulty = (selectedDifficulty + 1) % 4;
            }
            if (IsKeyPressed(KEY_ENTER)) {
                setDifficultyOptions();
                ResetGame();
                startCountdown();
            }
        } else if (menuState == MENU_COUNTDOWN) {
            countdownTimer += GetFrameTime();
            if (countdownTimer >= 1.0f) {
                countdownTimer = 0.0f;
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
                if (usernameLen == 0) {
                    strncpy(username, "PLAYER", sizeof(username) - 1);
                    username[sizeof(username) - 1] = '\0';
                }
                addScoreToLeaderboard(username, currentScore);
                scoreSaved = true;
                menuState = MENU_LEADERBOARD;
            }
        }

        if (menuState == MENU_LEADERBOARD) {
            if (IsKeyPressed(KEY_SPACE)) {
                menuState = MENU_TITLE;
            }
        }

        if (isClient && menuState == MENU_PLAYING) {
            sendInputToHost();
            if (!connectionReady) {
                readState(); // Fallback to file sync
            }
        }

        if (isHost && menuState == MENU_PLAYING) {
            if (IsKeyDown(KEY_A) && paddlePosition.x > 0)
                paddlePosition.x -= PADDLE_SPEED;
            if (IsKeyDown(KEY_D) && paddlePosition.x + paddleWidth < w)
                paddlePosition.x += PADDLE_SPEED;

            if (connectionReady) {
                if (clientInputLeft && paddleRightX > 0)
                    paddleRightX -= PADDLE_SPEED;
                if (clientInputRight && paddleRightX + paddleWidth < w)
                    paddleRightX += PADDLE_SPEED;
            } else {
                readInput();
                if (clientInputLeft && paddleRightX > 0)
                    paddleRightX -= PADDLE_SPEED;
                if (clientInputRight && paddleRightX + paddleWidth < w)
                    paddleRightX += PADDLE_SPEED;
            }

            if (gameStarted && !gameOver) {
                if (!ballTouchedPaddle) {
                    ballVelocity.y = slowBallDropSpeed;
                }
                ballVelocity.y += gravity;

                ballPosition.x += ballVelocity.x;
                ballPosition.y += ballVelocity.y;

                if (ballPosition.x - ballRadius <= 0 || ballPosition.x + ballRadius >= w)
                    ballVelocity.x *= -1;
                if (ballPosition.y - ballRadius <= 0)
                    ballVelocity.y *= -1;

                Rectangle p1 = {paddlePosition.x, paddlePosition.y, paddleWidth, paddleHeight};
                Rectangle p2 = {paddleRightX, paddlePosition.y, paddleWidth, paddleHeight};
                if (CheckCollisionCircleRec(ballPosition, ballRadius, p1) && ballVelocity.y > 0) {
                    float hitPos = (ballPosition.x - paddlePosition.x) / paddleWidth - 0.5f;
                    ballVelocity.x = hitPos * currentBallSpeed * 1.2f;
                    ballVelocity.y = -fabs(currentBallSpeed);
                    ballTouchedPaddle = true;
                    currentScore += 10;
                }
                if (CheckCollisionCircleRec(ballPosition, ballRadius, p2) && ballVelocity.y > 0) {
                    float hitPos = (ballPosition.x - paddleRightX) / paddleWidth - 0.5f;
                    ballVelocity.x = hitPos * currentBallSpeed * 1.2f;
                    ballVelocity.y = -fabs(currentBallSpeed);
                    ballTouchedPaddle = true;
                    currentScore += 10;
                }

                if (difficultySpeedIncrease && ballTouchedPaddle) {
                    currentBallSpeed *= 1.0008f;
                }

                bool hitBrick = false;
                for (int i = 0; i < brickRows && !hitBrick; i++) {
                    for (int j = 0; j < brickCols && !hitBrick; j++) {
                        if (bricks[i][j]) {
                            Rectangle br = {(float)(j * brickW), (float)(i * brickH), (float)(brickW - 2), (float)(brickH - 2)};
                            if (CheckCollisionCircleRec(ballPosition, ballRadius, br)) {
                                bricks[i][j] = 0;
                                ballVelocity.y *= -1;
                                ballVelocity.x += (GetRandomValue(-1, 1) * 0.5f);
                                currentScore += GetBrickScoreByRow(i);
                                spawnBrickParticles(i, j, brickColors[i % 6]);
                                hitBrick = true;
                            }
                        }
                    }
                }

                if (ballPosition.y + ballRadius >= h) {
                    lives--;
                    if (lives <= 0) {
                        gameOver = true;
                        menuState = MENU_GAMEOVER;
                    }
                    resetBall(w, h, slowBallDropSpeed);
                    ballTouchedPaddle = false;
                }
            }

            sendStateToClient();
        } else if (isClient && menuState == MENU_PLAYING) {
            if (IsKeyDown(KEY_LEFT) && paddleRightX > 0)
                paddleRightX -= PADDLE_SPEED;
            if (IsKeyDown(KEY_RIGHT) && paddleRightX + paddleWidth < w)
                paddleRightX += PADDLE_SPEED;

            sendInputToHost();
            if (!connectionReady) {
                readState(); // Fallback to file sync
            }
        }

        updateParticles(GetFrameTime());

        BeginDrawing();

        if (menuState == MENU_DIFFICULTY) {
            ClearBackground(YELLOW);
        } else {
            ClearBackground(RAYWHITE);
        }

        if (menuState == MENU_TITLE) {
            DrawCenteredText("BRICK BREAKOUT 2P", 100, 50, DARKBLUE);
            DrawCenteredText("1P: A/D KEYS", 200, 25, DARKGRAY);
            DrawCenteredText("2P: ARROW KEYS", 240, 25, DARKGRAY);
            DrawCenteredText("PRESS SPACE TO ENTER USERNAME", 320, 25, DARKGRAY);
            DrawCenteredText("PRESS ENTER TO OPEN DIFFICULTY AFTER USERNAME", 360, 20, DARKGRAY);
        } else if (menuState == MENU_USERNAME) {
            DrawCenteredText("ENTER USERNAME (LETTERS & NUMBERS, <15 CHARS)", 100, 20, DARKBLUE);
            DrawText(username, 300, 200, 30, BLACK);
            if (!usernameValid) {
                DrawText("USERNAME REQUIRED", 300, 250, 20, RED);
            }
            DrawCenteredText("PRESS ENTER TO CONFIRM", 350, 20, DARKGRAY);
        } else if (menuState == MENU_DIFFICULTY) {
            DrawCenteredText("SELECT DIFFICULTY", 80, 40, DARKBLUE);
            const char* difficultyNames[4] = {"1. EASY", "2. NORMAL", "3. HARD", "4. HELL"};
            const char* difficultyDescription[4] = {
                "2 lives, speed constant.",
                "2 lives, speed increases.",
                "1 life, speed constant.",
                "10 rows, 3 lives, hard mode."
            };
            for (int i = 0; i < 4; i++) {
                Color color = (i == selectedDifficulty) ? RED : BLACK;
                DrawText(difficultyNames[i], 300, 150 + i * 50, 30, color);
            }
            DrawText(difficultyDescription[selectedDifficulty], 300, 350, 20, DARKGRAY);
            DrawCenteredText("USE UP/DOWN TO CHANGE, ENTER TO START", 450, 20, DARKGRAY);
        } else if (menuState == MENU_COUNTDOWN) {
            DrawCenteredText("GET READY", 100, 40, DARKBLUE);
            const char* text = countdownValue > 0 ? TextFormat("%d", countdownValue) : "GO!";
            DrawCenteredText(text, 250, 80, RED);
        } else if (menuState == MENU_GAMEOVER) {
            DrawCenteredText("GAME OVER", 100, 50, RED);
            DrawCenteredText(TextFormat("SCORE: %d", currentScore), 180, 30, BLACK);
            DrawCenteredText("PRESS S TO SAVE SCORE AND VIEW LEADERBOARD", 350, 20, DARKGRAY);
        } else if (menuState == MENU_LEADERBOARD) {
            DrawCenteredText("LEADERBOARD", 60, 50, BLUE);
            for (int i = 0; i < (int)leaderboard.size(); i++) {
                DrawText(TextFormat("%d. %s - %d", i + 1, leaderboard[i].name, leaderboard[i].score), 250, 120 + i * 30, 20, DARKGRAY);
            }
            DrawCenteredText("PRESS SPACE TO RETURN TO TITLE", 500, 20, DARKGRAY);
        } else {
            drawBricks();
            drawPaddle();
            DrawRectangle(paddleRightX, paddlePosition.y, paddleWidth, paddleHeight, RED);
            DrawCircleV(ballPosition, ballRadius, MAROON);
            drawParticles();
            DrawText(TextFormat("SCORE: %d", currentScore), 20, 20, 25, BLACK);
            DrawText(TextFormat("LIVES: %d", lives), w - 120, 20, 25, BLACK);
        }

        if (isHost) {
            DrawText("HOST (1P)", 10, SCREEN_HEIGHT - 30, 20, BLUE);
            DrawText(connectionReady ? "CLIENT CONNECTED" : "FILE SYNC MODE", 10, 50, 20, connectionReady ? GREEN : ORANGE);
            DrawText(TextFormat("C L:%d R:%d", clientInputLeft, clientInputRight), 10, 75, 20, DARKGRAY);
            DrawText("USE A/D", 10, 100, 20, DARKGRAY);
        } else {
            DrawText("CLIENT (2P)", 10, SCREEN_HEIGHT - 30, 20, RED);
            DrawText(connectionReady ? "CONNECTED" : "FILE SYNC MODE", 10, 50, 20, connectionReady ? GREEN : ORANGE);
            DrawText("USE ARROW KEYS", 10, 75, 20, DARKGRAY);
            DrawText(TextFormat("PEER: %s", netPeer ? "OK" : "NULL"), 10, 100, 20, DARKGRAY);
        }

        DrawText("PRESS L TO LOAD ASSET", 10, 125, 20, DARKBLUE);
        if (loading) {
            DrawCenteredText("LOADING ASSET...", h/2, 40, PURPLE);
        } else if (finished) {
            DrawText("LOAD COMPLETE! BRICKS TURN GREEN.", 10, 150, 20, GREEN);
        }

        EndDrawing();
    }

    if (netHost) {
        enet_host_destroy(netHost);
    }
    CloseWindow();
    return 0;
}
