// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "headerbar.h"
#include "style.h"

#include "base/event_loop.h"
#include "ui/animation.h"
#include "ui/application.h"
#include "ui/header_button.h"
#include "ui/header_button_group.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/segmented_control.h"
#include "ui/theme.h"
#include "ui/window.h"
#include "ui/window_controls.h"
#include "ui/window_header.h"

#include <algorithm>
#include <cmath>
#include <memory>

using namespace properdesktop::ui;

namespace clockapp {

static constexpr double kPlateHeight = HeaderButtonGroup::kSegment;
static constexpr double kTextPadding = 12;
// The switch keeps this clear of the window buttons and the page's buttons.
static constexpr double kSwitchMargin = 12;
static constexpr double kSwitchMinimumWidth = 280;
// Around the widest tab's label, in every tab, as the tabs share the width.
static constexpr double kSwitchLabelPadding = 12;

// A text button on a plate of the window controls' style, as "Edit".
class HeaderTextButton : public View
{
public:
    explicit HeaderTextButton(const HeaderAction& action)
        : m_label(addChild<Label>(action.text))
        , m_triggered(action.triggered)
        , m_prominent(action.prominent)
    {
        m_label->setFontSize(style::points(9.5));
        m_label->setWeight(m_prominent ? 600 : 500);
        m_label->setElide(Label::Elide::None);
        setEnabled(action.enabled);
    }

    SizeF sizeHint() const override
    {
        return {std::ceil(m_label->sizeHint().width) + 2 * kTextPadding, kPlateHeight};
    }

    bool acceptsPointer() const override { return true; }

    void updateActive() { updateColor(); }

protected:
    void layout() override
    {
        const double scale = window() ? window()->scale() : 1;
        const SizeF text = m_label->sizeHint();
        const double baseline = style::snap(geometry().height / 2 + m_label->capHeight() / 2, scale);
        m_label->setGeometry({style::snap((geometry().width - text.width) / 2, scale),
                              baseline - m_label->baselineOffset(), text.width, text.height});
        updateColor();
    }

    void paint(Painter& painter) override
    {
        const Window* window = this->window();
        const bool dark = window ? window->headerDarkMode() : Theme::current().darkMode;
        window_controls::paintGroupPlate(painter, localRect(), dark);
        if (m_hovered && isEnabled()) {
            const Color overlay = dark ? Color::rgb(0xFFFFFF) : Color::rgb(0x000000);
            painter.fillRoundedRect(localRect(), geometry().height / 2,
                                    overlay.withAlpha(m_pressed ? 0.08 : 0.04));
        }
    }

    void enabledChanged() override
    {
        updateColor();
        requestPaint();
    }

    void pointerEnter() override
    {
        m_hovered = true;
        updateColor();
        requestPaint();
    }

    void pointerLeave() override
    {
        m_hovered = false;
        m_pressed = false;
        updateColor();
        requestPaint();
    }

    void pointerPress(const PointerEvent& event) override
    {
        if (event.button != MouseButton::Left || !isEnabled())
            return;
        m_pressed = true;
        updateColor();
        requestPaint();
    }

    void pointerRelease(const PointerEvent& event) override
    {
        if (!m_pressed)
            return;
        m_pressed = false;
        updateColor();
        requestPaint();
        if (localRect().contains(event.position) && m_triggered)
            m_triggered();
    }

private:
    void updateColor()
    {
        const Window* window = this->window();
        const bool dark = window ? window->headerDarkMode() : Theme::current().darkMode;
        const bool strong = (window && window->isActive()) || m_hovered;
        Color color = m_prominent ? style::withAlpha(style::accent(), strong ? 1.0 : 0.5)
                                  : window_controls::inkColor(dark, strong);
        if (!isEnabled())
            color = Theme::current().disabledText;
        else if (m_pressed)
            color = style::withAlpha(color, 0.6);
        m_label->setColor(ColorRecipe(color));
    }

    Label* m_label;
    std::function<void()> m_triggered;
    bool m_prominent;
    bool m_hovered = false;
    bool m_pressed = false;
};

// The page's buttons in a row: glyph buttons next to each other share a plate.
class HeaderBar::Actions : public View
{
public:
    explicit Actions(std::vector<HeaderAction> actions)
    {
        HeaderButtonGroup* group = nullptr;
        for (HeaderAction& action : actions) {
            if (action.glyph.empty()) {
                group = nullptr;
                auto* button = addChild<HeaderTextButton>(action);
                button->setName("header:" + action.id);
                m_items.push_back(button);
                m_textButtons.push_back(button);
                continue;
            }
            if (!group) {
                group = addChild<HeaderButtonGroup>();
                m_items.push_back(group);
            }
            HeaderButton* button = group->addButton(action.glyph);
            button->setName("header:" + action.id);
            button->setToolTip(action.text);
            button->setEnabled(action.enabled);
            button->onClicked = action.triggered;
        }
    }

