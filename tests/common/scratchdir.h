// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

// A folder of its own under the system's temporary folder, removed with it.
class ScratchDir
{
public:
    ScratchDir()
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "properdesktop-clock-XXXXXX").string();
        if (mkdtemp(pattern.data()))
            m_path = pattern;
    }

    ~ScratchDir()
    {
        std::error_code error;
        if (!m_path.empty())
            std::filesystem::remove_all(m_path, error);
    }

    ScratchDir(const ScratchDir&) = delete;
    ScratchDir& operator=(const ScratchDir&) = delete;

    const std::string& path() const { return m_path; }
    std::string file(const std::string& name) const { return m_path + "/" + name; }

private:
    std::string m_path;
};
