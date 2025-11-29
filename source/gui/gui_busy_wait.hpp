#ifndef GUI_BUSYWAIT_HPP
#define GUI_BUSYWAIT_HPP

#include <gui/gui.hpp>
#include <string>
#include <vector>

class GuiBusyWait : public Gui
{
public:
    GuiBusyWait(std::string description) : Gui()
    {
        set_text(description);
    }

    void draw() override;
    void update() override;
    bool contains(int x, int y) override;
    void close() override;
    void refresh() override;
    bool use_cursor() override { return false; }

    void set_text(const std::string &text);

private:
    std::vector<std::string> text_lines;
};

#endif