#include "memory.h"
#include <psapi.h>
#include <algorithm>

#pragma comment(lib, "psapi.lib")

namespace Memory {
    uintptr_t FindPattern(const std::string& module_name, const std::string& pattern) {
        std::vector<uint8_t> bytes;
        std::string mask;

        size_t i = 0;
        while (i < pattern.size()) {
            if (pattern[i] == ' ') { i++; continue; }
            if (pattern[i] == '?') {
                bytes.push_back(0);
                mask += '?';
                i++;
                if (i < pattern.size() && pattern[i] == '?') i++;
            }
            else {
                uint8_t byte = (uint8_t)strtol(pattern.substr(i, 2).c_str(), nullptr, 16);
                bytes.push_back(byte);
                mask += 'x';
                i += 2;
            }
        }

        return FindPatternRaw(module_name, bytes.data(), mask.c_str());
    }

    uintptr_t FindPatternRaw(const std::string& module_name, const uint8_t* pattern, const char* mask) {
        HMODULE mod = GetModuleHandleA(module_name.c_str());
        if (!mod) return 0;

        MODULEINFO info;
        if (!GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info)))
            return 0;

        uintptr_t start = (uintptr_t)mod;
        size_t size = info.SizeOfImage;
        size_t pattern_len = strlen(mask);

        for (size_t i = 0; i < size - pattern_len; i++) {
            bool found = true;
            for (size_t j = 0; j < pattern_len; j++) {
                if (mask[j] == 'x' && pattern[j] != *(uint8_t*)(start + i + j)) {
                    found = false;
                    break;
                }
            }
            if (found)
                return start + i;
        }
        return 0;
    }

    uintptr_t ResolveRip(uintptr_t address, int32_t offset, int32_t instruction_size) {
        int32_t rel = *(int32_t*)(address + offset);
        return address + instruction_size + rel;
    }

    void* HookFunction(void* target, void* detour) {
        if (!target || !detour) return nullptr;

        uint8_t trampoline[14] = { 0 };
        memcpy(trampoline, target, 14);

        uint8_t jmp[14] = { 0 };
        jmp[0] = 0x48;
        jmp[1] = 0xB8;
        *(uintptr_t*)&jmp[2] = (uintptr_t)detour;
        jmp[10] = 0xFF;
        jmp[11] = 0xE0;

        DWORD old;
        VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &old);
        memcpy(target, jmp, 14);
        VirtualProtect(target, 14, old, &old);

        void* trampoline_ptr = VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (trampoline_ptr) {
            memcpy(trampoline_ptr, trampoline, 14);
            uint8_t ret_jmp[14] = { 0 };
            ret_jmp[0] = 0x48;
            ret_jmp[1] = 0xB8;
            *(uintptr_t*)&ret_jmp[2] = (uintptr_t)target + 14;
            ret_jmp[10] = 0xFF;
            ret_jmp[11] = 0xE0;
            memcpy((uint8_t*)trampoline_ptr + 14, ret_jmp, 14);
        }

        return trampoline_ptr;
    }

    bool UnhookFunction(void* target, void* trampoline) {
        if (!target || !trampoline) return false;

        DWORD old;
        VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &old);
        memcpy(target, trampoline, 14);
        VirtualProtect(target, 14, old, &old);

        return true;
    }
}
