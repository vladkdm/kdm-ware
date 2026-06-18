@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul

set IMGUI=C:\Users\Vladk\OneDrive\Desktop\kdm ware\cs2-internal\dependencies\imgui
set MINHOOK=C:\Users\Vladk\OneDrive\Desktop\kdm ware\cs2-internal\dependencies\minhook
set SRC=C:\Users\Vladk\OneDrive\Desktop\kdm ware\cs2-internal
set OUT=%SRC%\build

if not exist "%OUT%" mkdir "%OUT%"

cl /MT /O2 /GS- /EHsc /std:c++20 /I"%IMGUI%" /I"%IMGUI%\backends" /I"%MINHOOK%\include" /c ^
    "%SRC%\main.cpp" "%SRC%\menu.cpp" "%SRC%\visuals.cpp" "%SRC%\memory.cpp" "%SRC%\radar.cpp" ^
    "%IMGUI%\imgui.cpp" "%IMGUI%\imgui_draw.cpp" "%IMGUI%\imgui_widgets.cpp" "%IMGUI%\imgui_tables.cpp" ^
    "%IMGUI%\backends\imgui_impl_win32.cpp" "%IMGUI%\backends\imgui_impl_dx11.cpp" ^
    "%MINHOOK%\src\hook.c" "%MINHOOK%\src\buffer.c" "%MINHOOK%\src\trampoline.c" "%MINHOOK%\src\hde\hde64.c" ^
    /Fo"%OUT%/"

link /DLL /OUT:"%OUT%\cs2-internal.dll" /SUBSYSTEM:WINDOWS /NOLOGO ^
    "%OUT%\main.obj" "%OUT%\menu.obj" "%OUT%\visuals.obj" "%OUT%\memory.obj" "%OUT%\radar.obj" ^
    "%OUT%\imgui.obj" "%OUT%\imgui_draw.obj" "%OUT%\imgui_widgets.obj" "%OUT%\imgui_tables.obj" ^
    "%OUT%\imgui_impl_win32.obj" "%OUT%\imgui_impl_dx11.obj" ^
    "%OUT%\hook.obj" "%OUT%\buffer.obj" "%OUT%\trampoline.obj" "%OUT%\hde64.obj" ^
    d3d11.lib dxgi.lib

echo Build complete!
dir "%OUT%\cs2-internal.dll"