    void updateActive()
    {
        for (HeaderTextButton* button : m_textButtons)
            button->updateActive();
    }

    SizeF sizeHint() const override
    {
        double width = 0;
        for (const View* item : m_items)
            width += item->sizeHint().width;
        if (!m_items.empty())
            width += kPlateGap * double(m_items.size() - 1);
        return {width, kPlateHeight};
    }

protected:
    void layout() override
    {
        double x = 0;
        for (View* item : m_items) {
            const double width = item->sizeHint().width;
            item->setGeometry({x, 0, width, kPlateHeight});
            x += width + kPlateGap;
        }
    }

private:
    std::vector<View*> m_items;
    std::vector<HeaderTextButton*> m_textButtons;
};

HeaderBar::HeaderBar()
    : m_tabs(addChild<SegmentedControl>())
{
    m_tabs->setName("header:tabs");
    m_tabs->setFontSize(style::points(9.5));
    m_tabs->setPlateInset(0);
}

HeaderBar::~HeaderBar() = default;

void HeaderBar::setActions(std::vector<HeaderAction> actions, bool animated)
{
    // A button of the old row may be the one calling, from its click.
    if (m_actions) {
        std::shared_ptr<View> doomed(takeChild(m_actions));
        Application::instance()->loop().post([doomed] {});
    }
    m_actions = addChild<Actions>(std::move(actions));
    m_actions->setName("header:actions");
    requestLayout();
    if (m_fade)
        m_fade->stop();
    if (!animated || !window()) {
        m_actions->setOpacity(1);
        return;
    }
    if (!m_fade) {
        m_fade = std::make_unique<Animation>(window());
        m_fade->setDuration(std::chrono::milliseconds(220));
        m_fade->setEasing(Animation::Easing::OutCubic);
    }
    Actions* faded = m_actions;
    faded->setLayered(true);
    faded->setOpacity(0);
    m_fade->onUpdate = [faded](double value) { faded->setOpacity(value); };
    m_fade->onFinished = [faded] {
        faded->setOpacity(1);
        faded->setLayered(false);
    };
    m_fade->start();
}

void HeaderBar::updateActive()
{
    if (m_actions)
        m_actions->updateActive();
}

double HeaderBar::controlCentre() const
{
    const auto* header = dynamic_cast<const WindowHeader*>(parent());
    const double top = header ? header->buttonsTop() : (geometry().height - window_controls::kButton) / 2;
    return top + window_controls::kButton / 2;
}

RectF HeaderBar::actionsRect() const
{
    return m_actions ? m_actions->geometry() : RectF{};
}

void HeaderBar::layout()
{
    const Window* window = this->window();
    const double scale = window ? window->scale() : 1;
    const double width = geometry().width;
    const double centre = controlCentre();

    // As far from the end as from the top, as the window buttons keep to the start.
    const double plateTop = style::snap(centre - kPlateHeight / 2, scale);
    double end = width - plateTop;
    if (m_actions) {
        const SizeF size = m_actions->sizeHint();
        end -= size.width;
        m_actions->setGeometry({style::snap(end, scale), plateTop, size.width, kPlateHeight});
    }

    // Centred on the window, moved aside only as far as the buttons beside it need.
    const auto* header = dynamic_cast<const WindowHeader*>(parent());
    const double start = (header ? header->leadingInset() : 0) + kSwitchMargin;
    const double stop = (m_actions && m_actions->sizeHint().width > 0 ? end : width - plateTop) - kSwitchMargin;
    double widest = 0;
    for (size_t i = 0; i < m_tabs->count(); ++i)
        widest = std::max(widest, m_tabs->implicitItemWidth(i));
    const double hint = (widest + 2 * kSwitchLabelPadding) * double(m_tabs->count()) + 2 * m_tabs->padding();
    const double switchWidth = std::max(std::min(kSwitchMinimumWidth, stop - start), std::min(hint, stop - start));
    const double x = std::max(start, std::min(stop - switchWidth, (width - switchWidth) / 2));
    m_tabs->setGeometry({style::snap(x, scale), style::snap(centre - kSwitchHeight / 2, scale),
                         style::snap(switchWidth, scale), kSwitchHeight});
}

} // namespace clockapp
