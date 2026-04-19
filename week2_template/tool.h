#ifndef TOOL_H
#define TOOL_H

#include "raylib.h"

typedef enum {
    TOOL_MULTI_BALL,
    TOOL_BOMB,
    TOOL_SLOW
} ToolType;

typedef struct {
    ToolType type;
    Vector2 position;
    bool active;
} Tool;

Tool CreateTool(Vector2 pos, ToolType type);
void UpdateTool(Tool *t, float dt);
void DrawTool(Tool t);
void ApplyTool(Tool t, float *ballSpeed);

#endif