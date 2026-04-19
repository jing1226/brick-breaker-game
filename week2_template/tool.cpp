#include "Tool.h"

Tool CreateTool(Vector2 pos, ToolType type) {
    Tool t;
    t.type = type;
    t.position = pos;
    t.active = true;
    return t;
}

void UpdateTool(Tool *t, float dt) {
    if (!t->active) return;
    t->position.y += 100.0f * dt;
}

void DrawTool(Tool t) {
    if (!t.active) return;
    Color color;
    switch (t.type) {
        case TOOL_MULTI_BALL: color = BLUE; break;
        case TOOL_BOMB: color = RED; break;
        case TOOL_SLOW: color = GREEN; break;
        default: color = WHITE; break;
    }
    DrawCircleV(t.position, 15, color);
    DrawCircleLinesV(t.position, 15, BLACK);
}

void ApplyTool(Tool t, float *ballSpeed) {
    switch (t.type) {
        case TOOL_MULTI_BALL:
            // 多球效果（这里先占位，后续可以扩展）
            break;
        case TOOL_BOMB:
            // 炸弹效果（这里先占位，后续可以扩展）
            break;
        case TOOL_SLOW:
            *ballSpeed *= 0.5f;
            break;
    }
}