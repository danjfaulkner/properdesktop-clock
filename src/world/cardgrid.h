// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ui/view.h"
#include "world/citydatabase.h"

namespace clockapp::world {

class CityCard;

// Cards of one fixed size in as many columns as the page fits. Every card glides to its
// place on the frame clock: added ones grow in where they land, removed
// ones shrink away as the rest close the gap, and a card dragged out of
// place lifts and follows the pointer while the others make room.
class CardGrid : public properdesktop::ui::View
{
public:
    static constexpr double kCardWidth = 171;

    CardGrid();
    ~CardGrid() override;

    // The cities in order; new ones come in animated when `animated`.
    void setCities(const std::vector<const City*>& cities, bool animated);
    // The cards in the order shown, the ones leaving included.
    const std::vector<CityCard*>& cards() const { return m_cards; }
    CityCard* card(const std::string& zoneId) const;

    void setTopInset(double inset);
    void setEditProgress(double progress);

    // How many columns fit a width.
    static int columnsFor(double width);

    // Puts every card where it is heading at once.
    void settleNow();
    // Moves every card on; whether any still moves.
    bool advance(double elapsedMs);
    // Keeps a card floating in the window's overlay aligned with the grid.
    void updateFloating();
    // Ends any drag and puts a floating card back in the grid.
    void cancelDrag();

    bool acceptsPointer() const override { return true; }
    double heightForWidth(double width) const override;
    properdesktop::ui::SizeF sizeHint() const override;

    // Something started to move.
    std::function<void()> onMotion;
    std::function<void(const std::string& zoneId)> onRemove;
    std::function<void(const std::string& zoneId, properdesktop::ui::PointF windowPosition)> onContextMenu;
    // A card dropped at another index.
    std::function<void(size_t from, size_t to)> onMoved;
    // Arrow keys from a card, for moving the focus.
    std::function<bool(CityCard* card, const properdesktop::ui::KeyEvent& event)> onKey;
    // Each card made, for the page to fill.
    std::function<void(CityCard* card)> onCardMade;

protected:
    void layout() override;
    void pointerPress(const properdesktop::ui::PointerEvent& event) override;
    void pointerMove(const properdesktop::ui::PointerEvent& event) override;
    void pointerRelease(const properdesktop::ui::PointerEvent& event) override;

private:
    class DragOverlay;

    struct Slot
    {
        double x;
        double y;
    };

    std::vector<Slot> slots(size_t count, double width) const;
    void retarget(bool jump);
    void place(CityCard* card);
    CityCard* cardAt(properdesktop::ui::PointF position) const;
    void startDrag(CityCard* card, properdesktop::ui::PointF position);
    void drag(properdesktop::ui::PointF position);
    void endDrag();
    void liftToOverlay(CityCard* card);
    void landFromOverlay();
    size_t slotAt(properdesktop::ui::PointF centre) const;
    std::vector<CityCard*> staying() const;

    std::vector<CityCard*> m_cards;
    double m_topInset = 0;
    double m_editProgress = 0;
    double m_width = 0;
    bool m_laidOut = false;
    // The next layout puts the cards in place without motion.
    bool m_jumpNext = false;

    CityCard* m_pressed = nullptr;
    bool m_dragging = false;
    properdesktop::ui::PointF m_pressAt;
    properdesktop::ui::PointF m_grabOffset;
    size_t m_dragFrom = 0;
    DragOverlay* m_overlay = nullptr;
    CityCard* m_floating = nullptr;
};

} // namespace clockapp::world
