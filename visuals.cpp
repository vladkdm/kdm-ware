#include "visuals.h"
#include "config.h"
#include "imgui.h"
#include "memory.h"
#include <algorithm>
#include <limits>
#include <cstdio>
#include <cmath>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

extern int g_screen_width;
extern int g_screen_height;

static uintptr_t g_client = 0;
static uintptr_t g_entity_system = 0;
static int g_max_idx = 0;
static uintptr_t g_local_ctrl = 0;
static float(*g_vm)[4] = nullptr;
static bool g_ok = false;
static bool g_inited = false;
static int g_debug_entities = 0;
static int g_frame = 0;

static bool IsValidPtr(uintptr_t p) {
    return p > 0x10000 && p < 0x7FFFFFFFFFFF;
}

static bool IsMemoryReadable(uintptr_t addr, size_t size) {
    if (!addr || size == 0) return false;
    MEMORY_BASIC_INFORMATION mbi;
    for (size_t offset = 0; offset < size;) {
        SIZE_T ret = VirtualQuery((void*)(addr + offset), &mbi, sizeof(mbi));
        if (ret == 0) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
            return false;
        offset += mbi.RegionSize;
    }
    return true;
}

static uintptr_t GetClientBase() {
    return (uintptr_t)GetModuleHandleA("client.dll");
}

static uintptr_t GetClientSize() {
    HMODULE mod = GetModuleHandleA("client.dll");
    if (!mod) return 0;
    MODULEINFO info;
    if (!GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info)))
        return 0;
    return info.SizeOfImage;
}

static bool IsInClientRange(uintptr_t addr, uintptr_t base, uintptr_t size) {
    return addr >= base && addr < base + size;
}

// Get entity pointer from chunked entity list at ES+0x10
// Chunk table: array of 64 pointers to chunks (512 slots x 0x70 each)
// Each slot's first 8 bytes = entity instance pointer
static uintptr_t GetEntityFromIndex(int idx) {
    if (idx < 0 || idx > 32767 || !g_entity_system) return 0;
    int chunk = idx >> 9;
    int slot = idx & 0x1FF;
    uintptr_t chunk_ptr_addr = g_entity_system + 0x10 + chunk * 8;
    if (!IsMemoryReadable(chunk_ptr_addr, sizeof(uintptr_t))) return 0;
    uintptr_t chunk_ptr = *(uintptr_t*)chunk_ptr_addr;
    if (!chunk_ptr || !IsMemoryReadable(chunk_ptr, slot * 0x70 + 8)) return 0;
    return *(uintptr_t*)(chunk_ptr + slot * 0x70);
}

static void Init() {
    if (g_inited) return;
    g_client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!g_client) return;

    uintptr_t client_base = GetClientBase();
    uintptr_t client_size = GetClientSize();

    // Entity system: cs2-dumper offset 0x24E76A0
    uintptr_t es_ptr_addr = client_base + 0x24E76A0;
    if (IsMemoryReadable(es_ptr_addr, sizeof(uintptr_t))) {
        uintptr_t es = *(uintptr_t*)es_ptr_addr;
        if (IsValidPtr(es) && IsMemoryReadable(es, 256)) {
            uintptr_t vtable = *(uintptr_t*)es;
            if (IsInClientRange(vtable, client_base, client_size)) {
                g_entity_system = es;
                if (IsMemoryReadable(es + 0x2090, sizeof(int)))
                    g_max_idx = *(int*)(es + 0x2090);
            }
        }
    }

    // Local player controller: 0x2320720
    uintptr_t lc_ptr_addr = client_base + 0x2320720;
    if (IsMemoryReadable(lc_ptr_addr, sizeof(uintptr_t))) {
        g_local_ctrl = *(uintptr_t*)lc_ptr_addr;
        if (!IsValidPtr(g_local_ctrl))
            g_local_ctrl = 0;
    }

    // View matrix: 0x2346B30
    g_vm = (float(*)[4])(client_base + 0x2346B30);

    g_ok = g_entity_system && IsValidPtr(g_local_ctrl) && g_vm;
    g_inited = true;
}

static bool W2S(const Vector3& world, Vector2& screen) {
    if (!g_vm) return false;
    float(*m)[4] = g_vm;
    float w = m[3][0] * world.x + m[3][1] * world.y + m[3][2] * world.z + m[3][3];
    if (w < 0.001f) return false;
    float x = m[0][0] * world.x + m[0][1] * world.y + m[0][2] * world.z + m[0][3];
    float y = m[1][0] * world.x + m[1][1] * world.y + m[1][2] * world.z + m[1][3];
    screen.x = (g_screen_width / 2.f) * (1.f + x / w);
    screen.y = (g_screen_height / 2.f) * (1.f - y / w);
    return true;
}

