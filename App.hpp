#pragma once
#include "Simulator.hpp"
#include "Canvas.hpp"

struct GLFWwindow;

class App
{
public:
    App()  = default;
    ~App() = default;

    bool init();
    void run();
    void shutdown();

private:
    GLFWwindow* window = nullptr;
    Simulator   sim;
    Canvas      canvas{&sim};

    double lastTime     = 0.0;
    float  simSpeedTPS  = 1000.f;  // ticks per second
    bool   simRunning   = false;
    bool   layoutInitialized = false;

    int         pendingBusWidth = 4;
    bool        showBusWidthPopup = false;
    std::string pendingBusType;

    void tick(double dt);
    void renderFrame();

    // ImGui panels
    void renderMenuBar();
    void renderPalette();
    void renderProperties();
    void renderSimControls();
    void renderCanvas();
    void buildDefaultLayout(ImGuiID dockId);

    static void applyDarkTheme();
    static void glfwErrorCallback(int, const char*);
};
