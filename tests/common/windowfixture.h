// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "clockwindow.h"
#include "scratchdir.h"
#include "style.h"
#include "ui/segmented_control.h"
#include "headerbar.h"

#include "ui/test_driver.h"
#include "ui/window.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

// The whole Clock window, shown headless over a scratch clock.conf and data
// folder, and worked through the driver.
class WindowFixture
{
public:
    explicit WindowFixture(double scale = 1,
                           properdesktop::ui::SizeF size = {clockapp::style::windowWidth, clockapp::style::windowHeight},
                           const ScratchDir* shared = nullptr)
        : m_dir(shared ? shared : &m_scratch)
        , window(std::make_unique<clockapp::ClockWindow>(clockapp::ClockWindow::Options{
              .paths = {m_dir->file("clock.conf"), m_dir->file("data")}, .bus = nullptr, .restoresSize = false}))
    {
        window->window().setScale(scale);
        window->window().resize(size);
        window->show();
        driver.layout();
    }

    // Runs frames until the tab switch and the pages are at rest.
    bool settle()
    {
        return properdesktop::ui::TestDriver::waitFor([this] {
            driver.layout();
            return !window->isSwitching() && !window->headerBar()->tabSwitch()->isGliding();
        });
    }

private:
    ScratchDir m_scratch;
    const ScratchDir* m_dir;

public:
    std::unique_ptr<clockapp::ClockWindow> window;
    properdesktop::ui::TestDriver driver{window->window(), [](const std::string& message) { ADD_FAILURE() << message; }};
};
