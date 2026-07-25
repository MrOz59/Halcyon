// Copyright (C) 2022 TiltedPhoques SRL.
// For licensing information see LICENSE at the root of this distribution.

#include "imgui.h"
#include <imgui/ImGuiClipboard_Win32.h>
#include <imgui/ImGuiDriver.h>
#include <imgui/ImguiFont.inl>
#include <imgui/imgui_impl_win32.h>

namespace ImGuiImpl
{
namespace
{
void SetSkyrimImStyle()
{
    auto& style = ImGui::GetStyle();

    // Carvão, ferro e bronze envelhecido: próximos da interface original de
    // Skyrim, mas com contraste suficiente sobre qualquer cena do jogo.
    style.Colors[ImGuiCol_Text] = ImVec4(0.88f, 0.86f, 0.79f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.50f, 0.45f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.025f, 0.027f, 0.028f, 0.965f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.054f, 0.050f, 0.78f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.035f, 0.035f, 0.032f, 0.985f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.54f, 0.46f, 0.30f, 0.72f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.72f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.075f, 0.075f, 0.070f, 0.96f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.18f, 0.12f, 0.96f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.29f, 0.25f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.045f, 0.044f, 0.040f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.13f, 0.085f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.035f, 0.034f, 0.031f, 0.94f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.055f, 0.053f, 0.047f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.025f, 0.025f, 0.023f, 0.92f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.27f, 0.19f, 0.92f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.52f, 0.44f, 0.27f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.70f, 0.59f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.78f, 0.67f, 0.42f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.52f, 0.44f, 0.27f, 0.88f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.78f, 0.67f, 0.42f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.12f, 0.11f, 0.085f, 0.96f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.25f, 0.14f, 0.98f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.43f, 0.35f, 0.19f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.22f, 0.19f, 0.12f, 0.76f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.29f, 0.16f, 0.92f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.48f, 0.39f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.54f, 0.46f, 0.30f, 0.52f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.73f, 0.62f, 0.38f, 0.88f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.86f, 0.73f, 0.44f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.54f, 0.46f, 0.30f, 0.16f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.73f, 0.62f, 0.38f, 0.75f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.86f, 0.73f, 0.44f, 1.00f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.060f, 0.058f, 0.052f, 1.00f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.31f, 0.26f, 0.15f, 0.96f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.23f, 0.19f, 0.11f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.045f, 0.044f, 0.040f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.12f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.12f, 0.105f, 0.075f, 1.00f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.54f, 0.46f, 0.30f, 0.58f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.35f, 0.31f, 0.22f, 0.32f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.035f, 0.034f, 0.031f, 0.52f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.12f, 0.105f, 0.075f, 0.28f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.48f, 0.39f, 0.21f, 0.65f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.86f, 0.73f, 0.44f, 0.95f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.78f, 0.67f, 0.42f, 0.88f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.78f);

    style.WindowPadding = ImVec2(12, 10);
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.FramePadding = ImVec2(8, 5);
    style.FrameRounding = 0.0f;
    style.ItemSpacing = ImVec2(9, 7);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.CellPadding = ImVec2(8, 5);
    style.TouchExtraPadding = ImVec2(0, 0);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 13.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabMinSize = 18.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
    style.ButtonTextAlign = ImVec2(0.50f, 0.50f);
    style.SelectableTextAlign = ImVec2(0.02f, 0.50f);

    style.WindowBorderSize = 1.5f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;
}
} // namespace

ImGuiDriver::ImGuiDriver()
{
    // create imgui
    ImGui::CreateContext();

    auto& io = ImGui::GetIO();

    InstallClipboardHandlers(io);

    // io.IniFilename = nullptr;

    auto& st = ImGui::GetStyle();
#if 0
    st.FrameBorderSize = 1.0f;
    st.FramePadding = ImVec2(4.0f, 2.0f);
    st.ItemSpacing = ImVec2(8.0f, 2.0f);
    st.WindowBorderSize = 1.0f;
    st.TabBorderSize = 1.0f;
#endif

    // make everything have smooth edges
    st.WindowRounding = 2.0f;
    st.ChildRounding = 2.0f;
    st.FrameRounding = 3.0f;
    st.ScrollbarRounding = 3.0f;
    st.GrabRounding = 2.f;
    st.TabRounding = 1.0f;
    SetSkyrimImStyle();
}

ImGuiDriver::~ImGuiDriver()
{
    ImGui::DestroyContext();
}

void ImGuiDriver::Initialize(void* apHandle)
{
    float scaleFactor = ImGui_ImplWin32_GetDpiScaleForHwnd(apHandle);
    // 3260 = 3x
    // 1920 =
    // https://github.com/ocornut/imgui/blob/master/docs/FAQ.md#q-how-should-i-handle-dpi-in-my-application
    auto& io = ImGui::GetIO();

    auto* extraGlyphRanges = io.Fonts->GetGlyphRangesCyrillic(); // Includes Latin
    io.Fonts->AddFontFromMemoryCompressedBase85TTF(
        Roboto_compressed_data_base85,
        20.f * scaleFactor, //->Scale = scaleFactor;
        nullptr, extraGlyphRanges);

    ImGui::GetStyle().ScaleAllSizes(scaleFactor);
}
} // namespace ImGuiImpl
