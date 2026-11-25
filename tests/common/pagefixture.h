// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "page.h"
#include "pagecontext.h"
#include "scratchdir.h"
#include "style.h"

#include "ui/test_driver.h"
#include "ui/window.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

// One page alone in a shown headless window of the Clock window's size,
// over a scratch clock.conf and data folder, its clock stopped at a moment
// the test chooses. Make the page with the context, then call show():
//
//     PageFixture fixture;
//     fixture.show(clockapp::makeStopwatchPage(fixture.context));
//     fixture.driver.click("stopwatch:start");
class PageFixture
{
public:
    explicit PageFixture(double scale = 1,
                         properdesktop::ui::SizeF size = {clockapp::style::windowWidth, clockapp::style::windowHeight})
        : context(window, {scratch.file("clock.conf"), scratch.file("data")})
    {
        context.setClock([this] { return now; });
        window.setScale(scale);
        window.resize(size);
    }

    ~PageFixture()
    {
        window.setContentView(nullptr);
    }

    void show(std::unique_ptr<clockapp::Page> made)
    {
        page = made.get();
        page->setHeaderHeight(clockapp::style::headerHeight);
        window.setContentView(std::move(made));
        window.show();
        context.isCurrent = [this](const clockapp::Page& shown) { return &shown == page; };
        page->activated();
        page->entering(1);
        driver.layout();
    }

    // Moves the stopped clock on and lets the pages hear the tick.
    void advance(std::chrono::system_clock::duration by)
    {
        now += by;
        context.emitTick();
        driver.layout();
    }

    ScratchDir scratch;
    properdesktop::ui::Window window;
    // 2026-11-20 09:41:00 UTC.
    std::chrono::system_clock::time_point now = std::chrono::system_clock::time_point(std::chrono::seconds(1795167660));
    clockapp::PageContext context;
    clockapp::Page* page = nullptr;
    properdesktop::ui::TestDriver driver{window, [](const std::string& message) { ADD_FAILURE() << message; }};
};
