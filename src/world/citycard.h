// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "ui/view.h"
#include "world/citydatabase.h"
#include "world/settle.h"
#include "world/sky.h"
#include "world/zonetime.h"

namespace properdesktop::ui {
class Label;
}

namespace clockapp::world {

class ClockFace;
class RemoveButton;

// One city: its face over its name, the day and the hours from here, and the
// time in figures. The card's top glows faintly in the colour of its sky.
// In edit mode a remove button sits in its top left corner, concentric with
// it.
class CityCard : public properdesktop::ui::View
{
public:
    static constexpr double kRadius = 18;
    static constexpr double kRemoveDiameter = 22;

    explicit CityCard(const City& city);

    const std::string& zoneId() const { return m_city.zoneId; }
    const City& city() const { return m_city; }

    // The card's height at a width.
    static double heightForCardWidth(double width);

    // Reads the time there at `utcMilliseconds`, against the zone `here`;
    // the second hand stands at `secondStep` of its step. A day that
    // changes crossfades when `animated`.
    void update(const ZoneTime& here, int64_t utcMilliseconds, double secondStep, bool animated);
    void setSecondStep(double secondStep);
    // From 0 to 1, how far the remove button has come in.
    void setEditProgress(double progress);
    bool isEditing() const { return m_editProgress > 0.5; }

    // Advances the caption's crossfade; whether it still moves.
    bool advance(double elapsedMs);

    // The motion the grid gives the card: where it is heading, how far it
    // has come in (0 to 1, out when removed) and how high a drag lifts it.
    SpringState x;
    SpringState y;
    SpringState presence;
    SpringState lift;
    bool removing = false;

    std::function<void()> onRemove;
    std::function<bool(const properdesktop::ui::KeyEvent& event)> onKey;

    std::string captionText() const;
    std::string timeText() const;
    ClockFace* face() const { return m_face; }
    properdesktop::ui::RectF removeButtonRect() const;
    RemoveButton* removeButton() const { return m_remove; }

    properdesktop::ui::RectF visualRect() const override;
    bool acceptsFocus() const override { return true; }

protected:
    void layout() override;
    void paint(properdesktop::ui::Painter& painter) override;
    bool keyPress(const properdesktop::ui::KeyEvent& event) override;
    void focusIn(properdesktop::ui::FocusReason reason) override;
    void focusOut() override;

private:
    void setCaption(std::string text, bool animated);
    void setTime(const std::string& text);

    City m_city;
    uint32_t m_seed;
    ClockFace* m_face;
    properdesktop::ui::Label* m_name;
    properdesktop::ui::Label* m_caption;
    properdesktop::ui::Label* m_previousCaption;
    properdesktop::ui::Label* m_time;
    properdesktop::ui::Label* m_period;
    SpringState m_captionFade;
    Sky m_sky;
    ZoneTime m_there;
    double m_editProgress = 0;
    RemoveButton* m_remove;
    // The day period before the figures, as some languages write it.
    bool m_periodFirst = false;
};

} // namespace clockapp::world
