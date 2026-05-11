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

float paddleRightX = SCREEN_WIDTH - 150;
const float PADDLE_SPEED = 6.0f;

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

void ResetGame() {
    currentScore = 0;
    lives = 3;
    gameOver = false;
    gameStarted = false;
    initBricks();
    initPaddle(SCREEN_WIDTH, SCREEN_HEIGHT);
    resetBall(SCREEN_WIDTH, SCREEN_HEIGHT);
    paddleRightX = SCREEN_WIDTH - 150;
}

void DrawCenteredText(const char* text, int y, int fontSize, Color color) {
    int w = MeasureText(text, fontSize);
    DrawText(text, (SCREEN_WIDTH - w) / 2, y, fontSize, color);
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
            // Try WSL IP first, fallback to localhost
            enet_address_set_host(&address, "172.21.181.34");
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

        if (isClient) {
            sendInputToHost();
            if (!connectionReady) {
                readState(); // Fallback to file sync
            }
        }

        if (isHost) {
            if (IsKeyPressed(KEY_ENTER)) {
                if (!gameStarted || gameOver) {
                    ResetGame();
                    gameStarted = true;
                }
            }

            if (IsKeyDown(KEY_A) && paddlePosition.x > 0)
                paddlePosition.x -= PADDLE_SPEED;
            if (IsKeyDown(KEY_D) && paddlePosition.x + paddleWidth < w / 2 - 10)
                paddlePosition.x += PADDLE_SPEED;

            if (connectionReady) {
                if (clientInputLeft && paddleRightX > w / 2 + 10)
                    paddleRightX -= PADDLE_SPEED;
                if (clientInputRight && paddleRightX + paddleWidth < w)
                    paddleRightX += PADDLE_SPEED;
            } else {
                // Fallback to file sync for input
                readInput();
                if (clientInputLeft && paddleRightX > w / 2 + 10)
                    paddleRightX -= PADDLE_SPEED;
                if (clientInputRight && paddleRightX + paddleWidth < w)
                    paddleRightX += PADDLE_SPEED;
            }

            if (gameStarted && !gameOver) {
                ballPosition.x += ballVelocity.x;
                ballPosition.y += ballVelocity.y;

                if (ballPosition.x - ballRadius <= 0 || ballPosition.x + ballRadius >= w)
                    ballVelocity.x *= -1;
                if (ballPosition.y - ballRadius <= 0)
                    ballVelocity.y *= -1;

                Rectangle p1 = {paddlePosition.x, paddlePosition.y, paddleWidth, paddleHeight};
                Rectangle p2 = {paddleRightX, paddlePosition.y, paddleWidth, paddleHeight};
                if (CheckCollisionCircleRec(ballPosition, ballRadius, p1) && ballVelocity.y > 0) {
                    ballVelocity.y *= -1;
                    currentScore += 10;
                }
                if (CheckCollisionCircleRec(ballPosition, ballRadius, p2) && ballVelocity.y > 0) {
                    ballVelocity.y *= -1;
                    currentScore += 10;
                }

                for (int i = 0; i < brickRows; i++) {
                    for (int j = 0; j < brickCols; j++) {
                        if (bricks[i][j]) {
                            Rectangle br = {(float)(j * brickW), (float)(i * brickH), (float)(brickW - 2), (float)(brickH - 2)};
                            if (CheckCollisionCircleRec(ballPosition, ballRadius, br)) {
                                bricks[i][j] = 0;
                                ballVelocity.y *= -1;
                                currentScore += 100;
                            }
                        }
                    }
                }

                if (ballPosition.y + ballRadius >= h) {
                    lives--;
                    if (lives <= 0) gameOver = true;
                    resetBall(w, h);
                }
            }

            sendStateToClient();
        } else if (isClient) {
            // Client also runs game logic for smooth gameplay
            if (IsKeyPressed(KEY_SPACE)) {
                if (!gameStarted || gameOver) {
                    ResetGame();
                    gameStarted = true;
                }
            }

            if (IsKeyDown(KEY_LEFT) && paddleRightX > w / 2 + 10)
                paddleRightX -= PADDLE_SPEED;
            if (IsKeyDown(KEY_RIGHT) && paddleRightX + paddleWidth < w)
                paddleRightX += PADDLE_SPEED;

            if (gameStarted && !gameOver) {
                ballPosition.x += ballVelocity.x;
                ballPosition.y += ballVelocity.y;

                if (ballPosition.x - ballRadius <= 0 || ballPosition.x + ballRadius >= w)
                    ballVelocity.x *= -1;
                if (ballPosition.y - ballRadius <= 0)
                    ballVelocity.y *= -1;

                Rectangle p1 = {paddlePosition.x, paddlePosition.y, paddleWidth, paddleHeight};
                Rectangle p2 = {paddleRightX, paddlePosition.y, paddleWidth, paddleHeight};
                if (CheckCollisionCircleRec(ballPosition, ballRadius, p1) && ballVelocity.y > 0) {
                    ballVelocity.y *= -1;
                    currentScore += 10;
                }
                if (CheckCollisionCircleRec(ballPosition, ballRadius, p2) && ballVelocity.y > 0) {
                    ballVelocity.y *= -1;
                    currentScore += 10;
                }

                for (int i = 0; i < brickRows; i++) {
                    for (int j = 0; j < brickCols; j++) {
                        if (bricks[i][j]) {
                            Rectangle br = {(float)(j * brickW), (float)(i * brickH), (float)(brickW - 2), (float)(brickH - 2)};
                            if (CheckCollisionCircleRec(ballPosition, ballRadius, br)) {
                                bricks[i][j] = 0;
                                ballVelocity.y *= -1;
                                currentScore += 100;
                            }
                        }
                    }
                }

                if (ballPosition.y + ballRadius >= h) {
                    lives--;
                    if (lives <= 0) gameOver = true;
                    resetBall(w, h);
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (!gameStarted) {
            DrawCenteredText("BRICK BREAKOUT 2P", 120, 50, DARKBLUE);
            DrawCenteredText("1P: A/D    2P: ←/→", 300, 25, DARKGRAY);
            if (isHost) {
                DrawCenteredText("PRESS ENTER TO START", 380, 20, BLACK);
                if (!connectionReady) {
                    DrawCenteredText("WAITING FOR CLIENT...", 420, 20, DARKGRAY);
                }
            } else {
                DrawCenteredText("PRESS SPACE TO START", 380, 20, BLACK);
                if (!connectionReady) {
                    DrawCenteredText("CONNECTING TO HOST...", 420, 20, DARKGRAY);
                }
            }
        } else if (gameOver) {
            DrawCenteredText("GAME OVER", 180, 60, RED);
            DrawCenteredText(TextFormat("SCORE: %d", currentScore), 260, 40, BLACK);
        } else {
            drawBricks();
            drawPaddle();
            DrawRectangle(paddleRightX, paddlePosition.y, paddleWidth, paddleHeight, RED);
            DrawCircleV(ballPosition, ballRadius, MAROON);
            DrawText(TextFormat("SCORE: %d", currentScore), 20, 20, 25, BLACK);
            DrawText(TextFormat("LIVES: %d", lives), w - 120, 20, 25, BLACK);
        }

        if (isHost) {
            DrawText("HOST (1P)", 10, SCREEN_HEIGHT - 30, 20, BLUE);
            DrawText(connectionReady ? "CLIENT CONNECTED" : "FILE SYNC MODE", 10, 50, 20, connectionReady ? GREEN : ORANGE);
            DrawText(TextFormat("C L:%d R:%d", clientInputLeft, clientInputRight), 10, 75, 20, DARKGRAY);
        } else {
            DrawText("CLIENT (2P)", 10, SCREEN_HEIGHT - 30, 20, RED);
            DrawText(connectionReady ? "CONNECTED" : "FILE SYNC MODE", 10, 50, 20, connectionReady ? GREEN : ORANGE);
            DrawText("USE ARROW KEYS", 10, 75, 20, DARKGRAY);
            DrawText(TextFormat("PEER: %s", netPeer ? "OK" : "NULL"), 10, 100, 20, DARKGRAY);
        }

        EndDrawing();
    }

    if (netHost) {
        enet_host_destroy(netHost);
    }
    CloseWindow();
    return 0;
}
