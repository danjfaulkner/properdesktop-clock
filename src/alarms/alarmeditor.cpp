// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/alarmeditor.h"

#include "alarms/alarmtime.h"
#include "alarms/spring.h"
#include "alarms/timepicker.h"
#include "alarms/toneplayer.h"
#include "alarms/tones.h"
#include "style.h"

#include <base/event_loop.h>
#include <base/i18n.h>
#include <base/locale.h>

#include "ui/application.h"
#include "ui/cell_layout.h"
#include "ui/dialog_window.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/row_combo_box.h"
#include "ui/switch.h"
#include "ui/text_field.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>

using namespace properdesktop::ui;
using properdesktop::i18n::tr;

namespace clockapp {

static constexpr double DialogWidth = 400;
static constexpr double DialogPadding = 16;
static constexpr double RowHeight = 44;
static constexpr double RowInset = 14;
static constexpr double ChipSize = 28;
static constexpr double ChipGap = 4;

static Corners themeCorners()
{
    return Theme::current().continuousCorners ? Corners::Continuous : Corners::Circular;
}

// A card of rows, a label on the left and its control on the right, a
// hairline between them.
class EditorCard : public View
{
public:
    View* addRow(const std::string& text, std::unique_ptr<View> control)
    {
        View* row = addChild<View>();
        Label* label = row->addChild<Label>(text);
        label->setVerticallyCentered(true);
        row->addChild(std::move(control));
        return row->children()[1].get();
    }

    double heightForWidth(double) const override { return RowHeight * double(children().size()); }
    SizeF sizeHint() const override { return {0, heightForWidth(0)}; }

protected:
    void layout() override
    {
        const double width = geometry().width;
        double y = 0;
        for (const auto& row : children()) {
            row->setGeometry({0, y, width, RowHeight});
            y += RowHeight;
            auto* label = static_cast<Label*>(row->children()[0].get());
            View* control = row->children()[1].get();
            const SizeF size = control->sizeHint();
            const double controlWidth = std::ceil(control->layoutHints().preferredWidth > 0
                                                      ? control->layoutHints().preferredWidth
                                                      : size.width);
            const double controlHeight = std::ceil(size.height);
            double overhang = 0;
            if (const auto* box = dynamic_cast<const RowComboBox*>(control))
                overhang = box->trailingOverhang();
            control->setGeometry({width - RowInset - controlWidth + overhang, style::centred(RowHeight, controlHeight),
                                  controlWidth, controlHeight});
            const double labelHeight = std::ceil(label->sizeHint().height);
            label->setGeometry({RowInset, style::centred(RowHeight, labelHeight),
                                std::max(0.0, width - 3 * RowInset - controlWidth + overhang), labelHeight});
        }
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        painter.fillRoundedRect(localRect(), theme.bigRadius, theme.secondBackground, themeCorners());
        const double hairline = 1 / painter.scale();
        for (size_t i = 0; i + 1 < children().size(); ++i) {
            const RectF row = children()[i]->geometry();
            painter.fillRoundedRect({RowInset, row.bottom() - hairline, geometry().width - 2 * RowInset, hairline}, 0,
                                    style::separator());
        }
    }
};

// One day of the week to repeat on: a round chip that fills with the
// accent when chosen.
class DayChip : public View
{
public:
    DayChip(Window* window, std::string text, bool chosen)
        : m_chosen(chosen)
        , m_fill(std::make_unique<Spring>(window, 180, chosen ? 1 : 0))
    {
        m_label = addChild<Label>(std::move(text));
        m_label->setFontSize(Theme::current().smallFontSize());
        m_label->setWeight(600);
        m_label->setTextAlignment(Label::TextAlignment::Center);
        m_fill->onChanged = [this] {
            updateColor();
            requestPaint();
        };
        updateColor();
    }

    bool isChosen() const { return m_chosen; }

    void setChosen(bool chosen)
    {
        if (chosen == m_chosen)
            return;
        m_chosen = chosen;
        m_fill->setTarget(chosen ? 1 : 0);
        if (onToggled)
            onToggled();
    }

    SizeF sizeHint() const override { return {ChipSize, ChipSize}; }
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    std::function<void()> onToggled;

protected:
    void layout() override
    {
        const double height = std::ceil(m_label->sizeHint().height);
        m_label->setGeometry({0, style::centred(geometry().height, height), geometry().width, height});
    }

    void paint(Painter& painter) override
    {
        const double t = m_fill->value();
        const Color off = style::controlTint();
        const Color on = style::accent();
        const Color fill{uint8_t(std::lround(off.r + (on.r - off.r) * t)),
                         uint8_t(std::lround(off.g + (on.g - off.g) * t)),
                         uint8_t(std::lround(off.b + (on.b - off.b) * t)),
                         uint8_t(std::lround(off.a + (on.a - off.a) * t))};
        painter.fillEllipse(localRect(), fill);
        if (hasVisibleFocus())
            painter.strokeRoundedRect(localRect(), ChipSize / 2, style::withAlpha(style::accent(), 0.6), 2);
    }

