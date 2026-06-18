#include "menu.h"
#include "config.h"
#include "imgui.h"

void Menu::Render() {
    if (!Config::menu_open) return;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("kdm ware", &Config::menu_open, ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("MainTabs")) {
        if (ImGui::BeginTabItem("Visuals")) {
            RenderVisualsTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void Menu::RenderVisualsTab() {
    if (ImGui::BeginTabBar("VisualsSubTabs")) {
        if (ImGui::BeginTabItem("Player")) {
            RenderPlayerSubTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void Menu::RenderPlayerSubTab() {
    ImGui::Checkbox("Enable ESP", &Config::Visuals::Player::enabled);
    ImGui::Checkbox("Box", &Config::Visuals::Player::box);
    ImGui::Checkbox("Name", &Config::Visuals::Player::name);
    ImGui::Checkbox("Health", &Config::Visuals::Player::health);
    ImGui::Checkbox("Distance", &Config::Visuals::Player::distance);
    ImGui::Checkbox("Snaplines", &Config::Visuals::Player::snaplines);
    if (Config::Visuals::Player::distance) {
        const char* units[] = { "Meters", "Feet", "Game Units" };
        ImGui::Combo("Unit", &Config::Visuals::Player::unit, units, 3);
    }
}
