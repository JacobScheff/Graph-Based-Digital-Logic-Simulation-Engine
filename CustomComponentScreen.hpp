#pragma once

#include "Canvas.hpp"
#include "CustomComponent.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

inline std::optional<ScreenDef> detectScreenFromCanvas(const Canvas& canvas)
{
    struct Entry {
        int id;
        float cx;
        float cy;
        float h;
    };

    std::vector<Entry> entries;
    for (const auto& cv : canvas.getComps()) {
        if (cv.typeName != "RGB_DISP" || !cv.comp)
            continue;
        entries.push_back({
            cv.id,
            cv.pos.x + cv.size.x * 0.5f,
            cv.pos.y + cv.size.y * 0.5f,
            cv.size.y
        });
    }
    if (entries.empty())
        return std::nullopt;

    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) {
                  if (std::fabs(a.cy - b.cy) <= std::max(a.h, b.h) * 0.5f)
                      return a.cx < b.cx;
                  return a.cy < b.cy;
              });

    const float tol = entries.front().h * 0.6f;
    std::vector<std::vector<Entry>> rows;
    for (const auto& e : entries) {
        bool placed = false;
        for (auto& row : rows) {
            if (std::fabs(e.cy - row.front().cy) <= tol) {
                row.push_back(e);
                placed = true;
                break;
            }
        }
        if (!placed)
            rows.push_back({e});
    }

    std::sort(rows.begin(), rows.end(),
              [](const auto& a, const auto& b) { return a.front().cy < b.front().cy; });
    for (auto& row : rows)
        std::sort(row.begin(), row.end(),
                  [](const Entry& a, const Entry& b) { return a.cx < b.cx; });

    ScreenDef screen;
    screen.cols = 1;
    screen.rows = static_cast<int>(rows.size());
    for (const auto& row : rows)
        screen.cols = std::max(screen.cols, static_cast<int>(row.size()));

    for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
        for (int c = 0; c < static_cast<int>(rows[r].size()); ++c) {
            ScreenPixelDef pixel;
            pixel.internalCompId = rows[r][c].id;
            pixel.col = c;
            pixel.row = r;
            screen.pixels.push_back(pixel);
        }
    }
    return screen;
}