    void pointerRelease(const PointerEvent& event) override
    {
        if (localRect().contains(event.position))
            setChosen(!m_chosen);
    }

    bool keyPress(const KeyEvent& event) override
    {
        if (event.key == key::Space || event.key == key::Return) {
            setChosen(!m_chosen);
            return true;
        }
        return false;
    }

    void focusIn(FocusReason) override { requestPaint(); }
    void focusOut() override { requestPaint(); }

private:
    void updateColor()
    {
        const Color off = style::text();
        const Color on = Theme::current().highlightedText;
        const double t = m_fill->value();
        m_label->setColor(ColorRecipe(t > 0.5 ? on : off));
    }

    bool m_chosen;
    std::unique_ptr<Spring> m_fill;
    Label* m_label;
};

// The seven days in the locale's week order.
class DayChips : public View
{
public:
    DayChips(Window* window, uint8_t repeat)
    {
        const properdesktop::base::Locale& locale = properdesktop::base::Locale::system();
        const int first = locale.firstDayOfWeek();
        for (int i = 0; i < 7; ++i) {
            // 1 Monday to 7 Sunday, as the locale counts; bit 0 is Sunday.
            const int day = (first - 1 + i) % 7 + 1;
            const int bit = day % 7;
            DayChip* chip = addChild<DayChip>(window, locale.standaloneDayName(day, properdesktop::base::Locale::FormatType::Narrow),
                                              (repeat & (1 << bit)) != 0);
            chip->setName("editor:day" + std::to_string(bit));
            chip->setToolTip(locale.standaloneDayName(day));
            chip->onToggled = [this] {
                if (onChanged)
                    onChanged();
            };
            m_chips.push_back({chip, bit});
        }
    }

    uint8_t repeat() const
    {
        uint8_t repeat = 0;
        for (const auto& [chip, bit] : m_chips) {
            if (chip->isChosen())
                repeat |= uint8_t(1 << bit);
        }
        return repeat;
    }

    SizeF sizeHint() const override { return {7 * ChipSize + 6 * ChipGap, ChipSize}; }

    std::function<void()> onChanged;

protected:
    void layout() override
    {
        double x = 0;
        for (const auto& entry : m_chips) {
            entry.first->setGeometry({x, 0, ChipSize, ChipSize});
            x += ChipSize + ChipGap;
        }
    }

private:
    std::vector<std::pair<DayChip*, int>> m_chips;
};

// Padding around the editor's column; dragging empty space moves the dialog.
class EditorRoot : public View
{
public:
    explicit EditorRoot(std::unique_ptr<View> content)
        : m_content(content.get())
    {
        addChild(std::move(content));
    }

    double heightForWidth(double width) const override
    {
        return m_content->heightForWidth(width - 2 * DialogPadding) + 2 * DialogPadding;
    }

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override
    {
        const RectF bounds = localRect();
        m_content->setGeometry({DialogPadding, DialogPadding, std::max(0.0, bounds.width - 2 * DialogPadding),
                                std::max(0.0, bounds.height - 2 * DialogPadding)});
    }

