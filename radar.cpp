#include "radar.h"
#include "config.h"
#include "imgui.h"
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <cmath>
#pragma comment(lib, "psapi.lib")

extern int g_screen_width;
extern int g_screen_height;

static uintptr_t g_client = 0;
static uintptr_t g_entity_system = 0;
static int g_max_idx = 0;
static bool g_inited = false;

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

    g_inited = true;
}

std::vector<RadarEntity> Radar::CollectEntities() {
    std::vector<RadarEntity> result;
    Init();
    if (!g_entity_system) return result;

    int max = (g_max_idx > 0 && g_max_idx < 1024) ? g_max_idx : 64;

    // First pass: find local controller, team, and pawn position
    uint8_t local_team = 0;
    Vector3 local_origin = { 0, 0, 0 };
    uintptr_t local_pawn = 0;
    for (int i = 1; i <= max; i++) {
        uintptr_t ctrl = GetEntityFromIndex(i);
        if (!ctrl || !*(bool*)(ctrl + 0x788)) continue;
        local_team = *(uint8_t*)(ctrl + 0x3EB);
        int32_t lph = *(int32_t*)(ctrl + 0x6BC);
        if (lph != -1) {
            local_pawn = GetEntityFromIndex(lph & 0x7FFF);
            if (local_pawn) {
                uintptr_t scene = *(uintptr_t*)(local_pawn + 0x330);
                if (scene) local_origin = *(Vector3*)(scene + 0xC8);
            }
        }
        break;
    }

    // Collect known pawn indices (local + enemies) to exclude them from weapon scan
    bool known_pawns[1024] = { false };
    if (local_pawn) {
        for (int i = 1; i <= max; i++)
            if (GetEntityFromIndex(i) == local_pawn) { known_pawns[i] = true; break; }
    }

    // Second pass: enemy controllers
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

        uintptr_t scene = *(uintptr_t*)(pawn + 0x330);
        if (!scene) continue;
        Vector3 origin = *(Vector3*)(scene + 0xC8);

        known_pawns[ph & 0x7FFF] = true;
        result.push_back({ origin, RadarEntType::Enemy });
    }

    // Third pass: weapons (entities with scene position but NOT known pawns)
    if (Config::Misc::Radar::show_weapons) {
        for (int i = 1; i <= max; i++) {
            if (known_pawns[i]) continue;
            uintptr_t ent = GetEntityFromIndex(i);
            if (!ent || ent == local_pawn) continue;

            uintptr_t scene = *(uintptr_t*)(ent + 0x330);
            if (!scene) continue;
            Vector3 pos = *(Vector3*)(scene + 0xC8);

            float dx = pos.x - local_origin.x;
            float dy = pos.y - local_origin.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist > 5000.f) continue;

            result.push_back({ pos, RadarEntType::Weapon });
        }
    }

    return result;
}

void Radar::Render() {
    if (!Config::Misc::Radar::enable) return;

    auto entities = CollectEntities();

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (!draw) return;

    float size = Config::Misc::Radar::size;
    float alpha = Config::Misc::Radar::alpha;
    float scale = Config::Misc::Radar::scale;

    float cx = size + 30.f;
    float cy = (float)g_screen_height - size - 30.f;

    int a_bg = (int)(80 * alpha);
    int a_border = (int)(120 * alpha);

    draw->AddCircleFilled(ImVec2(cx, cy), size, IM_COL32(0, 0, 0, a_bg), 64);
    draw->AddCircle(ImVec2(cx, cy), size, IM_COL32(255, 255, 255, a_border), 64, 1.0f);

    // Cross lines
    int a_cross = (int)(50 * alpha);
    draw->AddLine(ImVec2(cx - size, cy), ImVec2(cx + size, cy), IM_COL32(255, 255, 255, a_cross));
    draw->AddLine(ImVec2(cx, cy - size), ImVec2(cx, cy + size), IM_COL32(255, 255, 255, a_cross));

    if (entities.empty()) {
        draw->AddCircleFilled(ImVec2(cx, cy), 3, IM_COL32(0, 255, 255, 255));
        return;
    }

    Vector3 local_origin = { 0, 0, 0 };
    for (auto& e : entities) {
        if (e.type == RadarEntType::Enemy) {
            local_origin = e.origin;
            break;
        }
    }

    // If no local origin found, find it from the raw entity list
    if (local_origin.x == 0 && local_origin.y == 0 && local_origin.z == 0) {
        int max = (g_max_idx > 0 && g_max_idx < 1024) ? g_max_idx : 64;
        for (int i = 1; i <= max; i++) {
            uintptr_t ctrl = GetEntityFromIndex(i);
            if (!ctrl || !*(bool*)(ctrl + 0x788)) continue;
            int32_t lph = *(int32_t*)(ctrl + 0x6BC);
            if (lph == -1) continue;
            uintptr_t pawn = GetEntityFromIndex(lph & 0x7FFF);
            if (!pawn) continue;
            uintptr_t scene = *(uintptr_t*)(pawn + 0x330);
            if (!scene) continue;
            local_origin = *(Vector3*)(scene + 0xC8);
            break;
        }
    }

    float world_scale = (size / 3000.f) * scale;

    for (auto& e : entities) {
        float dx = (e.origin.x - local_origin.x) * world_scale;
        float dy = -(e.origin.y - local_origin.y) * world_scale;

        float dist = sqrtf(dx * dx + dy * dy);
        bool clamped = false;
        if (dist > size - 3.f) {
            dx = dx / dist * (size - 3.f);
            dy = dy / dist * (size - 3.f);
            clamped = true;
        }

        ImVec2 pos(cx + dx, cy + dy);
        int a_ent = (int)(255 * alpha);

        if (e.type == RadarEntType::Enemy && Config::Misc::Radar::show_enemies) {
            draw->AddCircleFilled(pos, 4, IM_COL32(255, 0, 0, a_ent));

            if (Config::Misc::Radar::show_line_of_sight) {
                int a_los = (int)(120 * alpha);
                draw->AddLine(ImVec2(cx, cy), pos, IM_COL32(0, 255, 0, a_los), 1.0f);
            }
        } else if (e.type == RadarEntType::Weapon && Config::Misc::Radar::show_weapons) {
            draw->AddCircleFilled(pos, 2.5f, IM_COL32(255, 255, 0, a_ent));
        }
    }

    draw->AddCircleFilled(ImVec2(cx, cy), 3, IM_COL32(0, 255, 255, 255));
}
