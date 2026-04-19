#include "PowerUp.h"

PowerUp CreatePowerUp(Vector2 pos, PowerUpType type) {
    PowerUp p;
    p.type = type;
    p.position = pos;
    p.active = true;
    p.duration = 5.0f; // 默认持续5秒
    p.lifetime = p.duration;
    return p;
}

void UpdatePowerUp(PowerUp *p, float dt) {
    if (!p->active) return;
    // 道具向下飘动
    p->position.y += 100.0f * dt;
}

void DrawPowerUp(PowerUp p) {
    if (!p.active) return;
    Color color;
    switch (p.type) {
        case POWERUP_PADDLE_EXTEND: color = GREEN; break;
        case POWERUP_MULTI_BALL:    color = BLUE; break;
        case POWERUP_SLOW_BALL:     color = YELLOW; break;
        default: color = WHITE; break;
    }
    DrawCircleV(p.position, 15, color);
    DrawCircleLinesV(p.position, 15, BLACK);
}

void ApplyPowerUp(PowerUp p, float *paddleWidth, float *ballSpeed) {
    switch (p.type) {
        case POWERUP_PADDLE_EXTEND:
            *paddleWidth = 200.0f; // 挡板加长
            break;
        case POWERUP_MULTI_BALL:
            // 这里先只做逻辑占位，多球可以后续扩展
            break;
        case POWERUP_SLOW_BALL:
            *ballSpeed *= 0.5f; // 球速减半
            break;
    }
}