    void pointerPress(const PointerEvent& event) override
    {
        if (event.button == MouseButton::Left && window())
            window()->startMove();
    }

private:
    View* m_content;
};

AlarmEditor::AlarmEditor(Window* parent, ClockStore& store, Alarm alarm, Clock now)
    : m_store(store)
    , m_alarm(std::move(alarm))
    , m_now(std::move(now))
    , m_window(std::make_unique<Window>())
    , m_preview(std::make_unique<TonePlayer>())
{
    Window* window = m_window.get();
    window->setTitle(isNew() ? tr("New Alarm") : tr("Edit Alarm"));
    window->setTransientParent(parent, true);
    window->setBackgroundColor(ColorId::Background);
    window->onCloseRequested = [this] { close(); };

    auto content = std::make_unique<ColumnLayout>();
    content->setName("editor:content");
    content->setSpacing(12);

    m_picker = content->addChild<TimePicker>(window, properdesktop::base::Locale::system().usesTwentyFourHourTime() == false);
    m_picker->setName("editor:time");
    m_picker->setLayoutHints({.fillWidth = true, .preferredHeight = m_picker->sizeHint().height});
    m_picker->setTime(m_alarm.hour, m_alarm.minute, false);
    m_picker->onChanged = [this] { updateCountdown(); };

    m_countdown = content->addChild<Label>();
    m_countdown->setName("editor:countdown");
    m_countdown->setColor(ColorId::DisabledText);
    m_countdown->setFontSize(Theme::current().smallFontSize());
    m_countdown->setTextAlignment(Label::TextAlignment::Center);
    m_countdown->setFontFeatures({{"tnum", 1}});
    m_countdown->setLayoutHints({.fillWidth = true, .margins = {0, -4, 0, 0}});

    EditorCard* card = content->addChild<EditorCard>();
    card->setLayoutHints({.fillWidth = true});

    m_days = static_cast<DayChips*>(card->addRow(tr("Repeat"), std::make_unique<DayChips>(window, m_alarm.repeat)));
    m_days->onChanged = [this] { updateCountdown(); };

    auto label = std::make_unique<TextField>();
    m_label = label.get();
    m_label->setName("editor:label");
    m_label->setFrameVisible(false);
    m_label->setRightAligned(true);
    m_label->setPlaceholder(tr("Alarm"));
    m_label->setText(m_alarm.label);
    m_label->setLayoutHints({.preferredWidth = 200});
    m_label->onAccepted = [this] { save(); };
    card->addRow(tr("Label"), std::move(label));

    std::vector<std::string> sounds;
    for (const std::string& name : clockstore::soundNames())
        sounds.push_back(tones::displayName(name));
    auto sound = std::make_unique<RowComboBox>(sounds);
    m_sound = sound.get();
    m_sound->setName("editor:sound");
    const std::vector<std::string>& names = clockstore::soundNames();
    const auto found = std::find(names.begin(), names.end(), tones::resolved(m_alarm.sound));
    m_sound->setCurrentIndex(int(found - names.begin()));
    m_sound->onActivated = [this](int index) {
        TonePlayer::Options options;
        options.periods = 1;
        m_preview->play(clockstore::soundNames()[size_t(index)], options);
    };
    card->addRow(tr("Sound"), std::move(sound));

    auto snooze = std::make_unique<Switch>();
    m_snooze = snooze.get();
    m_snooze->setName("editor:snooze");
    m_snooze->setChecked(m_alarm.snooze);
    card->addRow(tr("Snooze"), std::move(snooze));

    auto* actions = content->addChild<RowLayout>();
    actions->setSpacing(8);
    actions->setLayoutHints({.fillWidth = true, .margins = {0, 4, 0, 0}});
    if (!isNew()) {
        DialogButton* remove = actions->addChild<DialogButton>(tr("Delete Alarm"));
        remove->setName("editor:delete");
        remove->setDestructive(true);
        remove->onClicked = [this] { this->remove(); };
    }
    actions->addChild<View>()->setLayoutHints({.fillWidth = true});
    DialogButton* cancel = actions->addChild<DialogButton>(tr("Cancel"));
    cancel->setName("editor:cancel");
    cancel->onClicked = [this] { close(); };
    DialogButton* save = actions->addChild<DialogButton>(isNew() ? tr("Add") : tr("Save"), true);
    save->setName("editor:save");
    save->onClicked = [this] { this->save(); };

    window->setContentView(std::make_unique<EditorRoot>(std::move(content)));
    window->addShortcut(NoModifier, key::Escape, [this] { close(); });
    window->addShortcut(ControlModifier, key::Return, [this] { this->save(); });
    updateCountdown();
}

AlarmEditor::~AlarmEditor()
{
    m_preview->stop();
}

void AlarmEditor::show()
{
    if (!m_window)
        return;
    m_window->resize({DialogWidth, 0});
    m_window->setFitsContentHeight(true);
    m_window->show();
}

void AlarmEditor::close()
{
    m_preview->stop();
    if (!m_window)
        return;
    std::shared_ptr<Window> doomed(std::move(m_window));
    Application::instance()->loop().post([doomed] {});
    if (onClosed)
        onClosed();
}

Window* AlarmEditor::window() const
{
    return m_window.get();
}

void AlarmEditor::updateCountdown()
{
    Alarm alarm = m_alarm;
    const int64_t now = m_now() / 1000;
    alarm.hour = m_picker->hour();
    alarm.minute = m_picker->minute();
    alarm.repeat = m_days->repeat();
    alarm.enabled = true;
    alarm.armedAt = now;
    const std::optional<int64_t> ring = alarmtime::nextRing(alarm, now);
    // TRANSLATORS: under the editor's time, such as "Rings in 7 hours 20 minutes".
    m_countdown->setText(ring ? properdesktop::i18n::format(tr("Rings {when}"), {{"when", alarmtime::untilText(*ring - now)}})
                              : std::string());
}

void AlarmEditor::save()
{
    Alarm alarm = m_alarm;
    alarm.hour = m_picker->hour();
    alarm.minute = m_picker->minute();
    alarm.repeat = m_days->repeat();
    alarm.label = m_label->text();
    alarm.sound = clockstore::soundNames()[size_t(std::max(0, m_sound->currentIndex()))];
    alarm.snooze = m_snooze->isChecked();
    // Saving switches it on, from now.
    alarm.enabled = true;
    alarm.armedAt = m_now() / 1000;
    const std::string id = m_store.setAlarm(alarm);
    if (onSaved)
        onSaved(id);
    close();
}

void AlarmEditor::remove()
{
    const std::string id = m_alarm.id;
    m_store.removeAlarm(id);
    if (onDeleted)
        onDeleted(id);
    close();
}

} // namespace clockapp
