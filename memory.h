#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

namespace Memory {
    uintptr_t FindPattern(const std::string& module_name, const std::string& pattern);
    uintptr_t FindPatternRaw(const std::string& module_name, const uint8_t* pattern, const char* mask);
    void* HookFunction(void* target, void* detour);
    bool UnhookFunction(void* target, void* trampoline);
    uintptr_t ResolveRip(uintptr_t address, int32_t offset = 3, int32_t instruction_size = 7);

    template<typename T>
    T Read(uintptr_t address) {
        return *reinterpret_cast<T*>(address);
    }

    template<typename T>
    void ReadArray(uintptr_t address, T* buffer, size_t count) {
        for (size_t i = 0; i < count; i++)
            buffer[i] = Read<T>(address + i * sizeof(T));
    }

    inline HMODULE GetModule(const std::string& name) {
        HMODULE hMod = nullptr;
        while (!hMod) {
            hMod = GetModuleHandleA(name.c_str());
            Sleep(100);
        }
        return hMod;
    }
}
