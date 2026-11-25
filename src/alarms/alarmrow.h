// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/view.h"

#include <base/timer.h>

#include <functional>
#include <memory>
#include <string>

namespace properdesktop::ui {
class Label;
class Switch;
class Window;
}

namespace clockapp {

class Spring;

// One alarm on the page: a card with its time large, its label and repeat
// under it, and its switch. Switched off, the text dims; switched on, the
// line under the time says for a moment how long until it rings.
class AlarmRow : public properdesktop::ui::View
{
public:
    static constexpr double Height = 92;

    AlarmRow(properdesktop::ui::Window* window, std::string id);
    ~AlarmRow() override;

    const std::string& id() const { return m_id; }

    struct Content
    {
        std::string time;
        // AM or PM; empty on a 24-hour clock.
        std::string half;
        std::string detail;
        bool active = false;
    };
    void setContent(const Content& content, bool animate);
    bool isActive() const { return m_active; }
    // "in 7 hours 20 minutes" under the time for a few seconds.
    void flashHint(const std::string& text);

    properdesktop::ui::SizeF sizeHint() const override { return {0, Height}; }
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    std::function<void()> onOpen;
    std::function<void(bool on)> onToggled;

protected:
    void layout() override;
    void paint(properdesktop::ui::Painter& painter) override;
    void pointerPress(const properdesktop::ui::PointerEvent& event) override;
    void pointerRelease(const properdesktop::ui::PointerEvent& event) override;
    bool keyPress(const properdesktop::ui::KeyEvent& event) override;
    void focusIn(properdesktop::ui::FocusReason reason) override;
    void focusOut() override;
    void hoverChanged() override;

private:
    void applyDim();

    std::string m_id;
    bool m_active = false;
    bool m_pressed = false;
    properdesktop::ui::Label* m_time;
    properdesktop::ui::Label* m_half;
    properdesktop::ui::Label* m_detail;
    properdesktop::ui::Label* m_hint;
    properdesktop::ui::Switch* m_switch;
    // 1 on, 0 off.
    std::unique_ptr<Spring> m_light;
    // 0 the detail line, 1 the hint.
    std::unique_ptr<Spring> m_hintMix;
    properdesktop::base::Timer m_hintTimer;
};

} // namespace clockapp
