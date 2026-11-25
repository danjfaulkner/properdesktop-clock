// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "world/cardgrid.h"

#include <algorithm>
#include <cmath>

#include "style.h"
#include "ui/window.h"
#include "world/citycard.h"

using namespace properdesktop::ui;

namespace clockapp::world {

static constexpr double kMoveSettleMs = 420;
static constexpr double kPresenceSettleMs = 380;
static constexpr double kLiftSettleMs = 200;
static constexpr double kDragDistance = 5;

class CardGrid::DragOverlay : public View
{
public:
    explicit DragOverlay(CardGrid* owner)
        : m_owner(owner)
    {
    }

    ~DragOverlay() override
    {
        if (m_owner) {
            m_owner->m_overlay = nullptr;
            m_owner->m_floating = nullptr;
        }
    }

    void detach() { m_owner = nullptr; }

private:
    CardGrid* m_owner;
};

static double cardHeight()
{
    return CityCard::heightForCardWidth(CardGrid::kCardWidth);
}

CardGrid::CardGrid()
{
    setName("world:grid");
}

CardGrid::~CardGrid()
{
    if (m_overlay) {
        m_overlay->detach();
        if (View* parent = m_overlay->parent())
            parent->takeChild(m_overlay);
        m_overlay = nullptr;
        m_floating = nullptr;
    }
}

int CardGrid::columnsFor(double width)
{
    const double available = width - 2 * style::pagePadding;
    return std::max(1, int(std::floor((available + style::cardGap) / (kCardWidth + style::cardGap))));
}

CityCard* CardGrid::card(const std::string& zoneId) const
{
    for (CityCard* card : m_cards) {
        if (card->zoneId() == zoneId && !card->removing)
            return card;
    }
    return nullptr;
}

std::vector<CityCard*> CardGrid::staying() const
{
    std::vector<CityCard*> result;
    for (CityCard* card : m_cards) {
        if (!card->removing)
            result.push_back(card);
    }
    return result;
}

void CardGrid::setCities(const std::vector<const City*>& cities, bool animated)
{
    std::vector<CityCard*> order;
    for (const City* city : cities) {
        CityCard* existing = card(city->zoneId);
        if (!existing) {
            existing = addChild<CityCard>(*city);
            existing->setEditProgress(m_editProgress);
            const std::string id = city->zoneId;
            existing->onRemove = [this, id] {
                if (onRemove)
                    onRemove(id);
            };
            existing->onKey = [this, existing](const KeyEvent& event) { return onKey && onKey(existing, event); };
            if (animated && m_laidOut) {
                existing->presence.jump(0);
                existing->presence.target = 1;
            }
            existing->setVisible(false);
            if (onCardMade)
                onCardMade(existing);
        }
        order.push_back(existing);
    }

    // What is no longer listed shrinks away from where it stands.
    for (CityCard* card : m_cards) {
        if (std::find(order.begin(), order.end(), card) != order.end() || card->removing)
            continue;
        card->removing = true;
        card->presence.target = 0;
        if (!animated || !m_laidOut)
            card->presence.jump(0);
    }
    std::vector<CityCard*> leaving;
    for (CityCard* card : m_cards) {
        if (card->removing)
            leaving.push_back(card);
    }
    m_cards = order;
    m_cards.insert(m_cards.end(), leaving.begin(), leaving.end());

    if (!animated || !m_laidOut)
        settleNow();
    requestLayout();
    if (onMotion)
        onMotion();
}

void CardGrid::setTopInset(double inset)
{
    if (inset == m_topInset)
        return;
    m_topInset = inset;
    m_jumpNext = true;
    requestLayout();
}

void CardGrid::setEditProgress(double progress)
{
    m_editProgress = progress;
    for (CityCard* card : m_cards)
        card->setEditProgress(progress);
}

std::vector<CardGrid::Slot> CardGrid::slots(size_t count, double width) const
{
    const int columns = columnsFor(width);
    const double rowWidth = columns * kCardWidth + (columns - 1) * style::cardGap;
    const double left = std::max(style::pagePadding, style::centred(width, rowWidth));
    const double height = cardHeight();
    std::vector<Slot> result;
    for (size_t i = 0; i < count; ++i) {
        const size_t row = i / size_t(columns);
        const size_t column = i % size_t(columns);
        result.push_back({left + double(column) * (kCardWidth + style::cardGap),
                          m_topInset + style::pagePadding + double(row) * (height + style::cardGap)});
    }
    return result;
}

double CardGrid::heightForWidth(double width) const
{
    const size_t count = staying().size();
    if (count == 0)
        return m_topInset;
    const int columns = columnsFor(width);
    const size_t rows = (count + size_t(columns) - 1) / size_t(columns);
    const double height = cardHeight();
    return m_topInset + 2 * style::pagePadding + double(rows) * height + double(rows - 1) * style::cardGap;
}

SizeF CardGrid::sizeHint() const
{
    return {0, heightForWidth(geometry().width)};
}

void CardGrid::retarget(bool jump)
{
    const std::vector<CityCard*> shown = staying();
    const std::vector<Slot> places = slots(shown.size(), geometry().width);
    for (size_t i = 0; i < shown.size(); ++i) {
        CityCard* card = shown[i];
        if (m_dragging && card == m_pressed)
            continue;
        card->x.target = places[i].x;
        card->y.target = places[i].y;
        if (jump || !card->isVisible() || card->presence.value == 0) {
            card->x.jump(places[i].x);
            card->y.jump(places[i].y);
        }
    }
}

void CardGrid::layout()
{
    const double width = geometry().width;
    const bool resized = width != m_width;
    m_width = width;
    // A resize of the window moves the cards with it, at once.
    retarget(!m_laidOut || resized || m_jumpNext);
    m_laidOut = true;
    m_jumpNext = false;
    bool moving = false;
    for (CityCard* card : m_cards) {
        place(card);
        moving = moving || card->x.value != card->x.target || card->y.value != card->y.target
            || card->presence.value != card->presence.target;
    }
    if (moving && onMotion)
        onMotion();
}

void CardGrid::place(CityCard* card)
{
    const double scale = window() ? window()->scale() : 1;
    const PointF origin = card == m_floating ? mapToWindow({card->x.value, card->y.value})
                                             : PointF{card->x.value, card->y.value};
    card->setGeometry({style::snap(origin.x, scale), style::snap(origin.y, scale), kCardWidth, cardHeight()});
    const double presence = std::clamp(card->presence.value, 0.0, 1.0);
    const double lift = std::clamp(card->lift.value, 0.0, 1.0);
    const double size = (0.86 + 0.14 * presence) * (1 + 0.035 * lift);
    card->setOpacity(presence);
    card->setTransform(std::abs(size - 1) < 1e-4 ? Transform{} : Transform{{0, 0}, size});
    card->setVisible(presence > 0.001 || !card->removing);
}

void CardGrid::settleNow()
{
    for (CityCard* card : m_cards) {
        if (!(m_dragging && card == m_pressed)) {
            card->x.jump(card->x.target);
            card->y.jump(card->y.target);
        }
        card->presence.jump(card->presence.target);
        card->lift.jump(card->lift.target);
    }
    if (!m_dragging && m_floating)
        landFromOverlay();
    advance(0);
    for (CityCard* card : m_cards)
        place(card);
}

bool CardGrid::advance(double elapsedMs)
{
    bool moving = false;
    std::vector<CityCard*> gone;
    for (CityCard* card : m_cards) {
        const bool wasMoving = !(card->x.isAtRest() && card->y.isAtRest() && card->presence.isAtRest()
                                 && card->lift.isAtRest() && card->x.value == card->x.target
                                 && card->y.value == card->y.target && card->presence.value == card->presence.target
                                 && card->lift.value == card->lift.target);
        if (wasMoving) {
            if (!(m_dragging && card == m_pressed)) {
                card->x.advance(elapsedMs, kMoveSettleMs);
                card->y.advance(elapsedMs, kMoveSettleMs);
            }
            card->presence.advance(elapsedMs, kPresenceSettleMs);
            card->lift.advance(elapsedMs, kLiftSettleMs);
            place(card);
            moving = moving || !(card->x.isAtRest() && card->y.isAtRest() && card->presence.isAtRest() && card->lift.isAtRest());
        }
        if (card->advance(elapsedMs))
            moving = true;
        if (card->removing && card->presence.isAtRest() && card->presence.value <= 0)
            gone.push_back(card);
    }
    for (CityCard* card : gone) {
        m_cards.erase(std::find(m_cards.begin(), m_cards.end(), card));
        if (m_pressed == card) {
            m_pressed = nullptr;
            m_dragging = false;
        }
        if (m_floating == card)
            landFromOverlay();
        takeChild(card);
    }
    if (!m_dragging && m_floating && m_floating->x.isAtRest() && m_floating->y.isAtRest()
        && m_floating->lift.isAtRest() && m_floating->x.value == m_floating->x.target
        && m_floating->y.value == m_floating->y.target && m_floating->lift.value == m_floating->lift.target) {
        landFromOverlay();
    }
    if (!gone.empty())
        requestLayout();
    return moving;
}

void CardGrid::updateFloating()
{
    if (m_floating)
        place(m_floating);
}

void CardGrid::cancelDrag()
{
    if (m_dragging)
        endDrag();
    else
        m_pressed = nullptr;
    if (m_floating)
        landFromOverlay();
}

size_t CardGrid::slotAt(PointF centre) const
{
    const std::vector<CityCard*> shown = staying();
    const std::vector<Slot> places = slots(shown.size(), geometry().width);
    size_t best = 0;
    double bestDistance = 1e18;
    for (size_t i = 0; i < places.size(); ++i) {
        const double dx = places[i].x + kCardWidth / 2 - centre.x;
        const double dy = places[i].y + cardHeight() / 2 - centre.y;
        if (dx * dx + dy * dy < bestDistance) {
            bestDistance = dx * dx + dy * dy;
            best = i;
        }
    }
    return best;
}

CityCard* CardGrid::cardAt(PointF position) const
{
    if (m_floating && !m_floating->removing && m_floating->isVisible()) {
        const PointF local = position - PointF{m_floating->x.value, m_floating->y.value};
        if (m_floating->localRect().contains(local))
            return m_floating;
    }
    for (auto it = children().rbegin(); it != children().rend(); ++it) {
        auto* card = dynamic_cast<CityCard*>(it->get());
        if (card && !card->removing && card->isVisible() && card->geometry().contains(position))
            return card;
    }
    return nullptr;
}

void CardGrid::pointerPress(const PointerEvent& event)
{
    CityCard* card = cardAt(event.position);
    if (!card)
        return;
    if (event.button == MouseButton::Right) {
        if (onContextMenu)
            onContextMenu(card->zoneId(), mapToWindow(event.position));
        return;
    }
    if (event.button != MouseButton::Left)
        return;
    card->setFocus(FocusReason::Other);
    startDrag(card, event.position);
}

void CardGrid::pointerMove(const PointerEvent& event)
{
    drag(event.position);
}

void CardGrid::pointerRelease(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    endDrag();
}

void CardGrid::startDrag(CityCard* card, PointF position)
{
    m_pressed = card;
    m_dragging = false;
    m_pressAt = position;
    m_grabOffset = {position.x - card->x.value, position.y - card->y.value};
}

void CardGrid::drag(PointF position)
{
    CityCard* card = m_pressed;
    if (!card || card->removing)
        return;
    if (!m_dragging) {
        if (std::hypot(position.x - m_pressAt.x, position.y - m_pressAt.y) < kDragDistance || staying().size() < 2)
            return;
        m_dragging = true;
        const std::vector<CityCard*> shown = staying();
        m_dragFrom = size_t(std::find(shown.begin(), shown.end(), card) - shown.begin());
        liftToOverlay(card);
        card->lift.target = 1;
    }
    card->x.jump(position.x - m_grabOffset.x);
    card->y.jump(position.y - m_grabOffset.y);

    // The others make room where the card would drop.
    const size_t target = slotAt({card->x.value + kCardWidth / 2, card->y.value + cardHeight() / 2});
    auto it = std::find(m_cards.begin(), m_cards.end(), card);
    const size_t current = size_t(it - m_cards.begin());
    if (target != current && target < staying().size()) {
        m_cards.erase(it);
        m_cards.insert(m_cards.begin() + ptrdiff_t(target), card);
        retarget(false);
    }
    place(card);
    if (onMotion)
        onMotion();
}

void CardGrid::endDrag()
{
    CityCard* card = m_pressed;
    if (!card)
        return;
    m_pressed = nullptr;
    if (!m_dragging)
        return;
    m_dragging = false;
    card->lift.target = 0;
    retarget(false);
    const std::vector<CityCard*> shown = staying();
    const size_t to = size_t(std::find(shown.begin(), shown.end(), card) - shown.begin());
    if (onMotion)
        onMotion();
    if (to != m_dragFrom && onMoved)
        onMoved(m_dragFrom, to);
}

void CardGrid::liftToOverlay(CityCard* card)
{
    if (m_floating == card)
        return;
    if (m_floating)
        landFromOverlay();
    Window* win = window();
    if (!win) {
        raiseChild(card);
        return;
    }
    if (!m_overlay)
        m_overlay = win->overlay()->addChild<DragOverlay>(this);
    const bool focused = card->hasFocus();
    if (std::unique_ptr<View> taken = takeChild(card)) {
        m_overlay->addChild(std::move(taken));
        m_floating = card;
        if (focused)
            card->setFocus(FocusReason::Other);
    }
}

void CardGrid::landFromOverlay()
{
    if (!m_floating)
        return;
    CityCard* card = m_floating;
    m_floating = nullptr;
    const bool focused = card->hasFocus();
    if (m_overlay) {
        if (std::unique_ptr<View> taken = m_overlay->takeChild(card))
            addChild(std::move(taken));
        m_overlay->detach();
        if (View* parent = m_overlay->parent())
            parent->takeChild(m_overlay);
        m_overlay = nullptr;
    }
    if (focused)
        card->setFocus(FocusReason::Other);
    place(card);
}

} // namespace clockapp::world
