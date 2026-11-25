// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "stopwatch/motion.h"
#include "ui/view.h"

#include <functional>
#include <memory>
#include <string>

namespace properdesktop::ui {
class Label;
}

namespace clockapp {

// The round button of the stopwatch and the timers: a tinted disc with a
// hairline ring a little inside its edge, lit from above, its title in the
// tint. Changing the role or title cross-fades rather than jumps.
class RoundButton : public properdesktop::ui::View
{
public:
    enum class Role {
        Neutral,
        Start,
        Stop,
        // Pause and resume.
        Pause,
    };

    explicit RoundButton(properdesktop::ui::Window* window, double diameter = 76);
    ~RoundButton() override;

    void setTitle(const std::string& title);
    const std::string& title() const { return m_title; }
    // A play triangle in place of the title, which stays its accessible name.
    void setPlayGlyph(bool play);
    void setRole(Role role);
    Role role() const { return m_role; }

    // The width the title keeps to: as far inside the ring as the ring is
    // inside the edge.
    double titleWidth() const;
    // A title too long for that width is set smaller, in whole points, down
    // to this size; past it, the title needs a shorter word.
    double minimumTitleSize() const;
    double titleSize() const { return m_titleSize; }

    properdesktop::ui::SizeF sizeHint() const override { return {m_diameter, m_diameter}; }
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    std::function<void()> onClicked;

protected:
    void layout() override;
    void paint(properdesktop::ui::Painter& painter) override;
    void enabledChanged() override;
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const properdesktop::ui::PointerEvent& event) override;
    void pointerRelease(const properdesktop::ui::PointerEvent& event) override;
    bool keyPress(const properdesktop::ui::KeyEvent& event) override;
    void focusIn(properdesktop::ui::FocusReason) override { requestPaint(); }
    void focusOut() override { requestPaint(); }

private:
    void updateState();
    void fitTitle();

    double m_diameter;
    double m_titleSize = 0;
    std::string m_title;
    Role m_role = Role::Neutral;
    Role m_previousRole = Role::Neutral;
    properdesktop::ui::Label* m_label = nullptr;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_playGlyph = false;
    // 0 at rest, 1 hovered, 2 pressed; and the fade from the previous role.
    std::unique_ptr<Glide> m_state;
    std::unique_ptr<Glide> m_roleFade;
};

} // namespace clockapp
