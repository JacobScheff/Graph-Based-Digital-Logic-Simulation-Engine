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

    // Custom Component Save Modal State
    bool showSaveCustomPopup = false;
    char saveCustomName[128] = "";
    int  saveCustomWidth = 100;
    int  saveCustomHeight = 100;
    
    bool showLoadCustomPopup = false;
    char loadCustomName[128] = "";
    
    // Data populated when opening Save modal
    struct PortUI {
        int id;
        std::string label;
        int busWidth;
        int side = 0; // 0=L, 1=T, 2=R, 3=B
        float t = 0.5f;
        int order = 0;
    };
    std::vector<PortUI> saveInPorts;
    std::vector<PortUI> saveOutPorts;
    int savePreviewDragIdx = -1;
    bool savePreviewDragIsInput = true;

    void renderSaveCustomPreview();

    void tick(double dt);
    void renderFrame();

    // ImGui panels
    void renderMenuBar();
    void renderPalette();
    void renderProperties();
    void renderSimControls();
    void renderCanvas();
    void buildDefaultLayout(unsigned int dockId);

    std::string currentFilePath;
    std::vector<std::string> customComponentPaths;
    void loadRegistry();
    void saveRegistry();
    void appendToRegistry(const std::string& path);
    bool loadCustomComponentFile(const std::string& filename);
    
    // Palette Edit Context Menu
    std::string componentToEditPath;
    std::string componentToEditName;
    bool showConfirmEditModal = false;

    static void applyDarkTheme();
    static void glfwErrorCallback(int, const char*);
};
