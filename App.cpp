#include "App.hpp"
#include "IO.hpp"
#include "Pin.hpp"
#include "Net.hpp"
#include "PowerRails.hpp"

#include "imgui.h"
#include "imgui_internal.h"
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

    io.FontGlobalScale = 1.3f;

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
        glClearColor(0.06f, 0.06f, 0.08f, 1.f);
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

// ─── Programmatic Dock Layout ─────────────────────────────────────────────────

void App::buildDefaultLayout(ImGuiID dockId)
{
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->WorkSize);

    // Split: Left (palette) | Rest
    ImGuiID dockLeft, dockRest;
    ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.16f, &dockLeft, &dockRest);

    // Split Rest: Center | Right (properties)
    ImGuiID dockCenter, dockRight;
    ImGui::DockBuilderSplitNode(dockRest, ImGuiDir_Right, 0.20f, &dockRight, &dockCenter);

    // Split Center: Top (sim controls) | Bottom (canvas)
    ImGuiID dockTop, dockBottom;
    ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Up, 0.07f, &dockTop, &dockBottom);

    ImGui::DockBuilderDockWindow("Components", dockLeft);
    ImGui::DockBuilderDockWindow("Simulation", dockTop);
    ImGui::DockBuilderDockWindow("Canvas",     dockBottom);
    ImGui::DockBuilderDockWindow("Properties", dockRight);

    // Make simulation controls bar non-resizable height
    ImGuiDockNode* topNode = ImGui::DockBuilderGetNode(dockTop);
    if (topNode) {
        topNode->LocalFlags |= ImGuiDockNodeFlags_NoResize;
    }

    ImGui::DockBuilderFinish(dockId);
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

    // Build programmatic layout on first frame BEFORE DockSpace
    if (!layoutInitialized) {
        buildDefaultLayout(dock);
        layoutInitialized = true;
    }

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
        auto busBtn = [&](const char* label, int width) {
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(36, 36, 50, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(55, 52, 140, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(75, 65, 210, 255));
            if (ImGui::Button(label, {88, 32})) {
                canvas.beginPlacement(pendingBusType, width);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
        };
        busBtn("2-bit", 2);
        ImGui::SameLine();
        busBtn("4-bit", 4);
        ImGui::SameLine();
        busBtn("8-bit", 8);
        ImGui::Spacing();
        if (ImGui::Button("Cancel", {88, 28}))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ─── Menu Bar ─────────────────────────────────────────────────────────────────

void App::renderMenuBar()
{
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New"))  { /* TODO */ }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) { glfwSetWindowShouldClose(window, GLFW_TRUE); }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Select All",      "Ctrl+A")) {
            // Delegate to canvas — handled via keyboard in Canvas::render
        }
        if (ImGui::MenuItem("Clear Selection", "Esc",    false, canvas.hasSelection())) {
            canvas.clearSelection();
        }
        if (ImGui::MenuItem("Delete Selected", "Del",    false, canvas.hasSelection())) {
            canvas.deleteSelected();
            canvas.settle();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Show Demo", nullptr, false, false);
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

// ─── Component Palette ────────────────────────────────────────────────────────

void App::renderPalette()
{
    ImGui::Begin("Components");

    auto paletteBtn = [&](const char* label, const char* type, const char* tip) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(28, 28, 38, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(50, 48, 120, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(70, 60, 195, 255));
        if (ImGui::Button(label, {-1.f, 30.f}))
            canvas.beginPlacement(type);
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered() && tip)
            ImGui::SetTooltip("%s", tip);
    };

    auto busPaletteBtn = [&](const char* label, const char* type, const char* tip) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(28, 28, 38, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(50, 48, 120, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(70, 60, 195, 255));
        if (ImGui::Button(label, {-1.f, 30.f})) {
            pendingBusType = type;
            showBusWidthPopup = true;
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered() && tip)
            ImGui::SetTooltip("%s", tip);
    };

    if (ImGui::CollapsingHeader("Logic Gates", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(4.f);
        ImGui::Spacing();
        paletteBtn("NOT",  "NOT",  "Inverter \xe2\x80\x93 1 in, 1 out");
        paletteBtn("BUF",  "BUF",  "Buffer \xe2\x80\x93 1 in, 1 out");
        paletteBtn("AND",  "AND",  "AND gate \xe2\x80\x93 2 in, 1 out");
        paletteBtn("NAND", "NAND", "NAND gate \xe2\x80\x93 2 in, 1 out");
        paletteBtn("OR",   "OR",   "OR gate \xe2\x80\x93 2 in, 1 out");
        paletteBtn("NOR",  "NOR",  "NOR gate \xe2\x80\x93 2 in, 1 out");
        paletteBtn("XOR",  "XOR",  "XOR gate \xe2\x80\x93 2 in, 1 out");
        paletteBtn("XNOR", "XNOR", "XNOR gate \xe2\x80\x93 2 in, 1 out");
        ImGui::Spacing();
        ImGui::Unindent(4.f);
    }

    if (ImGui::CollapsingHeader("Inputs / Sources", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(4.f);
        ImGui::Spacing();
        paletteBtn("Switch", "SW",     "Toggle switch (click on canvas)");
        paletteBtn("Button", "BTN",    "Momentary button (hold on canvas)");
        paletteBtn("Clock",  "CLK",    "Oscillator \xe2\x80\x93 freq set in Properties");
        paletteBtn("Num In", "NUM_IN", "4-bit numeric input (click/scroll)");
        ImGui::Spacing();
        ImGui::Unindent(4.f);
    }

    if (ImGui::CollapsingHeader("Wiring / Bus", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(4.f);
        ImGui::Spacing();
        busPaletteBtn("Bus Merge", "BUS_MERGE", "N x 1-bit \xe2\x86\x92 N-bit bus");
        busPaletteBtn("Bus Split", "BUS_SPLIT", "N-bit bus \xe2\x86\x92 N x 1-bit");
        paletteBtn("Junction", "JUNCTION", "Wire pass-through node");
        ImGui::Spacing();
        ImGui::Unindent(4.f);
    }

    if (ImGui::CollapsingHeader("Outputs", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(4.f);
        ImGui::Spacing();
        paletteBtn("LED",      "LED",      "Single-bit LED indicator");
        paletteBtn("Num Disp", "NUM_DISP", "4-bit numeric display (0\xe2\x80\x93" "15)");
        ImGui::Spacing();
        ImGui::Unindent(4.f);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Shortcuts")) {
        ImGui::Indent(4.f);
        ImGui::Spacing();
        ImGui::TextDisabled("Shift+Click  Keep placing");
        ImGui::TextDisabled("R-Click Wire  Junction menu");
        ImGui::TextDisabled("R/M-Drag  Pan canvas");
        ImGui::TextDisabled("Scroll  Zoom in/out");
        ImGui::TextDisabled("Del/Bksp  Delete selected");
        ImGui::TextDisabled("Esc  Clear selection");
        ImGui::Spacing();
        ImGui::Unindent(4.f);
    }

    ImGui::End();
}

// ─── Canvas ───────────────────────────────────────────────────────────────────

void App::renderCanvas()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
    ImGui::Begin("Canvas");
    ImGui::PopStyleVar();
    canvas.render();
    ImGui::End();
}

// ─── Properties ───────────────────────────────────────────────────────────────

void App::renderProperties()
{
    ImGui::Begin("Properties");

    Component*         comp = canvas.getSelectedComponent();
    const std::string& type = canvas.getSelectedTypeName();
    bool               hasSel = canvas.hasSelection();

    if (hasSel) {
        ImGui::SeparatorText("Selection");
        int compCount = canvas.getSelectedComponentCount();
        int wireCount = canvas.getSelectedWireCount();
        int juncCount = canvas.getSelectedJunctionCount();

        if (compCount > 0) ImGui::Text("  Components: %d", compCount);
        if (wireCount > 0) ImGui::Text("  Wires: %d", wireCount);
        if (juncCount > 0) ImGui::Text("  Junctions: %d", juncCount);

        ImGui::Spacing();

        // Delete button — red accent
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(180, 50, 50, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  IM_COL32(210, 45, 45, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   IM_COL32(160, 30, 30, 255));
        if (ImGui::Button("Delete Selected", {-1.f, 28.f})) {
            canvas.deleteSelected();
            canvas.settle();
            comp = nullptr;
            hasSel = false;
        }
        ImGui::PopStyleColor(3);

        if (hasSel) {
            ImGui::Spacing();
            // Clear Selection — subtle outline-like button
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(30, 30, 42, 180));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  IM_COL32(50, 48, 70, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,   IM_COL32(40, 40, 58, 255));
            if (ImGui::Button("Clear Selection", {-1.f, 26.f})) {
                canvas.clearSelection();
                comp = nullptr;
                hasSel = false;
            }
            ImGui::PopStyleColor(3);
        }
        ImGui::Spacing();
    }

    if (!comp) {
        if (!hasSel) {
            ImGui::Spacing();
            ImGui::TextDisabled("No selection");
            ImGui::Spacing();
            ImGui::TextDisabled("Click a component on the");
            ImGui::TextDisabled("canvas to inspect it here.");
            ImGui::Spacing();
            ImGui::SeparatorText("Power Rails");
            ImGui::Spacing();
            ImGui::TextColored({0.30f,0.69f,0.31f,1.f}, "  (O) VDD: %s",
                stateLabel(sim.getVddNet()->getState(), sim));
            ImGui::TextColored({0.25f,0.55f,0.90f,1.f}, "  (O) GND: %s",
                stateLabel(sim.getGndNet()->getState(), sim));
        }
        ImGui::End();
        return;
    }

    // Component header
    ImGui::SeparatorText(type.c_str());

    if (type == "SW") {
        auto* sw = static_cast<Switch*>(comp);
        bool  on = sw->isOn();
        ImGui::TextColored(on ? ImVec4{0.30f,0.69f,0.31f,1.f} : ImVec4{0.25f,0.55f,0.90f,1.f},
            "(O) Output: %s", on ? "VDD" : "GND");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button,
            on ? IM_COL32(76,175,80,200) : IM_COL32(33,140,220,200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            on ? IM_COL32(90,195,95,255) : IM_COL32(45,160,240,255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            on ? IM_COL32(60,155,65,255) : IM_COL32(25,120,200,255));
        if (ImGui::Button(on ? "Set GND##sw" : "Set VDD##sw", {-1.f, 36.f})) {
            sw->toggle();
            canvas.settle();
        }
        ImGui::PopStyleColor(3);
    }

    if (type == "BTN") {
        auto* btn = static_cast<Button*>(comp);
        ImGui::Text("Hold to press:");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button,
            btn->isPressed() ? IM_COL32(76,175,80,200) : IM_COL32(50,50,70,255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            btn->isPressed() ? IM_COL32(90,195,95,255) : IM_COL32(65,65,90,255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            btn->isPressed() ? IM_COL32(60,155,65,255) : IM_COL32(80,80,110,255));
        bool held = ImGui::Button("PRESS##btn", {-1.f, 44.f});
        if (held && !btn->isPressed()) { btn->press();   canvas.settle(); }
        if (!held && btn->isPressed()) { btn->release(); canvas.settle(); }
        ImGui::PopStyleColor(3);
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
        ImGui::TextDisabled("Click/scroll on canvas");
    }

    if (type == "NUM_DISP") {
        auto* nd = static_cast<NumericDisplay*>(comp);
        ImGui::Text("Value: %d  (0x%X)", nd->getValue(), nd->getValue());
        if (nd->hasAmbiguity())
            ImGui::TextColored({1.f,0.45f,0.4f,1.f}, "  (ambiguous input)");
    }

    if (type == "LED") {
        auto* led = static_cast<LED*>(comp);
        ImVec4 ledCol = led->isLit() ? ImVec4{0.30f,0.69f,0.31f,1.f}
                                     : ImVec4{0.25f,0.55f,0.90f,1.f};
        ImGui::TextColored(ledCol, "(O) State: %s",
            stateLabel(led->getLitState(), sim));
    }

    if (type == "BUS_MERGE" || type == "BUS_SPLIT") {
        int bw = comp->getBusWidth();
        ImGui::Text("Bus width: %d bits", bw);
        ImGui::TextDisabled("(delete and re-place to change)");
    }

    // ─── Pin States ───────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::SeparatorText("Pin States");

    ImVec4 sColors[] = {
        {0.25f,0.55f,0.90f,1.f},   // LOW  — blue
        {0.30f,0.69f,0.31f,1.f},   // HIGH — green
        {0.45f,0.45f,0.45f,1.f},   // FLOAT — gray
        {0.95f,0.30f,0.25f,1.f},   // UNDEF — red
    };

    if (ImGui::BeginTable("##pins", 3,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Pin",   ImGuiTableColumnFlags_WidthFixed, 50.f);
        ImGui::TableSetupColumn("Dir",   ImGuiTableColumnFlags_WidthFixed, 30.f);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (int i = 0; i < comp->numReceivers(); ++i) {
            State s = comp->getReceiver(i)->getState();
            const char* lbl = stateLabel(s, sim);
            int ci = (s == State::FLOATING) ? 2 : (s == State::UNDEFINED) ? 3 :
                     readAsTrue(s, sim) ? 1 : 0;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char pinBuf[16];
            std::snprintf(pinBuf, sizeof(pinBuf), "[%d]", i);
            ImGui::Text("%s", pinBuf);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("IN");
            ImGui::TableNextColumn();
            ImGui::TextColored(sColors[ci], "%s", lbl);
        }
        for (int i = 0; i < comp->numDrivers(); ++i) {
            State s = comp->getDriver(i)->getState();
            const char* lbl = stateLabel(s, sim);
            int ci = (s == State::FLOATING) ? 2 : (s == State::UNDEFINED) ? 3 :
                     readAsTrue(s, sim) ? 1 : 0;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char pinBuf[16];
            std::snprintf(pinBuf, sizeof(pinBuf), "[%d]", i);
            ImGui::Text("%s", pinBuf);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("OUT");
            ImGui::TableNextColumn();
            ImGui::TextColored(sColors[ci], "%s", lbl);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

// ─── Simulation Controls ──────────────────────────────────────────────────────

void App::renderSimControls()
{
    ImGui::Begin("Simulation", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Play/Pause
    ImGui::PushStyleColor(ImGuiCol_Button,
        simRunning ? IM_COL32(185, 55, 55, 255) : IM_COL32(55, 170, 55, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        simRunning ? IM_COL32(210, 65, 65, 255) : IM_COL32(70, 195, 70, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        simRunning ? IM_COL32(160, 45, 45, 255) : IM_COL32(45, 145, 45, 255));
    if (ImGui::Button(simRunning ? "Pause" : "Play", {48.f, 28.f})) {
        simRunning = !simRunning;
        if (simRunning) sim.start();
        else            sim.stop();
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(simRunning ? "Pause simulation" : "Start simulation");

    // Step
    ImGui::SameLine(0, 4);
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(40, 40, 56, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  IM_COL32(55, 55, 80, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   IM_COL32(70, 68, 120, 255));
    if (ImGui::Button("Step", {48.f, 28.f})) {
        sim.step(1);
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step one tick");

    // Reset
    ImGui::SameLine(0, 4);
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(40, 40, 56, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  IM_COL32(55, 55, 80, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   IM_COL32(70, 68, 120, 255));
    if (ImGui::Button("Reset", {48.f, 28.f})) {
        simRunning = false;
        sim.reset();
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset simulation");

    // Speed slider
    ImGui::SameLine(0, 16);
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderFloat("##speed", &simSpeedTPS, 1.f, 100000.f,
                           "%.0f TPS", ImGuiSliderFlags_Logarithmic)) {
        sim.setTicksPerSecond(simSpeedTPS);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Simulation speed (ticks per second)");

    // Tick counter
    ImGui::SameLine(0, 16);
    ImGui::TextDisabled("Tick: %llu", (unsigned long long)sim.getWheel().getCurrentTick());

    // Color legend (compact)
    ImGui::SameLine(0, 16);
    ImGui::TextColored({0.30f,0.69f,0.31f,1.f}, "(O)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("VDD (High)");
    ImGui::SameLine(0, 4);
    ImGui::TextColored({0.25f,0.55f,0.90f,1.f}, "(O)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("GND (Low)");
    ImGui::SameLine(0, 4);
    ImGui::TextColored({0.45f,0.45f,0.45f,1.f}, "(O)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Floating");
    ImGui::SameLine(0, 4);
    ImGui::TextColored({0.95f,0.30f,0.25f,1.f}, "(O)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Undefined");

    ImGui::End();
}

// ─── Theme ────────────────────────────────────────────────────────────────────

void App::applyDarkTheme()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();

    // Rounding
    s.WindowRounding    = 6.f;
    s.FrameRounding     = 6.f;
    s.GrabRounding      = 6.f;
    s.TabRounding       = 4.f;
    s.ScrollbarRounding = 6.f;

    // Borders
    s.WindowBorderSize  = 1.f;
    s.FrameBorderSize   = 0.f;
    s.TabBorderSize     = 0.f;
    s.TabBarBorderSize  = 1.f;

    // Spacing
    s.WindowPadding     = {12.f, 10.f};
    s.FramePadding      = { 6.f,  5.f};
    s.ItemSpacing       = { 8.f,  5.f};
    s.ItemInnerSpacing  = { 6.f,  4.f};
    s.IndentSpacing     = 16.f;
    s.ScrollbarSize     = 12.f;

    // Docking
    s.DockingSeparatorSize = 2.f;

    auto& c = s.Colors;

    // Backgrounds
    c[ImGuiCol_WindowBg]          = ImVec4(0.09f, 0.09f, 0.11f, 1.f);
    c[ImGuiCol_ChildBg]           = ImVec4(0.07f, 0.07f, 0.09f, 1.f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.10f, 0.10f, 0.13f, 0.98f);

    // Borders
    c[ImGuiCol_Border]            = ImVec4(0.16f, 0.16f, 0.22f, 0.5f);
    c[ImGuiCol_BorderShadow]      = ImVec4(0.f, 0.f, 0.f, 0.f);

    // Frames
    c[ImGuiCol_FrameBg]           = ImVec4(0.11f, 0.11f, 0.15f, 1.f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.16f, 0.16f, 0.22f, 1.f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.20f, 0.20f, 0.28f, 1.f);

    // Title bars
    c[ImGuiCol_TitleBg]           = ImVec4(0.065f, 0.065f, 0.085f, 1.f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.08f, 0.08f, 0.10f, 1.f);
    c[ImGuiCol_TitleBgCollapsed]  = ImVec4(0.05f, 0.05f, 0.07f, 0.8f);

    // Menu bar
    c[ImGuiCol_MenuBarBg]         = ImVec4(0.08f, 0.08f, 0.10f, 1.f);

    // Headers (CollapsingHeader, etc.)
    c[ImGuiCol_Header]            = ImVec4(0.13f, 0.13f, 0.20f, 1.f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.22f, 0.21f, 0.40f, 1.f);
    c[ImGuiCol_HeaderActive]      = ImVec4(0.28f, 0.26f, 0.55f, 1.f);

    // Buttons
    c[ImGuiCol_Button]            = ImVec4(0.14f, 0.14f, 0.20f, 1.f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.22f, 0.21f, 0.48f, 1.f);
    c[ImGuiCol_ButtonActive]      = ImVec4(0.29f, 0.27f, 0.65f, 1.f);

    // Tabs
    c[ImGuiCol_Tab]               = ImVec4(0.08f, 0.08f, 0.11f, 1.f);
    c[ImGuiCol_TabHovered]        = ImVec4(0.22f, 0.21f, 0.48f, 1.f);
    c[ImGuiCol_TabActive]         = ImVec4(0.15f, 0.15f, 0.25f, 1.f);
    c[ImGuiCol_TabSelected]       = ImVec4(0.15f, 0.15f, 0.25f, 1.f);
    c[ImGuiCol_TabSelectedOverline] = ImVec4(0.38f, 0.36f, 0.85f, 1.f);
    c[ImGuiCol_TabDimmed]         = ImVec4(0.06f, 0.06f, 0.09f, 1.f);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.10f, 0.10f, 0.16f, 1.f);

    // Sliders & grabs
    c[ImGuiCol_SliderGrab]        = ImVec4(0.38f, 0.36f, 0.82f, 1.f);
    c[ImGuiCol_SliderGrabActive]  = ImVec4(0.50f, 0.47f, 0.95f, 1.f);
    c[ImGuiCol_CheckMark]         = ImVec4(0.42f, 0.40f, 0.88f, 1.f);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.06f, 0.06f, 0.08f, 0.6f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.20f, 0.20f, 0.28f, 1.f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.28f, 0.38f, 1.f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.35f, 0.35f, 0.48f, 1.f);

    // Separators
    c[ImGuiCol_Separator]         = ImVec4(0.16f, 0.16f, 0.22f, 0.6f);
    c[ImGuiCol_SeparatorHovered]  = ImVec4(0.38f, 0.36f, 0.82f, 0.8f);
    c[ImGuiCol_SeparatorActive]   = ImVec4(0.48f, 0.45f, 0.95f, 1.f);

    // Resize grips
    c[ImGuiCol_ResizeGrip]        = ImVec4(0.20f, 0.20f, 0.30f, 0.3f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.38f, 0.36f, 0.82f, 0.5f);
    c[ImGuiCol_ResizeGripActive]  = ImVec4(0.48f, 0.45f, 0.95f, 0.8f);

    // Docking
    c[ImGuiCol_DockingPreview]    = ImVec4(0.38f, 0.36f, 0.82f, 0.5f);
    c[ImGuiCol_DockingEmptyBg]    = ImVec4(0.06f, 0.06f, 0.08f, 1.f);

    // Table
    c[ImGuiCol_TableHeaderBg]     = ImVec4(0.11f, 0.11f, 0.16f, 1.f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.16f, 0.16f, 0.22f, 0.6f);
    c[ImGuiCol_TableBorderLight]  = ImVec4(0.12f, 0.12f, 0.17f, 0.5f);
    c[ImGuiCol_TableRowBg]        = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_TableRowBgAlt]     = ImVec4(0.10f, 0.10f, 0.14f, 0.4f);

    // Text
    c[ImGuiCol_Text]              = ImVec4(0.88f, 0.88f, 0.92f, 1.f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.44f, 0.44f, 0.50f, 1.f);
    c[ImGuiCol_TextSelectedBg]    = ImVec4(0.28f, 0.26f, 0.60f, 0.5f);
}
