#ifndef GUI_DIRTSCREEN_HPP
#define GUI_DIRTSCREEN_HPP

#include "gui.hpp"

#include <string>
#include <vector>

class gui_dirtscreen : public gui
{
public:
    gui_dirtscreen(const gertex::GXView &viewport);

    void draw() override;
    void update() override;
    bool contains(int x, int y) override;
    void close() override;
    void refresh() override;
    bool use_cursor() override { return false; }

    void set_text(const std::string &text);
    void set_progress(uint8_t progress, uint8_t max_progress);

private:
    uint8_t progress = 0;
    uint8_t max_progress = 0;
    std::vector<std::string> text_lines;
};

#endif