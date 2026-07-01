#pragma once

#include "Canvas.hpp"
#include "CustomComponent.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

inline std::optional<ScreenDef> detectScreenFromCanvas(
    const Canvas& canvas,
    const std::unordered_map<std::string, CustomComponentDef>& customDefs)
{
    struct BlockPixel {
        int internalCompId = -1;
        int col = 0;
        int row = 0;
        int innerHostCompId = -1;
    };

    struct ScreenBlock {
        int compId = -1;
        float cx = 0.f;
        float cy = 0.f;
        float h = 0.f;
        int cols = 1;
        int rows = 1;
        bool nested = false;
        std::vector<BlockPixel> pixels;
    };

    std::vector<ScreenBlock> blocks;
    for (const auto& cv : canvas.getComps()) {
        if (!cv.comp) continue;

        if (cv.typeName == "RGB_DISP") {
            ScreenBlock block;
            block.compId = cv.id;
            block.cx = cv.pos.x + cv.size.x * 0.5f;
            block.cy = cv.pos.y + cv.size.y * 0.5f;
            block.h = cv.size.y;
            block.pixels.push_back({cv.id, 0, 0});
            blocks.push_back(std::move(block));
            continue;
        }

        auto defIt = customDefs.find(cv.typeName);
        if (defIt == customDefs.end() || !defIt->second.screen.has_value())
            continue;

        const ScreenDef& inner = defIt->second.screen.value();
        if (inner.pixels.empty())
            continue;

        ScreenBlock block;
        block.compId = cv.id;
        block.cx = cv.pos.x + cv.size.x * 0.5f;
        block.cy = cv.pos.y + cv.size.y * 0.5f;
        block.h = cv.size.y;
        block.cols = std::max(1, inner.cols);
        block.rows = std::max(1, inner.rows);
        block.nested = true;
        for (const auto& px : inner.pixels) {
            block.pixels.push_back({px.internalCompId, px.col, px.row, px.hostCompId});
        }
        blocks.push_back(std::move(block));
    }

    if (blocks.empty())
        return std::nullopt;

    std::sort(blocks.begin(), blocks.end(),
              [](const ScreenBlock& a, const ScreenBlock& b) {
                  if (std::fabs(a.cy - b.cy) <= std::max(a.h, b.h) * 0.5f)
                      return a.cx < b.cx;
                  return a.cy < b.cy;
              });

    const float tol = blocks.front().h * 0.6f;
    std::vector<std::vector<ScreenBlock>> rows;
    for (const auto& block : blocks) {
        bool placed = false;
        for (auto& row : rows) {
            if (std::fabs(block.cy - row.front().cy) <= tol) {
                row.push_back(block);
                placed = true;
                break;
            }
        }
        if (!placed)
            rows.push_back({block});
    }

    std::sort(rows.begin(), rows.end(),
              [](const auto& a, const auto& b) { return a.front().cy < b.front().cy; });
    for (auto& row : rows)
        std::sort(row.begin(), row.end(),
                  [](const ScreenBlock& a, const ScreenBlock& b) { return a.cx < b.cx; });

    ScreenDef screen;
    screen.cols = 0;
    screen.rows = 0;
    for (const auto& row : rows) {
        int rowCols = 0;
        for (const auto& block : row)
            rowCols += block.cols;
        screen.cols = std::max(screen.cols, rowCols);
        screen.rows += row.empty() ? 0 : row.front().rows;
    }

    int rowBase = 0;
    for (const auto& row : rows) {
        if (row.empty()) continue;
        int colBase = 0;
        for (const auto& block : row) {
            for (const auto& local : block.pixels) {
                ScreenPixelDef pixel;
                pixel.col = colBase + local.col;
                pixel.row = rowBase + local.row;
                pixel.internalCompId = local.internalCompId;
                pixel.hostCompId = block.nested ? block.compId : -1;
                pixel.nestedHostCompId = local.innerHostCompId;
                screen.pixels.push_back(pixel);
            }
            colBase += block.cols;
        }
        rowBase += row.front().rows;
    }

    return screen;
}
