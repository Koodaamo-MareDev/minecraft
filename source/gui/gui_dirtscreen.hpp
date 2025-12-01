#ifndef GUI_DIRTSCREEN_HPP
#define GUI_DIRTSCREEN_HPP

#include <gui/gui.hpp>

#include <string>
#include <vector>

struct Progress
{
    uint8_t progress = 0;
    uint8_t max_progress = 0;
};

class GuiDirtscreen : public Gui
{
public:
    void draw() override;
    void update() override;
    bool contains(int x, int y) override;
    void close() override;
    void refresh() override;
    bool use_cursor() override { return false; }

    void set_text(const std::string &text);
    void set_progress(Progress *prog);

private:
    std::vector<std::string> text_lines;
    Progress *progress = nullptr;
};
#endif