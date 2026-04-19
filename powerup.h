#ifndef POWERUP_H
#define POWERUP_H

#include "raylib.h"

typedef enum {
    POWERUP_PADDLE_EXTEND,   // 加长板
    POWERUP_MULTI_BALL,      // 多球
    POWERUP_SLOW_BALL        // 减速球
} PowerUpType;

typedef struct {
    PowerUpType type;
    Vector2 position;
    bool active;      // 是否在场景中
    float duration;   // 持续时间（秒），0 表示永久
    float lifetime;   // 剩余持续时间
} PowerUp;

PowerUp CreatePowerUp(Vector2 pos, PowerUpType type);
void UpdatePowerUp(PowerUp *p, float dt);
void DrawPowerUp(PowerUp p);
void ApplyPowerUp(PowerUp p, float *paddleWidth, float *ballSpeed);

#endif