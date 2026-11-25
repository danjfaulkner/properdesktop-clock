// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "page.h"
#include "store/store.h"

#include <base/signal.h>

#include <memory>
#include <string>

namespace properdesktop::ui {
class HeaderBackdrop;
class ScrollView;
}

namespace clockapp {

class AlarmEditor;
class AlarmList;
class AlarmsEmptyState;
class PageContext;
class Spring;

// The Alarms tab: a card for each alarm, soonest time of day first, or a
// drawn clock face asking for the first one.
class AlarmsPage : public Page
{
public:
    explicit AlarmsPage(PageContext& context);
    ~AlarmsPage() override;

    std::vector<HeaderAction> headerActions() override;
    std::string newItemText() const override;
    void newItem() override;
    void activated() override;

    // The editor over the window, for an alarm or a new one.
    void edit(const std::string& id);
    AlarmEditor* editor() const { return m_editor.get(); }

protected:
    void layout() override;
    void paint(properdesktop::ui::Painter& painter) override;

private:
    int64_t nowMs() const;
    void sync(bool animate);
    void toggled(const std::string& id, bool on);
    void openEditor(Alarm alarm);

    PageContext& m_context;
    ClockStore& m_store;
    properdesktop::ui::ScrollView* m_scroll;
    AlarmList* m_list;
    properdesktop::ui::HeaderBackdrop* m_backdrop;
    AlarmsEmptyState* m_empty;
    std::unique_ptr<Spring> m_emptyFade;
    std::unique_ptr<AlarmEditor> m_editor;
    properdesktop::base::Subscription m_changed;
    properdesktop::base::Subscription m_tick;
};

} // namespace clockapp