static bool W2S_alt(const Vector3& world, Vector2& screen) {
    if (!g_vm) return false;
    float(*m)[4] = g_vm;
    float w = world.x * m[0][3] + world.y * m[1][3] + world.z * m[2][3] + m[3][3];
    if (w < 0.001f) return false;
    float x = world.x * m[0][0] + world.y * m[1][0] + world.z * m[2][0] + m[3][0];
    float y = world.x * m[0][1] + world.y * m[1][1] + world.z * m[2][1] + m[3][1];
    screen.x = (g_screen_width / 2.f) * (1.f + x / w);
    screen.y = (g_screen_height / 2.f) * (1.f - y / w);
    return true;
}

bool Visuals::GetBoundingBox(uintptr_t pawn, Rect& out) {
    uintptr_t collision = *(uintptr_t*)(pawn + 0x340);
    if (!collision) return false;
    uintptr_t scene = *(uintptr_t*)(pawn + 0x330);
    if (!scene) return false;

    Vector3 origin = *(Vector3*)(scene + 0xC8);
    Vector3 mins = *(Vector3*)(collision + 0x40);
    Vector3 maxs = *(Vector3*)(collision + 0x4C);

    Vector3 wmins = { mins.x + origin.x, mins.y + origin.y, mins.z + origin.z };
    Vector3 wmaxs = { maxs.x + origin.x, maxs.y + origin.y, maxs.z + origin.z };

    const float fmax = (std::numeric_limits<float>::max)();
    out.x = out.y = fmax;
    out.w = out.h = -fmax;

    Vector3 corners[8] = {
        {wmins.x, wmins.y, wmins.z}, {wmins.x, wmaxs.y, wmins.z},
        {wmaxs.x, wmaxs.y, wmins.z}, {wmaxs.x, wmins.y, wmins.z},
        {wmaxs.x, wmaxs.y, wmaxs.z}, {wmins.x, wmaxs.y, wmaxs.z},
        {wmins.x, wmins.y, wmaxs.z}, {wmaxs.x, wmins.y, wmaxs.z}
    };

    for (int i = 0; i < 8; i++) {
        Vector2 screen;
        bool ok = W2S(corners[i], screen);
        if (!ok) ok = W2S_alt(corners[i], screen);
        if (!ok) return false;
        out.x = (std::min)(out.x, screen.x);
        out.y = (std::min)(out.y, screen.y);
        out.w = (std::max)(out.w, screen.x);
        out.h = (std::max)(out.h, screen.y);
    }
    return true;
}

std::vector<EntityEntry> Visuals::GetValidEntities() {
    static std::vector<EntityEntry> cache;
    static int last_frame = 0;
    if (!Config::Visuals::enabled || !Config::Visuals::Player::enabled)
        return cache;

    Init();
    if (!g_ok) return cache;

    g_frame++;
    if (g_frame == last_frame)
        return cache;
    last_frame = g_frame;

    // Only refresh every 5th frame to reduce flicker
    if (g_frame % 5 != 1)
        return cache;

    cache.clear();

    int max = (g_max_idx > 0 && g_max_idx < 1024) ? g_max_idx : 64;

    // First pass: find local controller, its team, and its pawn origin
    uint8_t local_team = 0;
    Vector3 local_origin = { 0, 0, 0 };
    for (int i = 1; i <= max; i++) {
        uintptr_t ctrl = GetEntityFromIndex(i);
        if (!ctrl || !*(bool*)(ctrl + 0x788)) continue;
        local_team = *(uint8_t*)(ctrl + 0x3EB);
        int32_t lph = *(int32_t*)(ctrl + 0x6BC);
        if (lph != -1) {
            uintptr_t lpawn = GetEntityFromIndex(lph & 0x7FFF);
            if (lpawn) {
                uintptr_t scene = *(uintptr_t*)(lpawn + 0x330);
                if (scene) local_origin = *(Vector3*)(scene + 0xC8);
            }
        }
        break;
    }

    // Second pass: collect enemies
    for (int i = 1; i <= max; i++) {
        uintptr_t ctrl = GetEntityFromIndex(i);
        if (!ctrl) continue;
        if (*(bool*)(ctrl + 0x788)) continue;

        uint8_t team = *(uint8_t*)(ctrl + 0x3EB);
        if (team < 2 || team > 3 || team == local_team) continue;

        if (!*(bool*)(ctrl + 0x914)) continue;

        int32_t ph = *(int32_t*)(ctrl + 0x6BC);
        if (ph == -1) continue;

        uintptr_t pawn = GetEntityFromIndex(ph & 0x7FFF);
        if (!pawn || pawn == ctrl) continue;

        int hp = *(int*)(pawn + 0x34C);
        if (hp <= 0 || hp > 100) continue;

        uintptr_t scene = *(uintptr_t*)(pawn + 0x330);
        if (!scene) continue;
        Vector3 origin = *(Vector3*)(scene + 0xC8);
        float dx = origin.x - local_origin.x;
        float dy = origin.y - local_origin.y;
        float dz = origin.z - local_origin.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz) * 0.0254f;

        Rect bbox;
        if (GetBoundingBox(pawn, bbox)) {
            const char* name = (const char*)(ctrl + 0x6F4);
            if (!name[0]) name = "unknown";
            cache.push_back({ bbox, hp, team, name, true, dist });
        }
    }

    g_debug_entities = (int)cache.size();
    return cache;
}

