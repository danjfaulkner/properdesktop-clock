// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

// Renders the stopwatch and timers pages headless, still and as frames of
// their motion on a stepped frame clock, for looking at them:
// clock-timers-capture OUTDIR [SCALE].
#include "icons.h"
#include "page.h"
#include "pagecontext.h"
#include "stopwatch/stopwatchpage.h"
#include "store/store.h"
#include "style.h"
#include "timers/timermath.h"
#include "timers/timerspage.h"

#include "ui/application.h"
#include "ui/icon.h"
#include "ui/image.h"
#include "ui/test_input.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace clockapp;
using namespace properdesktop::ui;

static constexpr int64_t Millisecond = 1000000;
static constexpr int64_t Now = 1795167660000;

class SteppedTime : public TimeSource
{
public:
    Reading now() const override { return m_now; }
    void advance(int64_t ms)
    {
        m_now.bootNs += ms * Millisecond;
        m_now.wallMs += ms;
    }

private:
    Reading m_now{1000000 * Millisecond, Now, "capture"};
};

// A frame clock of its own, so motion is captured frame by frame.
static uint64_t g_frame = 1000000000000ULL;

static void save(Window& window, const std::string& file, int frames = 2)
{
    Image image;
    for (int i = 0; i < frames; ++i) {
        g_frame += 16666667;
        image = TestInput::presentFrame(window, g_frame);
    }
    if (!image.savePng(file))
        std::fprintf(stderr, "cannot write %s\n", file.c_str());
}

// Frames every 1/60 s for `ms`, one file every `every` frames.
static void film(Window& window, const std::string& prefix, int ms, int every = 2)
{
    const int frames = ms * 60 / 1000;
    for (int i = 0; i < frames; ++i) {
        g_frame += 16666667;
        Image image = TestInput::presentFrame(window, g_frame);
        if (i % every == 0) {
            char name[32];
            std::snprintf(name, sizeof name, "-%03d.png", i);
            image.savePng(prefix + name);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: clock-timers-capture OUTDIR [SCALE]\n");
        return 1;
    }
    const std::string out = argv[1];
    const double scale = argc > 2 ? std::stod(argv[2]) : 2;
    Application application(argc, argv, Application::Platform::Headless);
    for (const auto& [name, svg] : embeddedIcons())
        Icon::registerGlyph(std::string(name), std::string(svg));
    Theme::setCurrent(Theme::dark());
    std::filesystem::create_directories(out);
    std::filesystem::remove(out + "/clock.conf");
    std::filesystem::remove_all(out + "/data");

    Window window;
    window.setScale(scale);
    window.resize({style::windowWidth, style::windowHeight});
    PageContext context(window, {out + "/clock.conf", out + "/data"});
    const auto nowPoint = std::chrono::system_clock::time_point(std::chrono::milliseconds(Now));
    context.setClock([nowPoint] { return nowPoint; });
    Page* shown = nullptr;
    context.isCurrent = [&shown](const Page& page) { return &page == shown; };
    const auto show = [&](std::unique_ptr<Page> page) {
        shown = page.get();
        page->setHeaderHeight(style::headerHeight);
        window.setContentView(std::move(page));
        window.show();
        shown->activated();
        shown->entering(1);
    };

    {
        SteppedTime time;
        auto made = std::make_unique<StopwatchPage>(context, &time);
        StopwatchPage* page = made.get();
        show(std::move(made));
        save(window, out + "/stopwatch-idle.png");
        page->startOrStop();
        for (int lap : {31420, 29870, 33050, 30210}) {
            time.advance(lap);
            save(window, out + "/stopwatch-tmp.png", 1);
            page->lapOrReset();
        }
        time.advance(12340);
        save(window, out + "/stopwatch-laps.png", 30);
        // The roll of the seconds and the new lap sliding in.
        time.advance(990);
        page->lapOrReset();
        film(window, out + "/film-lap", 400);
        page->startOrStop();
        page->lapOrReset();
        save(window, out + "/stopwatch-reset.png", 40);
        std::filesystem::remove(out + "/stopwatch-tmp.png");
    }

    {
        ClockStore store(clockstore::pathIn(out + "/data"));
        std::vector<timermath::Recent> recents{{300000, "Tea", "chime"}, {1500000, {}, "dawn"},
                                               {45000, "Eggs", "pulse"}};
        timermath::saveRecents(context.settings(), recents);
        auto made = std::make_unique<TimersPage>(context, &store);
        TimersPage* page = made.get();
        show(std::move(made));
        save(window, out + "/timers-composer.png", 30);

        RunningTimer running = timermath::make(600000, "Pasta", "chime", Now - 240000);
        store.setTimer(running);
        RunningTimer paused = timermath::make(1500000, {}, "dawn", Now - 900000);
        timermath::pause(paused, Now - 300000);
        store.setTimer(paused);
        page->showComposer(false);
        save(window, out + "/timers-cards.png", 30);
        std::ofstream(out + "/cards-layout.txt") << window.layoutDump();

        page->startRecent(2);
        film(window, out + "/film-start", 700, 3);

        RunningTimer done = timermath::make(60000, "Bread", "glow", Now - 60000);
        store.setTimer(done);
        film(window, out + "/film-finish", 2400, 6);
        save(window, out + "/timers-finished.png", 10);
    }
    window.setContentView(nullptr);
    return 0;
}
