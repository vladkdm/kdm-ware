#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include "MinHook.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "menu.h"
#include "visuals.h"
#include "radar.h"
#include "config.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static IDXGISwapChain* g_swapchain = nullptr;
static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static ID3D11RenderTargetView* g_render_target = nullptr;
static HWND g_window = nullptr;
static bool g_init = false;
static WNDPROC original_wndproc = nullptr;

int g_screen_width = 1920;
int g_screen_height = 1080;

typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain*, UINT, UINT);
static PresentFn original_present = nullptr;

static void ShutdownImGui();
static void CreateRenderTarget() {
    ID3D11Texture2D* backbuffer = nullptr;
    if (g_swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)) == S_OK) {
        g_device->CreateRenderTargetView(backbuffer, nullptr, &g_render_target);
        backbuffer->Release();
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYUP && wParam == VK_INSERT) {
        Config::menu_open = !Config::menu_open;
    }

    if (g_init && Config::menu_open &&
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    return CallWindowProcA(original_wndproc, hwnd, msg, wParam, lParam);
}

static HRESULT __stdcall Present_hook(IDXGISwapChain* swapchain, UINT sync, UINT flags) {
    if (!g_init) {
        g_swapchain = swapchain;
        if (swapchain->GetDevice(IID_PPV_ARGS(&g_device)) == S_OK) {
            g_device->GetImmediateContext(&g_context);

            DXGI_SWAP_CHAIN_DESC desc;
            swapchain->GetDesc(&desc);
            g_window = desc.OutputWindow;
            g_screen_width = desc.BufferDesc.Width;
            g_screen_height = desc.BufferDesc.Height;

            original_wndproc = (WNDPROC)SetWindowLongPtrA(
                g_window, GWLP_WNDPROC, (LONG_PTR)WndProc);

            LoadLibraryA("d3dcompiler_47.dll");

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;

            io.Fonts->AddFontFromFileTTF(
                "C:\\Users\\Vladk\\OneDrive\\Desktop\\kdm ware\\cs2-internal\\dependencies\\Inter\\extras\\ttf\\Inter-Regular.ttf",
                16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(g_window);
            ImGui_ImplDX11_Init(g_device, g_context);

            CreateRenderTarget();
            g_init = true;
        }
    }

    if (g_init && (Config::menu_open || Config::Visuals::enabled)) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (Config::menu_open)
            Menu::Render();

        Visuals visuals;
        visuals.Render();

        Radar radar;
        radar.Render();

        ImGui::EndFrame();
        ImGui::Render();

        g_context->OMSetRenderTargets(1, &g_render_target, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return original_present(swapchain, sync, flags);
}

static void ShutdownImGui() {
    g_init = false;
    if (original_wndproc && g_window)
        SetWindowLongPtrA(g_window, GWLP_WNDPROC, (LONG_PTR)original_wndproc);
    if (g_render_target) { g_render_target->Release(); g_render_target = nullptr; }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
    g_swapchain = nullptr;
    g_window = nullptr;
}

static void* FindPresentViaD3D11() {
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA) };
    wc.lpfnWndProc = DefWindowProcA;
    wc.lpszClassName = "Tmp";
    RegisterClassExA(&wc);
    HWND tmp = CreateWindowExA(0, "Tmp", "", WS_POPUP, 0, 0, 1, 1, 0, 0, 0, 0);
    if (!tmp) { UnregisterClassA("Tmp", nullptr); return nullptr; }

    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferCount = 1;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.Width = 1;
    desc.BufferDesc.Height = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = tmp;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;

    ID3D11Device* dev = nullptr;
    IDXGISwapChain* sc = nullptr;
    ID3D11DeviceContext* ctx = nullptr;

    void* present = nullptr;
    if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &desc, &sc, &dev, nullptr, &ctx))) {
        present = (*(void***)sc)[8];
    }

    if (sc) sc->Release();
    if (ctx) ctx->Release();
    if (dev) dev->Release();
    DestroyWindow(tmp);
    UnregisterClassA("Tmp", nullptr);
    return present;
}

static bool InitHooks() {
    if (MH_Initialize() != MH_OK)
        return false;

    void* present_addr = FindPresentViaD3D11();
    if (!present_addr)
        return false;

    if (MH_CreateHook(present_addr, Present_hook,
        (void**)&original_present) != MH_OK)
        return false;

    if (MH_EnableHook(present_addr) != MH_OK)
        return false;

    return true;
}

static void ShutdownHooks() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

DWORD WINAPI MainThread(LPVOID) {
    while (!GetModuleHandleA("client.dll"))
        Sleep(100);

    if (!InitHooks()) {
        MessageBoxA(0, "Failed to init hooks", "Error", MB_ICONERROR);
        return 0;
    }

    while (!(GetAsyncKeyState(VK_END) & 1))
        Sleep(50);

    if (g_init) ShutdownImGui();
    ShutdownHooks();

    FreeLibraryAndExitThread((HMODULE)GetModuleHandleA(nullptr), 0);
    return 0;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        HANDLE h = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