void Visuals::Render() {
    if (!Config::Visuals::enabled || !Config::Visuals::Player::enabled)
        return;

    auto entries = GetValidEntities();
    for (auto& entry : entries)
        RenderPlayerESP(entry);

    if (Config::menu_open) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw) {
            char buf[256];
            sprintf_s(buf, "OK=%d Ent=%d Max=%d\nES=0x%llX VM=0x%llX\nLC=0x%llX",
                g_ok, g_debug_entities, g_max_idx,
                g_entity_system, (uintptr_t)g_vm,
                g_local_ctrl);
            draw->AddText(ImVec2(10, 10), IM_COL32(255, 255, 0, 255), buf);
        }
    }
}

void Visuals::RenderPlayerESP(const EntityEntry& entry) {
    if (entry.health <= 0) return;
    float height = entry.bbox.h - entry.bbox.y;
    if (height < 1.f) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (!draw) return;

    float cx = (entry.bbox.x + entry.bbox.w) * 0.5f;
    float bottom = entry.bbox.h;

    if (Config::Visuals::Player::box) {
        DrawBox(entry.bbox, IM_COL32(255, 255, 255, 220));
    }
    if (Config::Visuals::Player::name && entry.name) {
        float tw = ImGui::CalcTextSize(entry.name).x;
        draw->AddText(ImVec2(cx - tw * 0.5f, entry.bbox.y - 14), IM_COL32(255, 255, 255, 255), entry.name);
    }
    if (Config::Visuals::Player::health) {
        float h = entry.bbox.h - entry.bbox.y;
        float bar_x = entry.bbox.x - 6;
        float bar_y = entry.bbox.y;
        float fill = (entry.health / 100.f) * h;
        draw->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + 4, bar_y + h), IM_COL32(0, 0, 0, 160));
        ImU32 col = entry.health > 50 ? IM_COL32(0, 220, 0, 255) : (entry.health > 25 ? IM_COL32(220, 220, 0, 255) : IM_COL32(220, 0, 0, 255));
        draw->AddRectFilled(ImVec2(bar_x, bar_y + h - fill), ImVec2(bar_x + 4, bar_y + h), col);
    }
    if (Config::Visuals::Player::distance) {
        float d = entry.distance;
        const char* unit_str = "m";
        if (Config::Visuals::Player::unit == 1) { d *= 3.28084f; unit_str = "ft"; }
        else if (Config::Visuals::Player::unit == 2) { d /= 0.0254f; unit_str = "u"; }
        char dbuf[32];
        sprintf_s(dbuf, "%.0f%s", d, unit_str);
        float dw = ImGui::CalcTextSize(dbuf).x;
        draw->AddText(ImVec2(cx - dw * 0.5f, bottom + 2), IM_COL32(200, 200, 200, 255), dbuf);
    }
    if (Config::Visuals::Player::snaplines) {
        draw->AddLine(ImVec2(cx, bottom), ImVec2(g_screen_width * 0.5f, g_screen_height), IM_COL32(255, 255, 255, 160), 1.5f);
    }
}

void Visuals::DrawBox(const Rect& rect, ImColor color) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (!draw) return;
    draw->AddRect(ImVec2(rect.x, rect.y), ImVec2(rect.w, rect.h), color, 0.f, 0, 1.5f);
}
