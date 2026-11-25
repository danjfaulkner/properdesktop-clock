// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "stopwatch/motion.h"
#include "ui/view.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace properdesktop::ui {
class Label;
}

namespace clockapp {

struct LapExtremes;

// The laps, newest first: the lap running now on top, then the completed
// ones, the fastest in green and the slowest in red. A new lap slides the
// list down a row and fades in at the top.
class LapsList : public properdesktop::ui::View
{
public:
    static constexpr double RowHeight = 44;

    explicit LapsList(properdesktop::ui::Window* window);
    ~LapsList() override;

    // The completed laps, the first first; the running lap's time, or a
    // negative one for none.
    void setLaps(const std::vector<int64_t>& durations, std::optional<LapExtremes> extremes, int64_t currentNs,
                 bool animate);
    void setCurrent(int64_t currentNs);
    // The rows' distance from the list's sides: the list spans the window
    // so its scroll bar sits at the window's edge.
    void setInsets(double left, double right);

    size_t rowCount() const { return m_rows.size(); }

    properdesktop::ui::SizeF sizeHint() const override;
    double heightForWidth(double) const override { return sizeHint().height; }

protected:
    void layout() override;
    void paint(properdesktop::ui::Painter& painter) override;

private:
    struct Row
    {
        properdesktop::ui::View* view = nullptr;
        properdesktop::ui::Label* name = nullptr;
        properdesktop::ui::Label* time = nullptr;
    };

    Row makeRow();
    void setRow(Row& row, size_t lapNumber, int64_t ns, int mark);

    properdesktop::ui::Window* m_window;
    std::vector<Row> m_rows;
    bool m_hasCurrent = false;
    double m_left = 0;
    double m_right = 0;
    // From -RowHeight up to 0 as a new lap slides in.
    std::unique_ptr<Glide> m_slide;
};

} // namespace clockapp
