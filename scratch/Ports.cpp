// ─── Port In: Boundary input for custom components ──────────────────────────────
class PortIn : public Component
{
public:
    explicit PortIn(int width = 1)
        : Component("PORT_IN", width, width, 1), label("in")
    {
        setBusWidth(width);
    }
    std::string label;
    void update() override {
        if (!getSimulator()) return;
        int w = getBusWidth();
        for (int i = 0; i < w; ++i) {
            drivers[i]->setState(receivers[i]->getState());
        }
    }
};

// ─── Port Out: Boundary output for custom components ──────────────────────────
class PortOut : public Component
{
public:
    explicit PortOut(int width = 1)
        : Component("PORT_OUT", width, width, 1), label("out")
    {
        setBusWidth(width);
    }
    std::string label;
    void update() override {
        if (!getSimulator()) return;
        int w = getBusWidth();
        for (int i = 0; i < w; ++i) {
            drivers[i]->setState(receivers[i]->getState());
        }
    }
};
