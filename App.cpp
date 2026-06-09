#include "App.hpp"
#include "IO.hpp"
#include "Pin.hpp"
#include "Net.hpp"
#include "PowerRails.hpp"
#include "json.hpp"

using json = nlohmann::json;

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cctype>

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
        busBtn("1-bit", 1);
        ImGui::SameLine();
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

    if (showSaveCustomPopup) {
        ImGui::OpenPopup("Save Custom Component");
        showSaveCustomPopup = false;
    }
    if (ImGui::BeginPopupModal("Save Custom Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Component Name", saveCustomName, sizeof(saveCustomName));
        ImGui::InputInt("Width", &saveCustomWidth);
        ImGui::InputInt("Height", &saveCustomHeight);
        
        ImGui::Separator();
        ImGui::Text("Input Ports");
        for (auto& p : saveInPorts) {
            ImGui::PushID(p.id);
            ImGui::Text("%s (%d-bit)", p.label.c_str(), p.busWidth);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::Combo("Edge", &p.side, "Left\0Top\0Right\0Bottom\0");
            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::Text("Output Ports");
        for (auto& p : saveOutPorts) {
            ImGui::PushID(p.id);
            ImGui::Text("%s (%d-bit)", p.label.c_str(), p.busWidth);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::Combo("Edge", &p.side, "Left\0Top\0Right\0Bottom\0");
            ImGui::PopID();
        }

        ImGui::Spacing();
        if (ImGui::Button("Save", {88, 28})) {
            CustomComponentDef def;
            def.typeName = saveCustomName;
            def.width = saveCustomWidth;
            def.height = saveCustomHeight;
            def.canvasJson = canvas.serialize();
            
            for (auto& p : saveInPorts) {
                CustomPortDef cpd;
                cpd.internalCompId = p.id;
                cpd.label = p.label;
                cpd.busWidth = p.busWidth;
                cpd.side = p.side;
                cpd.order = p.order; // We will just append them in order for now
                def.inPorts.push_back(cpd);
            }
            for (auto& p : saveOutPorts) {
                CustomPortDef cpd;
                cpd.internalCompId = p.id;
                cpd.label = p.label;
                cpd.busWidth = p.busWidth;
                cpd.side = p.side;
                cpd.order = p.order;
                def.outPorts.push_back(cpd);
            }

            // Save JSON to disk
            json j;
            j["typeName"] = def.typeName;
            j["width"] = def.width;
            j["height"] = def.height;
            j["canvasJson"] = def.canvasJson;
            
            auto portsToJson = [](const std::vector<CustomPortDef>& ports) {
                json arr = json::array();
                for (const auto& p : ports) {
                    json jp;
                    jp["internalCompId"] = p.internalCompId;
                    jp["label"] = p.label;
                    jp["busWidth"] = p.busWidth;
                    jp["side"] = p.side;
                    jp["order"] = p.order;
                    arr.push_back(jp);
                }
                return arr;
            };
            j["inPorts"] = portsToJson(def.inPorts);
            j["outPorts"] = portsToJson(def.outPorts);

            // We should use a file browser, but for simplicity we save to current dir
            std::string filename = std::string(saveCustomName) + ".json";
            FILE* f = fopen(filename.c_str(), "w");
            if (f) {
                std::string out = j.dump(4);
                fwrite(out.c_str(), 1, out.size(), f);
                fclose(f);
            }

            // Register it locally immediately
            canvas.customDefs[def.typeName] = def;

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {88, 28}))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (showLoadCustomPopup) {
        ImGui::OpenPopup("Load Custom Component");
        showLoadCustomPopup = false;
    }
    if (ImGui::BeginPopupModal("Load Custom Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Filename", loadCustomName, sizeof(loadCustomName));
        ImGui::Spacing();
        if (ImGui::Button("Load", {88, 28})) {
            std::string filename = std::string(loadCustomName);
            if (filename.find(".json") == std::string::npos) filename += ".json";
            
            FILE* f = fopen(filename.c_str(), "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                std::string data(size, ' ');
                fread(&data[0], 1, size, f);
                fclose(f);
                
                try {
                    json j = json::parse(data);
                    CustomComponentDef def;
                    def.typeName = j.value("typeName", "Custom");
                    def.width = j.value("width", 100);
                    def.height = j.value("height", 100);
                    def.canvasJson = j.value("canvasJson", "");
                    
                    if (j.contains("inPorts")) {
                        for (auto& jp : j["inPorts"]) {
                            CustomPortDef p;
                            p.internalCompId = jp.value("internalCompId", -1);
                            p.label = jp.value("label", "in");
                            p.busWidth = jp.value("busWidth", 1);
                            p.side = jp.value("side", 0);
                            p.order = jp.value("order", 0);
                            def.inPorts.push_back(p);
                        }
                    }
                    if (j.contains("outPorts")) {
                        for (auto& jp : j["outPorts"]) {
                            CustomPortDef p;
                            p.internalCompId = jp.value("internalCompId", -1);
                            p.label = jp.value("label", "out");
                            p.busWidth = jp.value("busWidth", 1);
                            p.side = jp.value("side", 0);
                            p.order = jp.value("order", 0);
                            def.outPorts.push_back(p);
                        }
                    }
                    
                    canvas.customDefs[def.typeName] = def;
                } catch (...) {}
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
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
        if (ImGui::MenuItem("Save Custom Component")) {
            showSaveCustomPopup = true;
            saveCustomName[0] = '\0';
            saveCustomWidth = 100;
            saveCustomHeight = 100;
            saveInPorts.clear();
            saveOutPorts.clear();
            for (const auto& cv : canvas.getComps()) {
                if (cv.typeName == "PORT_IN" || cv.typeName == "PORT_OUT") {
                    PortUI p;
                    p.id = cv.id;
                    p.busWidth = cv.busWidth;
                    if (cv.typeName == "PORT_IN") {
                        if (auto* pi = dynamic_cast<PortIn*>(cv.comp.get())) p.label = pi->label;
                        saveInPorts.push_back(p);
                    } else {
                        if (auto* po = dynamic_cast<PortOut*>(cv.comp.get())) p.label = po->label;
                        saveOutPorts.push_back(p);
                    }
                }
            }
        }
        if (ImGui::MenuItem("Load Custom Component")) {
            showLoadCustomPopup = true;
            loadCustomName[0] = '\0';
        }
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

// Helper: draw a small inline icon for gate type on the draw list
static void drawPaletteIcon(ImDrawList* dl, ImVec2 pos, const char* type, ImU32 col)
{
    const float S = 12.f; // icon size
    const float cx = pos.x + S * 0.5f;
    const float cy = pos.y + S * 0.5f;

    if (strcmp(type, "NOT") == 0 || strcmp(type, "BUF") == 0) {
        // Triangle (inverter symbol)
        dl->AddTriangleFilled(
            ImVec2(pos.x, pos.y), ImVec2(pos.x + S, cy), ImVec2(pos.x, pos.y + S), col);
    } else if (strcmp(type, "AND") == 0 || strcmp(type, "NAND") == 0) {
        // D-shape: left flat line + right arc
        dl->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(cx, pos.y + S), col, 0.f);
        dl->PathArcTo(ImVec2(cx, cy), S * 0.5f, -1.57f, 1.57f, 12);
        dl->PathFillConvex(col);
    } else if (strcmp(type, "OR") == 0 || strcmp(type, "NOR") == 0) {
        // Shield/curved shape
        dl->AddBezierQuadratic(ImVec2(pos.x, pos.y), ImVec2(cx, pos.y - 2.f), ImVec2(pos.x + S, cy), col, 1.5f, 12);
        dl->AddBezierQuadratic(ImVec2(pos.x + S, cy), ImVec2(cx, pos.y + S + 2.f), ImVec2(pos.x, pos.y + S), col, 1.5f, 12);
        dl->AddBezierQuadratic(ImVec2(pos.x, pos.y + S), ImVec2(pos.x + 3.f, cy), ImVec2(pos.x, pos.y), col, 1.5f, 12);
    } else if (strcmp(type, "XOR") == 0 || strcmp(type, "XNOR") == 0) {
        // Curved shape with extra line
        dl->AddBezierQuadratic(ImVec2(pos.x + 2.f, pos.y), ImVec2(cx + 2.f, pos.y - 2.f), ImVec2(pos.x + S, cy), col, 1.5f, 12);
        dl->AddBezierQuadratic(ImVec2(pos.x + S, cy), ImVec2(cx + 2.f, pos.y + S + 2.f), ImVec2(pos.x + 2.f, pos.y + S), col, 1.5f, 12);
        dl->AddBezierQuadratic(ImVec2(pos.x + 2.f, pos.y + S), ImVec2(pos.x + 5.f, cy), ImVec2(pos.x + 2.f, pos.y), col, 1.5f, 12);
        // Extra input arc line
        dl->AddBezierQuadratic(ImVec2(pos.x, pos.y + S), ImVec2(pos.x + 3.f, cy), ImVec2(pos.x, pos.y), col, 1.5f, 12);
    } else if (strcmp(type, "SW") == 0) {
        // Toggle circle
        dl->AddCircle(ImVec2(cx, cy), S * 0.45f, col, 12, 1.5f);
        dl->AddCircleFilled(ImVec2(cx + 2.f, cy), 2.5f, col);
    } else if (strcmp(type, "BTN") == 0) {
        // Square with dot
        dl->AddRect(ImVec2(pos.x + 1.f, pos.y + 1.f), ImVec2(pos.x + S - 1.f, pos.y + S - 1.f), col, 1.f, 0, 1.5f);
        dl->AddCircleFilled(ImVec2(cx, cy), 2.f, col);
    } else if (strcmp(type, "CLK") == 0) {
        // Small sine/square wave
        float y0 = cy + 3.f, y1 = cy - 3.f;
        dl->AddLine(ImVec2(pos.x, y0), ImVec2(pos.x + 2.f, y0), col, 1.5f);
        dl->AddLine(ImVec2(pos.x + 2.f, y0), ImVec2(pos.x + 2.f, y1), col, 1.5f);
        dl->AddLine(ImVec2(pos.x + 2.f, y1), ImVec2(pos.x + 5.f, y1), col, 1.5f);
        dl->AddLine(ImVec2(pos.x + 5.f, y1), ImVec2(pos.x + 5.f, y0), col, 1.5f);
        dl->AddLine(ImVec2(pos.x + 5.f, y0), ImVec2(pos.x + 8.f, y0), col, 1.5f);
        dl->AddLine(ImVec2(pos.x + 8.f, y0), ImVec2(pos.x + 8.f, y1), col, 1.5f);
        dl->AddLine(ImVec2(pos.x + 8.f, y1), ImVec2(pos.x + S, y1), col, 1.5f);
    } else if (strcmp(type, "LED") == 0) {
        // Filled circle (LED)
        dl->AddCircleFilled(ImVec2(cx, cy), S * 0.45f, col);
    } else if (strcmp(type, "REG") == 0) {
        // Box with 'R'
        dl->AddRect(ImVec2(pos.x, pos.y), ImVec2(pos.x + S, pos.y + S), col, 1.f, 0, 1.5f);
        // Tiny R letter approximation
        dl->AddLine(ImVec2(pos.x + 3.f, pos.y + 3.f), ImVec2(pos.x + 3.f, pos.y + S - 3.f), col, 1.5f);
        dl->AddLine(ImVec2(pos.x + 3.f, pos.y + 3.f), ImVec2(pos.x + 7.f, pos.y + 3.f), col, 1.5f);
        dl->AddLine(ImVec2(pos.x + 7.f, pos.y + 3.f), ImVec2(pos.x + 7.f, cy), col, 1.5f);
        dl->AddLine(ImVec2(pos.x + 7.f, cy), ImVec2(pos.x + 3.f, cy), col, 1.5f);
        dl->AddLine(ImVec2(pos.x + 5.f, cy), ImVec2(pos.x + 8.f, pos.y + S - 3.f), col, 1.5f);
    } else {
        // Default: small filled square
        dl->AddRectFilled(ImVec2(pos.x + 2.f, pos.y + 2.f), ImVec2(pos.x + S - 2.f, pos.y + S - 2.f), col, 1.f);
    }
}

// Helper: draw a teal accent bar on the left of a collapsing header
static void drawHeaderAccentBar(ImDrawList* dl)
{
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    dl->AddRectFilled(ImVec2(min.x, min.y + 2.f), ImVec2(min.x + 3.f, max.y - 2.f),
        IM_COL32(51, 166, 184, 200), 1.f);
}

// Helper: case-insensitive substring match
static bool matchesFilter(const char* label, const char* filter)
{
    if (!filter || filter[0] == '\0') return true;
    // simple case-insensitive search
    std::string haystack(label);
    std::string needle(filter);
    std::transform(haystack.begin(), haystack.end(), haystack.begin(),
        [](unsigned char c){ return (char)std::tolower(c); });
    std::transform(needle.begin(), needle.end(), needle.begin(),
        [](unsigned char c){ return (char)std::tolower(c); });
    return haystack.find(needle) != std::string::npos;
}

void App::renderPalette()
{
    ImGui::Begin("Components");

    // Filter / search
    static char filterBuf[64] = "";
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##filter", "Search...", filterBuf, sizeof(filterBuf));
    ImGui::Spacing();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 iconCol = IM_COL32(51, 166, 184, 220); // teal icon color

    // Lambda: palette button with inline icon
    auto paletteBtn = [&](const char* label, const char* type, const char* tip) {
        if (!matchesFilter(label, filterBuf)) return;
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(28, 28, 38, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(30, 70, 78, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(35, 110, 120, 255));

        // Reserve space for icon by padding the label
        char iconLabel[128];
        std::snprintf(iconLabel, sizeof(iconLabel), "     %s", label);

        if (ImGui::Button(iconLabel, {-1.f, 30.f}))
            canvas.beginPlacement(type);

        // Draw icon on top of the button
        ImVec2 bmin = ImGui::GetItemRectMin();
        ImVec2 bmax = ImGui::GetItemRectMax();
        float iconY = bmin.y + (bmax.y - bmin.y - 12.f) * 0.5f;
        drawPaletteIcon(dl, ImVec2(bmin.x + 8.f, iconY), type, iconCol);

        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered() && tip)
            ImGui::SetTooltip("%s", tip);
    };

    auto busPaletteBtn = [&](const char* label, const char* type, const char* tip) {
        if (!matchesFilter(label, filterBuf)) return;
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(28, 28, 38, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(30, 70, 78, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(35, 110, 120, 255));

        char iconLabel[128];
        std::snprintf(iconLabel, sizeof(iconLabel), "     %s", label);

        if (ImGui::Button(iconLabel, {-1.f, 30.f})) {
            pendingBusType = type;
            showBusWidthPopup = true;
        }

        ImVec2 bmin = ImGui::GetItemRectMin();
        ImVec2 bmax = ImGui::GetItemRectMax();
        float iconY = bmin.y + (bmax.y - bmin.y - 12.f) * 0.5f;
        drawPaletteIcon(dl, ImVec2(bmin.x + 8.f, iconY), type, iconCol);

        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered() && tip)
            ImGui::SetTooltip("%s", tip);
    };

    if (ImGui::CollapsingHeader("Logic Gates", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawHeaderAccentBar(dl);
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
        busPaletteBtn("Register", "REG", "N-bit register (rising edge clock)");
        ImGui::Spacing();
        ImGui::Unindent(4.f);
    }

    if (ImGui::CollapsingHeader("Custom Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawHeaderAccentBar(dl);
        ImGui::Indent(4.f);
        ImGui::Spacing();
        busPaletteBtn("Port In",  "PORT_IN",  "Boundary input for packaging custom components");
        busPaletteBtn("Port Out", "PORT_OUT", "Boundary output for packaging custom components");
        ImGui::Spacing();
        ImGui::Unindent(4.f);
    }

    if (ImGui::CollapsingHeader("Inputs / Sources", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawHeaderAccentBar(dl);
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
        drawHeaderAccentBar(dl);
        ImGui::Indent(4.f);
        ImGui::Spacing();
        busPaletteBtn("Bus Merge", "BUS_MERGE", "N x 1-bit \xe2\x86\x92 N-bit bus");
        busPaletteBtn("Bus Split", "BUS_SPLIT", "N-bit bus \xe2\x86\x92 N x 1-bit");
        paletteBtn("Junction", "JUNCTION", "Wire pass-through node");
        ImGui::Spacing();
        ImGui::Unindent(4.f);
    }

    if (ImGui::CollapsingHeader("Outputs", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawHeaderAccentBar(dl);
        ImGui::Indent(4.f);
        ImGui::Spacing();
        paletteBtn("LED",      "LED",      "Single-bit LED indicator");
        paletteBtn("Num Disp", "NUM_DISP", "4-bit numeric display (0\xe2\x80\x93" "15)");
        ImGui::Spacing();
        ImGui::Unindent(4.f);
    }

    if (!canvas.customDefs.empty()) {
        if (ImGui::CollapsingHeader("Loaded Custom", ImGuiTreeNodeFlags_DefaultOpen)) {
            drawHeaderAccentBar(dl);
            ImGui::Indent(4.f);
            ImGui::Spacing();
            for (const auto& [name, def] : canvas.customDefs) {
                paletteBtn(name.c_str(), name.c_str(), "Custom user component");
            }
            ImGui::Spacing();
            ImGui::Unindent(4.f);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Shortcuts")) {
        drawHeaderAccentBar(dl);
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
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                // VDD
                ImVec2 cur = ImGui::GetCursorScreenPos();
                dl->AddCircleFilled(ImVec2(cur.x + 12.f, cur.y + 7.f), 5.f, IM_COL32(76, 176, 80, 255));
                ImGui::Indent(22.f);
                ImGui::TextColored({0.30f,0.69f,0.31f,1.f}, "VDD: %s",
                    stateLabel(sim.getVddNet()->getState(), sim));
                ImGui::Unindent(22.f);
                // GND
                cur = ImGui::GetCursorScreenPos();
                dl->AddCircleFilled(ImVec2(cur.x + 12.f, cur.y + 7.f), 5.f, IM_COL32(64, 140, 230, 255));
                ImGui::Indent(22.f);
                ImGui::TextColored({0.25f,0.55f,0.90f,1.f}, "GND: %s",
                    stateLabel(sim.getGndNet()->getState(), sim));
                ImGui::Unindent(22.f);
            }
        }
        ImGui::End();
        return;
    }

    if (type == "PORT_IN") {
        auto* p = static_cast<PortIn*>(comp);
        char buf[32];
        strncpy(buf, p->label.c_str(), sizeof(buf));
        if (ImGui::InputText("Label", buf, sizeof(buf))) {
            p->label = buf;
        }
    }
    if (type == "PORT_OUT") {
        auto* p = static_cast<PortOut*>(comp);
        char buf[32];
        strncpy(buf, p->label.c_str(), sizeof(buf));
        if (ImGui::InputText("Label", buf, sizeof(buf))) {
            p->label = buf;
        }
    }

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
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 cur = ImGui::GetCursorScreenPos();
            ImU32 lc = led->isLit() ? IM_COL32(76, 176, 80, 255) : IM_COL32(64, 140, 230, 255);
            dl->AddCircleFilled(ImVec2(cur.x + 8.f, cur.y + 7.f), 5.f, lc);
            ImGui::Indent(18.f);
            ImGui::TextColored(ledCol, "State: %s",
                stateLabel(led->getLitState(), sim));
            ImGui::Unindent(18.f);
        }
    }

    if (type == "BUS_MERGE" || type == "BUS_SPLIT" || type == "REG") {
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
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX))
    {
        ImGui::TableSetupColumn("Pin",   ImGuiTableColumnFlags_WidthFixed, 56.f);
        ImGui::TableSetupColumn("Dir",   ImGuiTableColumnFlags_WidthFixed, 38.f);
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
            {
                ImDrawList* pdl = ImGui::GetWindowDrawList();
                ImVec2 cur = ImGui::GetCursorScreenPos();
                ImU32 pc = ImGui::ColorConvertFloat4ToU32(sColors[ci]);
                pdl->AddCircleFilled(ImVec2(cur.x + 5.f, cur.y + 7.f), 4.f, pc);
                ImGui::Indent(14.f);
                ImGui::TextColored(sColors[ci], "%s", lbl);
                ImGui::Unindent(14.f);
            }
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
            {
                ImDrawList* pdl = ImGui::GetWindowDrawList();
                ImVec2 cur = ImGui::GetCursorScreenPos();
                ImU32 pc = ImGui::ColorConvertFloat4ToU32(sColors[ci]);
                pdl->AddCircleFilled(ImVec2(cur.x + 5.f, cur.y + 7.f), 4.f, pc);
                ImGui::Indent(14.f);
                ImGui::TextColored(sColors[ci], "%s", lbl);
                ImGui::Unindent(14.f);
            }
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

    // Color legend (compact, filled circles)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        struct LegendEntry { ImU32 col; const char* tip; };
        LegendEntry legends[] = {
            { IM_COL32(76, 176, 80, 255),   "VDD (High)" },
            { IM_COL32(64, 140, 230, 255),  "GND (Low)" },
            { IM_COL32(115, 115, 115, 255), "Floating" },
            { IM_COL32(242, 77, 64, 255),   "Undefined" },
        };
        for (int li = 0; li < 4; ++li) {
            ImGui::SameLine(0, li == 0 ? 16.f : 8.f);
            ImVec2 cur = ImGui::GetCursorScreenPos();
            float cy = cur.y + ImGui::GetTextLineHeight() * 0.5f;
            dl->AddCircleFilled(ImVec2(cur.x + 6.f, cy), 6.f, legends[li].col);
            ImGui::Dummy({14.f, ImGui::GetTextLineHeight()});
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", legends[li].tip);
        }
    }

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

    // Teal accent base:  ImVec4(0.20f, 0.65f, 0.72f, 1.f)
    // Lighter teal:      ImVec4(0.28f, 0.78f, 0.85f, 1.f)
    // Darker teal:       ImVec4(0.14f, 0.50f, 0.56f, 1.f)

    // Backgrounds (warmed up slightly)
    c[ImGuiCol_WindowBg]          = ImVec4(0.08f, 0.08f, 0.10f, 1.f);
    c[ImGuiCol_ChildBg]           = ImVec4(0.07f, 0.07f, 0.09f, 1.f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.09f, 0.09f, 0.12f, 0.98f);

    // Borders
    c[ImGuiCol_Border]            = ImVec4(0.16f, 0.16f, 0.22f, 0.5f);
    c[ImGuiCol_BorderShadow]      = ImVec4(0.f, 0.f, 0.f, 0.f);

    // Frames
    c[ImGuiCol_FrameBg]           = ImVec4(0.11f, 0.11f, 0.15f, 1.f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.14f, 0.18f, 0.22f, 1.f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.16f, 0.24f, 0.28f, 1.f);

    // Title bars
    c[ImGuiCol_TitleBg]           = ImVec4(0.065f, 0.065f, 0.085f, 1.f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.08f, 0.08f, 0.10f, 1.f);
    c[ImGuiCol_TitleBgCollapsed]  = ImVec4(0.05f, 0.05f, 0.07f, 0.8f);

    // Menu bar
    c[ImGuiCol_MenuBarBg]         = ImVec4(0.08f, 0.08f, 0.10f, 1.f);

    // Headers (CollapsingHeader, etc.) — teal accent
    c[ImGuiCol_Header]            = ImVec4(0.10f, 0.16f, 0.18f, 1.f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.14f, 0.38f, 0.42f, 1.f);
    c[ImGuiCol_HeaderActive]      = ImVec4(0.18f, 0.52f, 0.58f, 1.f);

    // Buttons — teal accent
    c[ImGuiCol_Button]            = ImVec4(0.12f, 0.16f, 0.18f, 1.f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.16f, 0.42f, 0.48f, 1.f);
    c[ImGuiCol_ButtonActive]      = ImVec4(0.20f, 0.58f, 0.65f, 1.f);

    // Tabs — teal accent
    c[ImGuiCol_Tab]               = ImVec4(0.08f, 0.08f, 0.11f, 1.f);
    c[ImGuiCol_TabHovered]        = ImVec4(0.14f, 0.40f, 0.45f, 1.f);
    c[ImGuiCol_TabActive]         = ImVec4(0.10f, 0.22f, 0.25f, 1.f);
    c[ImGuiCol_TabSelected]       = ImVec4(0.10f, 0.22f, 0.25f, 1.f);
    c[ImGuiCol_TabSelectedOverline] = ImVec4(0.20f, 0.65f, 0.72f, 1.f);
    c[ImGuiCol_TabDimmed]         = ImVec4(0.06f, 0.06f, 0.09f, 1.f);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.08f, 0.14f, 0.16f, 1.f);

    // Sliders & grabs — teal accent
    c[ImGuiCol_SliderGrab]        = ImVec4(0.20f, 0.65f, 0.72f, 1.f);
    c[ImGuiCol_SliderGrabActive]  = ImVec4(0.28f, 0.78f, 0.85f, 1.f);
    c[ImGuiCol_CheckMark]         = ImVec4(0.22f, 0.70f, 0.76f, 1.f);

    // Scrollbar — teal tinted
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.06f, 0.06f, 0.08f, 0.6f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.14f, 0.28f, 0.32f, 1.f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.42f, 0.48f, 1.f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.22f, 0.55f, 0.60f, 1.f);

    // Separators — teal accent
    c[ImGuiCol_Separator]         = ImVec4(0.14f, 0.20f, 0.22f, 0.6f);
    c[ImGuiCol_SeparatorHovered]  = ImVec4(0.20f, 0.65f, 0.72f, 0.8f);
    c[ImGuiCol_SeparatorActive]   = ImVec4(0.28f, 0.78f, 0.85f, 1.f);

    // Resize grips — teal accent
    c[ImGuiCol_ResizeGrip]        = ImVec4(0.14f, 0.28f, 0.32f, 0.3f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.20f, 0.65f, 0.72f, 0.5f);
    c[ImGuiCol_ResizeGripActive]  = ImVec4(0.28f, 0.78f, 0.85f, 0.8f);

    // Docking — teal accent
    c[ImGuiCol_DockingPreview]    = ImVec4(0.20f, 0.65f, 0.72f, 0.5f);
    c[ImGuiCol_DockingEmptyBg]    = ImVec4(0.06f, 0.06f, 0.08f, 1.f);

    // Table
    c[ImGuiCol_TableHeaderBg]     = ImVec4(0.09f, 0.13f, 0.15f, 1.f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.14f, 0.20f, 0.22f, 0.6f);
    c[ImGuiCol_TableBorderLight]  = ImVec4(0.10f, 0.15f, 0.17f, 0.5f);
    c[ImGuiCol_TableRowBg]        = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_TableRowBgAlt]     = ImVec4(0.08f, 0.11f, 0.13f, 0.4f);

    // Text — increased contrast
    c[ImGuiCol_Text]              = ImVec4(0.90f, 0.91f, 0.94f, 1.f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.44f, 0.44f, 0.50f, 1.f);
    c[ImGuiCol_TextSelectedBg]    = ImVec4(0.14f, 0.42f, 0.48f, 0.5f);
}
