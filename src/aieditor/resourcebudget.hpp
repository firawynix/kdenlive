/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QString>
#include <QtGlobal>

namespace Kdenlive {
namespace AiEditor {

struct ResourceBudget
{
    int percent{80};
    int cpuThreads{1};
    quint64 memoryLimitBytes{0};
    QString device{QStringLiteral("cpu")};

    static int logicalCpuCount();
    static quint64 totalMemoryBytes();
    static bool cudaHardwareLikelyAvailable();
    static ResourceBudget fromSettings();

    // Returns an opaque native job handle when the platform supports a memory
    // budget. The caller must pass it to releaseProcessBudget after the child
    // process exits.
    static quintptr applyToProcess(qint64 processId, const ResourceBudget &budget);
    static void releaseProcessBudget(quintptr handle);
};

} // namespace AiEditor
} // namespace Kdenlive
