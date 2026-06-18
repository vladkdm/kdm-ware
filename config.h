#pragma once

namespace Config {
    inline bool menu_open = true;

    namespace Visuals {
        inline bool enabled = true;

        namespace Player {
            inline bool enabled = true;
            inline bool box = true;
            inline bool health = true;
            inline bool name = true;
            inline bool snaplines = false;
            inline bool weapon = true;
            inline bool distance = false;
            inline bool skeleton = false;
            inline bool glow = false;
            inline bool chams = false;
            inline int unit = 0; // 0=meters 1=feet 2=units
            inline float box_color[4] = { 1.f, 1.f, 1.f, 1.f };
            inline float visible_color[4] = { 0.f, 1.f, 0.f, 1.f };
            inline float snapline_color[4] = { 1.f, 1.f, 1.f, 1.f };
        }
    }
}
