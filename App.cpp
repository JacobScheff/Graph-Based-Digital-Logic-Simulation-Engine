#include "App.hpp"
#include "IO.hpp"
#include "Pin.hpp"
#include "Net.hpp"
#include "PowerRails.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cmath>

void App::glfwErrorCallback(int err, const char* desc)
{
    fprintf(stderr, "GLFW error %d: %s\n", err, desc);
}

bool App::init()
{
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(1400, 860, "Logic Simulator", nullptr, nullptr);
    if (!window) return false;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.FontGlobalScale = 1.5f;

    applyDarkTheme();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    sim.setTicksPerSecond(simSpeedTPS);
    lastTime = glfwGetTime();
    return true;
}

void App::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

void App::run()
{
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double now = glfwGetTime();
        double dt  = now - lastTime;
        lastTime   = now;

        tick(dt);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderFrame();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.07f, 0.07f, 0.10f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
}

void App::tick(double dt)
{
    if (simRunning)
        sim.update(dt);
}

void App::renderFrame()
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f,0.f});
    ImGui::Begin("##host", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    renderMenuBar();

    ImGuiID dock = ImGui::GetID("##dock");
    ImGui::DockSpace(dock, {0,0}, ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();

    renderPalette();
    renderCanvas();
    renderProperties();
    renderSimControls();

    if (showBusWidthPopup) {
        ImGui::OpenPopup("Bus Width");
        showBusWidthPopup = false;
    }
    if (ImGui::BeginPopupModal("Bus Width", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Select bus width for %s:", pendingBusType.c_str());
        ImGui::Spacing();
        if (ImGui::Button("2-bit", {80, 0})) {
            canvas.beginPlacement(pendingBusType, 2);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("4-bit", {80, 0})) {
            canvas.beginPlacement(pendingBusType, 4);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("8-bit", {80, 0})) {
            canvas.beginPlacement(pendingBusType, 8);
            ImGui::CloseCurrentPopup();
        }
        ImGui::Spacing();
        if (ImGui::Button("Cancel", {80, 0}))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void App::renderMenuBar()
{
    if (!ImGui::BeginMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New")) { /* TODO */ }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Show Demo", nullptr, false, false);
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

void App::renderPalette()
{
    ImGui::SetNextWindowSize({180, 0}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({20, 30}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({200, 600}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Components");

    auto paletteBtn = [&](const char* label, const char* type, const char* tip) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(31, 31, 41, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(49, 46, 129, 255)); // Indigo hover
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(67, 56, 202, 255)); // Active
        if (ImGui::Button(label, {-1.f, 26.f}))
            canvas.beginPlacement(type);
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered() && tip)
            ImGui::SetTooltip("%s", tip);
    };

    auto busPaletteBtn = [&](const char* label, const char* type, const char* tip) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(31, 31, 41, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(49, 46, 129, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(67, 56, 202, 255));
        if (ImGui::Button(label, {-1.f, 26.f})) {
            pendingBusType = type;
            showBusWidthPopup = true;
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered() && tip)
            ImGui::SetTooltip("%s", tip);
    };

    if (ImGui::CollapsingHeader("Logic Gates", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        paletteBtn("NOT",  "NOT",  "Inverter – 1 in, 1 out");
        paletteBtn("BUF",  "BUF",  "Buffer  – 1 in, 1 out");
        paletteBtn("AND",  "AND",  "AND gate");
        paletteBtn("NAND", "NAND", "NAND gate");
        paletteBtn("OR",   "OR",   "OR gate");
        paletteBtn("NOR",  "NOR",  "NOR gate");
        paletteBtn("XOR",  "XOR",  "XOR gate");
        paletteBtn("XNOR", "XNOR", "XNOR gate");
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Inputs / Sources", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        paletteBtn("Switch", "SW",     "Toggle switch (click on canvas)");
        paletteBtn("Button", "BTN",    "Momentary button (click on canvas)");
        paletteBtn("Clock",  "CLK",    "Oscillator – freq set in Properties");
        paletteBtn("Num In", "NUM_IN", "4-bit numeric input (click to cycle)");
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Wiring / Bus", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        busPaletteBtn("Bus Merge", "BUS_MERGE", "N x 1-bit → N-bit bus");
        busPaletteBtn("Bus Split", "BUS_SPLIT", "N-bit bus → N x 1-bit");
        paletteBtn("Junction", "JUNCTION", "Wire pass-through node");
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Outputs", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        paletteBtn("LED",      "LED",      "Single-bit LED indicator");
        paletteBtn("Num Disp", "NUM_DISP", "4-bit numeric display (0–15)");
        ImGui::Spacing();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Shift+Click = Keep placing");
    ImGui::TextDisabled("R-Click Wire = Junction");
    ImGui::TextDisabled("Drag Canvas = Pan");
    ImGui::TextDisabled("Scroll Canvas = Zoom");

    ImGui::End();
}

void App::renderCanvas()
{
    ImGui::SetNextWindowPos({250, 120}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({800, 600}, ImGuiCond_FirstUseEver);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
    ImGui::Begin("Canvas");
    ImGui::PopStyleVar();
    canvas.render();
    ImGui::End();
}

void App::renderProperties()
{
    ImGui::SetNextWindowSize({220, 0}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({1080, 30}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({250, 600}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Properties");

    Component*         comp = canvas.getSelectedComponent();
    const std::string& type = canvas.getSelectedTypeName();
    bool               hasSel = canvas.hasSelection();
 
    if (hasSel) {
        ImGui::SeparatorText("Selection");
        int compCount = canvas.getSelectedComponentCount();
        int wireCount = canvas.getSelectedWireCount();
        int juncCount = canvas.getSelectedJunctionCount();
        
        if (compCount > 0) ImGui::Text("Components: %d", compCount);
        if (wireCount > 0) ImGui::Text("Wires: %d", wireCount);
        if (juncCount > 0) ImGui::Text("Junctions: %d", juncCount);
        
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(239, 68, 68, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(220, 38, 38, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(185, 28, 28, 255));
        if (ImGui::Button("Delete Selected", {-1.f, 32.f})) {
            canvas.deleteSelected();
            canvas.settle();
            comp = nullptr;
            hasSel = false;
        }
        ImGui::PopStyleColor(3);
        if (hasSel) {
            if (ImGui::Button("Clear Selection", {-1.f, 26.f})) {
                canvas.clearSelection();
                comp = nullptr;
                hasSel = false;
            }
        }
        ImGui::Spacing();
    }
 
    if (!comp) {
        if (!hasSel) {
            ImGui::TextDisabled("(nothing selected)");
            ImGui::Spacing();
            ImGui::SeparatorText("Power Rails");
            ImGui::TextColored({0.3f,0.69f,0.31f,1.f}, "VDD: %s",
                stateLabel(sim.getVddNet()->getState(), sim));
            ImGui::TextColored({0.13f,0.59f,0.95f,1.f}, "GND: %s",
                stateLabel(sim.getGndNet()->getState(), sim));
        }
        ImGui::End();
        return;
    }

    ImGui::Text("Type: %s", type.c_str());
    ImGui::Separator();

    if (type == "SW") {
        auto* sw = static_cast<Switch*>(comp);
        bool  on = sw->isOn();
        ImGui::Text("Output: %s", on ? "VDD" : "GND");
        ImGui::PushStyleColor(ImGuiCol_Button,
            on ? IM_COL32(76,175,80,200) : IM_COL32(33,150,243,200));
        if (ImGui::Button(on ? "Set GND##sw" : "Set VDD##sw", {-1.f, 40.f})) {
            sw->toggle();
            canvas.settle();
        }
        ImGui::PopStyleColor();
    }

    if (type == "BTN") {
        auto* btn = static_cast<Button*>(comp);
        ImGui::Text("Hold to press:");
        ImGui::PushStyleColor(ImGuiCol_Button,
            btn->isPressed() ? IM_COL32(76,175,80,200) : IM_COL32(60,60,80,255));
        bool held = ImGui::Button("PRESS##btn", {-1.f, 50.f});
        if (held && !btn->isPressed()) { btn->press();   canvas.settle(); }
        if (!held && btn->isPressed()) { btn->release(); canvas.settle(); }
        ImGui::PopStyleColor();
    }

    if (type == "CLK") {
        auto* clk     = static_cast<Clock*>(comp);
        int   hp      = clk->getHalfPeriod();
        float freqHz  = (hp > 0) ? float(simSpeedTPS) / float(hp * 2) : 0.f;

        ImGui::Text("Half-period: %d ticks", hp);
        ImGui::Text("Frequency:  %.1f Hz", freqHz);
        ImGui::Spacing();
        if (ImGui::SliderInt("Half-period##clk", &hp, 1, 128)) {
            clk->setHalfPeriod(hp);
        }
        ImGui::TextDisabled("(at %.0f ticks/s)", simSpeedTPS);
    }

    if (type == "NUM_IN") {
        auto* ni = static_cast<NumericInput*>(comp);
        int   v  = ni->getValue();
        if (ImGui::SliderInt("Value##ni", &v, 0, 15)) {
            ni->setValue(v);
            canvas.settle();
        }
        ImGui::Text("Binary: %d%d%d%d",
            (v>>3)&1, (v>>2)&1, (v>>1)&1, v&1);
        ImGui::TextDisabled("Click on canvas to cycle");
    }

    if (type == "NUM_DISP") {
        auto* nd = static_cast<NumericDisplay*>(comp);
        ImGui::Text("Value: %d", nd->getValue());
        if (nd->hasAmbiguity())
            ImGui::TextColored({1.f,0.4f,0.4f,1.f}, "  (ambiguous input)");
    }

    if (type == "LED") {
        auto* led = static_cast<LED*>(comp);
        ImGui::TextColored(led->isLit() ? ImVec4{0.3f,0.69f,0.31f,1.f} : ImVec4{0.13f,0.59f,0.95f,1.f},
            "State: %s", stateLabel(led->getLitState(), sim));
    }

    if (type == "BUS_MERGE" || type == "BUS_SPLIT") {
        int bw = comp->getBusWidth();
        ImGui::Text("Bus width: %d bits", bw);
        ImGui::TextDisabled("(delete and re-place to change width)");
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Pin States");
    ImVec4 sColors[] = {
        {0.13f,0.59f,0.95f,1.f},
        {0.30f,0.69f,0.31f,1.f},
        {0.47f,0.47f,0.47f,1.f},
        {0.96f,0.26f,0.21f,1.f},
    };
    for (int i = 0; i < comp->numReceivers(); ++i) {
        State s = comp->getReceiver(i)->getState();
        const char* lbl = stateLabel(s, sim);
        int ci = (s == State::FLOATING) ? 2 : (s == State::UNDEFINED) ? 3 :
                 readAsTrue(s, sim) ? 1 : 0;
        ImGui::TextColored(sColors[ci], "  IN[%d]  %s", i, lbl);
    }
    for (int i = 0; i < comp->numDrivers(); ++i) {
        State s = comp->getDriver(i)->getState();
        const char* lbl = stateLabel(s, sim);
        int ci = (s == State::FLOATING) ? 2 : (s == State::UNDEFINED) ? 3 :
                 readAsTrue(s, sim) ? 1 : 0;
        ImGui::TextColored(sColors[ci], "  OUT[%d] %s", i, lbl);
    }

    ImGui::End();
}

void App::renderSimControls()
{
    ImGui::SetNextWindowPos({250, 30}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({800, 80}, ImGuiCond_FirstUseEver);

    ImGui::Begin("Simulation");

    ImGui::PushStyleColor(ImGuiCol_Button,
        simRunning ? IM_COL32(200,60,60,255) : IM_COL32(60,180,60,255));
    if (ImGui::Button(simRunning ? "  Pause  " : "  Play   ")) {
        simRunning = !simRunning;
        if (simRunning) sim.start();
        else            sim.stop();
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        sim.step(1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        simRunning = false;
        sim.reset();
    }

    ImGui::SameLine(0, 20);
    ImGui::SetNextItemWidth(160);
    if (ImGui::SliderFloat("Ticks/s", &simSpeedTPS, 1.f, 100000.f,
                           "%.0f", ImGuiSliderFlags_Logarithmic)) {
        sim.setTicksPerSecond(simSpeedTPS);
    }

    ImGui::SameLine(0, 20);
    ImGui::Text("Tick: %llu", (unsigned long long)sim.getWheel().getCurrentTick());

    ImGui::SameLine(0, 20);
    ImGui::TextColored({0.30f,0.69f,0.31f,1.f}, "VDD");
    ImGui::SameLine(); ImGui::TextColored({0.13f,0.59f,0.95f,1.f}, "GND");
    ImGui::SameLine(); ImGui::TextColored({0.47f,0.47f,0.47f,1.f}, "FLOAT");
    ImGui::SameLine(); ImGui::TextColored({0.96f,0.26f,0.21f,1.f}, "UNDEF");

    ImGui::End();
}

void App::applyDarkTheme()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 8.f;
    s.FrameRounding     = 4.f;
    s.GrabRounding      = 4.f;
    s.TabRounding       = 4.f;
    s.WindowBorderSize  = 1.f;
    s.FrameBorderSize   = 0.f;
    s.WindowPadding     = {12.f, 12.f};
    s.FramePadding      = { 6.f,  5.f};
    s.ItemSpacing       = { 8.f,  6.f};

    auto& c = s.Colors;
    c[ImGuiCol_WindowBg]          = ImVec4(0.08f, 0.08f, 0.10f, 1.f); // deep dark navy-black #0D0D10
    c[ImGuiCol_ChildBg]           = ImVec4(0.06f, 0.06f, 0.08f, 1.f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.10f, 0.10f, 0.14f, 1.f);
    c[ImGuiCol_Border]            = ImVec4(0.18f, 0.18f, 0.24f, 0.8f); // sleek borders #2E2E3E
    c[ImGuiCol_FrameBg]           = ImVec4(0.12f, 0.12f, 0.17f, 1.f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.18f, 0.18f, 0.25f, 1.f);
    c[ImGuiCol_TitleBg]           = ImVec4(0.06f, 0.06f, 0.08f, 1.f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.08f, 0.08f, 0.10f, 1.f);
    c[ImGuiCol_Header]            = ImVec4(0.15f, 0.15f, 0.25f, 1.f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.25f, 0.25f, 0.45f, 1.f);
    c[ImGuiCol_Button]            = ImVec4(0.15f, 0.15f, 0.22f, 1.f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.24f, 0.23f, 0.53f, 1.f); // Indigo hover #3E3B87
    c[ImGuiCol_ButtonActive]      = ImVec4(0.31f, 0.29f, 0.71f, 1.f); // Indigo active #4F4AB5
    c[ImGuiCol_Tab]               = ImVec4(0.08f, 0.08f, 0.12f, 1.f);
    c[ImGuiCol_TabHovered]        = ImVec4(0.24f, 0.23f, 0.53f, 1.f);
    c[ImGuiCol_TabActive]         = ImVec4(0.18f, 0.18f, 0.30f, 1.f);
    c[ImGuiCol_SliderGrab]        = ImVec4(0.38f, 0.36f, 0.85f, 1.f); // Indigo accent grab #615CD9
    c[ImGuiCol_SliderGrabActive]  = ImVec4(0.48f, 0.45f, 0.95f, 1.f);
    c[ImGuiCol_CheckMark]         = ImVec4(0.38f, 0.36f, 0.85f, 1.f);
    c[ImGuiCol_SeparatorHovered]  = ImVec4(0.38f, 0.36f, 0.85f, 1.f);
    c[ImGuiCol_SeparatorActive]   = ImVec4(0.48f, 0.45f, 0.95f, 1.f);
    c[ImGuiCol_DockingPreview]    = ImVec4(0.38f, 0.36f, 0.85f, 0.6f);
}
