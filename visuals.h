#pragma once
#include "interfaces.h"
#include "imgui.h"
#include <vector>

struct EntityEntry {
    Rect bbox;
    int health;
    int team;
    const char* name;
    bool visible;
    float distance;
};

class Visuals {
public:
    void Render();

private:
    std::vector<EntityEntry> GetValidEntities();
    bool WorldToScreen(const Vector3& world, Vector2& screen);
    bool GetBoundingBox(uintptr_t pawn, Rect& out);
    void RenderPlayerESP(const EntityEntry& entry);
    void DrawBox(const Rect& rect, ImColor color);
};
