// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "alarms/tones.h"
#include "store/store.h"

#include <base/i18n.h>

#include <algorithm>
#include <cmath>
#include <numbers>

using properdesktop::i18n::tr;

namespace clockapp::tones {

static constexpr double TwoPi = 2 * std::numbers::pi;

// A note's partials: frequency ratio, level and decay time in seconds.
struct Partial
{
    double ratio;
    double level;
    double decay;
};

struct Voice
{
    std::vector<Partial> partials;
    // Seconds of the raised-cosine rise at the note's start.
    double attack;
};

static double noteFrequency(int midi)
{
    return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
}

class Score
{
public:
    Score(double period, double tail)
        : m_period(size_t(period * SampleRate))
        , m_samples(m_period + size_t(tail * SampleRate), 0.0f)
    {
    }

    // A struck note whose partials ring out on their own decays.
    void strike(const Voice& voice, double frequency, double start, double level)
    {
        const size_t first = size_t(start * SampleRate);
        const size_t attack = std::max<size_t>(1, size_t(voice.attack * SampleRate));
        for (const Partial& partial : voice.partials) {
            const double f = frequency * partial.ratio;
            if (f >= SampleRate / 2.4)
                continue;
            const double step = TwoPi * f / SampleRate;
            const double fall = std::exp(-1.0 / (partial.decay * SampleRate));
            double envelope = 1;
            for (size_t i = 0; first + i < m_samples.size(); ++i) {
                double rise = 1;
                if (i < attack)
                    rise = 0.5 - 0.5 * std::cos(std::numbers::pi * double(i) / double(attack));
                const double value = level * partial.level * envelope * rise * std::sin(step * double(i));
                m_samples[first + i] += float(value);
                envelope *= fall;
                if (envelope < 1e-4)
                    break;
            }
        }
    }

    // A held note that swells in and out over its length, a slow shimmer on top.
    void swell(double frequency, double start, double length, double level)
    {
        const size_t first = size_t(start * SampleRate);
        const size_t count = size_t(length * SampleRate);
        const double step = TwoPi * frequency / SampleRate;
        for (size_t i = 0; i < count && first + i < m_samples.size(); ++i) {
            const double t = double(i) / double(count);
            const double shape = std::pow(std::sin(std::numbers::pi * t), 2.0);
            const double shimmer = 1 + 0.12 * std::sin(TwoPi * 5.2 * double(i) / SampleRate);
            const double phase = step * double(i);
            const double value = std::sin(phase) + 0.22 * std::sin(2 * phase) + 0.06 * std::sin(3 * phase);
            m_samples[first + i] += float(level * shape * shimmer * value);
        }
    }

    std::vector<float> finish()
    {
        for (size_t i = m_period; i < m_samples.size(); ++i)
            m_samples[i - m_period] += m_samples[i];
        m_samples.resize(m_period);
        float peak = 0;
        for (float sample : m_samples)
            peak = std::max(peak, std::abs(sample));
        if (peak > 0) {
            for (float& sample : m_samples)
                sample *= 0.9f / peak;
        }
        return std::move(m_samples);
    }

private:
    size_t m_period;
    std::vector<float> m_samples;
};

// Wooden bars: a strong fundamental, its fourth harmonic gone almost at once.
static const Voice Marimba{{{1, 1, 0.42}, {3.93, 0.28, 0.07}, {9.6, 0.06, 0.02}}, 0.004};
// A small bell: its inharmonic partials ring long and fade upward first.
static const Voice Bell{{{1, 1, 1.5}, {2.0, 0.45, 0.9}, {2.76, 0.32, 0.6}, {5.4, 0.14, 0.3}, {8.93, 0.05, 0.15}},
                        0.003};
// A rounded beep: nearly a pure tone, a breath of octave.
static const Voice Beep{{{1, 1, 0.16}, {2, 0.12, 0.08}}, 0.012};
// A plucked string, bright at first.
static const Voice Pluck{{{1, 1, 0.34}, {2, 0.4, 0.18}, {3, 0.18, 0.09}, {4, 0.08, 0.05}}, 0.002};

static std::vector<float> dawn()
{
    // C major rising, then resting on the fifth.
    Score score(2.6, 1.4);
    const int notes[] = {72, 76, 79, 84, 79};
    const double times[] = {0, 0.17, 0.34, 0.51, 0.95};
    for (size_t i = 0; i < std::size(notes); ++i)
        score.strike(Marimba, noteFrequency(notes[i]), times[i], i == 3 ? 1.0 : 0.8);
    return score.finish();
}

static std::vector<float> chime()
{
    Score score(3.2, 3.0);
    score.strike(Bell, noteFrequency(81), 0, 1.0);
    score.strike(Bell, noteFrequency(76), 0.42, 0.75);
    score.strike(Bell, noteFrequency(88), 1.5, 0.55);
    return score.finish();
}

static std::vector<float> pulse()
{
    // Three soft beeps and a breath.
    Score score(1.3, 0.6);
    for (int i = 0; i < 3; ++i)
        score.strike(Beep, noteFrequency(84), i * 0.18, 1.0);
    return score.finish();
}

static std::vector<float> ripple()
{
    // A pentatonic run falling like drops on water.
    Score score(2.2, 1.0);
    const int notes[] = {88, 86, 83, 81, 79, 76};
    for (size_t i = 0; i < std::size(notes); ++i)
        score.strike(Pluck, noteFrequency(notes[i]), double(i) * 0.105, 1.0 - double(i) * 0.08);
    return score.finish();
}

static std::vector<float> glow()
{
    // A warm major chord breathing in and out.
    Score score(4.0, 0.0);
    for (int note : {57, 61, 64, 69})
        score.swell(noteFrequency(note), 0, 4.0, note == 57 ? 0.5 : 0.35);
    return score.finish();
}

std::string resolved(const std::string& name)
{
    const std::vector<std::string>& names = clockstore::soundNames();
    return std::find(names.begin(), names.end(), name) == names.end() ? names.front() : name;
}

std::string displayName(const std::string& name)
{
    const std::string tone = resolved(name);
    if (tone == "chime")
        return tr("Chime");
    if (tone == "pulse")
        return tr("Pulse");
    if (tone == "ripple")
        return tr("Ripple");
    if (tone == "glow")
        return tr("Glow");
    return tr("Dawn");
}

std::vector<float> synthesize(const std::string& name)
{
    const std::string tone = resolved(name);
    if (tone == "chime")
        return chime();
    if (tone == "pulse")
        return pulse();
    if (tone == "ripple")
        return ripple();
    if (tone == "glow")
        return glow();
    return dawn();
}

} // namespace clockapp::tones
