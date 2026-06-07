#include "App.hpp"
#include <cstdio>

int main()
{
    App app;
    if (!app.init()) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
