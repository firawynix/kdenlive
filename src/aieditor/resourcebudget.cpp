/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "resourcebudget.hpp"

#include "kdenlivesettings.h"

#include <QThread>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_UNIX)
#include <unistd.h>
#endif

namespace Kdenlive {
namespace AiEditor {

int ResourceBudget::logicalCpuCount()
{
    return qMax(1, QThread::idealThreadCount());
}

quint64 ResourceBudget::totalMemoryBytes()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? quint64(status.ullTotalPhys) : 0;
#elif defined(Q_OS_UNIX)
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGE_SIZE);
    return pages > 0 && pageSize > 0 ? quint64(pages) * quint64(pageSize) : 0;
#else
    return 0;
#endif
}

bool ResourceBudget::cudaHardwareLikelyAvailable()
{
#ifdef Q_OS_WIN
    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);
    for (DWORD index = 0; EnumDisplayDevicesW(nullptr, index, &device, 0); ++index) {
        const QString description = QString::fromWCharArray(device.DeviceString);
        if (description.contains(QStringLiteral("NVIDIA"), Qt::CaseInsensitive)) {
            return true;
        }
        device = {};
        device.cb = sizeof(device);
    }
#endif
    return false;
}

bool ResourceBudget::gpuHardwareLikelyAvailable()
{
#ifdef Q_OS_WIN
    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);
    for (DWORD index = 0; EnumDisplayDevicesW(nullptr, index, &device, 0); ++index) {
        const QString description = QString::fromWCharArray(device.DeviceString);
        if (!description.contains(QStringLiteral("Microsoft"), Qt::CaseInsensitive) &&
            (description.contains(QStringLiteral("AMD"), Qt::CaseInsensitive) || description.contains(QStringLiteral("Radeon"), Qt::CaseInsensitive) ||
             description.contains(QStringLiteral("NVIDIA"), Qt::CaseInsensitive) || description.contains(QStringLiteral("Intel"), Qt::CaseInsensitive) ||
             description.contains(QStringLiteral("Arc"), Qt::CaseInsensitive))) {
            return true;
        }
        device = {};
        device.cb = sizeof(device);
    }
#endif
    return cudaHardwareLikelyAvailable();
}

ResourceBudget ResourceBudget::fromSettings()
{
    ResourceBudget result;
    result.cpuPercent = qBound(10, KdenliveSettings::aiCpuPercent(), 100);
    result.gpuPercent = qBound(0, KdenliveSettings::aiGpuPercent(), 100);
    result.memoryPercent = qBound(10, KdenliveSettings::aiMemoryPercent(), 100);
    const int automaticThreads = qMax(1, qRound(double(logicalCpuCount()) * double(result.cpuPercent) / 100.0));
    result.cpuThreads = KdenliveSettings::aiCpuThreads() > 0 ? qBound(1, KdenliveSettings::aiCpuThreads(), logicalCpuCount()) : automaticThreads;
    const quint64 totalMemory = totalMemoryBytes();
    result.memoryLimitBytes = totalMemory == 0 ? 0 : totalMemory * quint64(result.memoryPercent) / 100;
    const QString configuredDevice = KdenliveSettings::aiProcessingDevice();
    if (result.gpuPercent == 0 || configuredDevice == QLatin1String("cpu")) {
        result.device = QStringLiteral("cpu");
    } else if (configuredDevice == QLatin1String("cuda") && cudaHardwareLikelyAvailable()) {
        result.device = QStringLiteral("cuda");
    } else if (configuredDevice == QLatin1String("auto") && cudaHardwareLikelyAvailable()) {
        result.device = QStringLiteral("cuda");
    } else if ((configuredDevice == QLatin1String("auto") || configuredDevice == QLatin1String("gpu")) && gpuHardwareLikelyAvailable()) {
        result.device = QStringLiteral("gpu");
    } else {
        result.device = QStringLiteral("cpu");
    }
    return result;
}

quintptr ResourceBudget::applyToProcess(qint64 processId, const ResourceBudget &budget)
{
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(processId));
    if (!process) {
        return 0;
    }
    const int usableThreads = qBound(1, budget.cpuThreads, qMin(logicalCpuCount(), int(sizeof(DWORD_PTR) * 8)));
    DWORD_PTR affinityMask = usableThreads == int(sizeof(DWORD_PTR) * 8) ? ~DWORD_PTR(0) : (DWORD_PTR(1) << usableThreads) - 1;
    SetProcessAffinityMask(process, affinityMask);

    HANDLE job = nullptr;
    if (budget.memoryLimitBytes > 0) {
        job = CreateJobObjectW(nullptr, nullptr);
        if (job) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
            limits.ProcessMemoryLimit = SIZE_T(budget.memoryLimitBytes);
            if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) || !AssignProcessToJobObject(job, process)) {
                CloseHandle(job);
                job = nullptr;
            }
        }
    }
    CloseHandle(process);
    return reinterpret_cast<quintptr>(job);
#else
    Q_UNUSED(processId)
    Q_UNUSED(budget)
    return 0;
#endif
}

void ResourceBudget::releaseProcessBudget(quintptr handle)
{
#ifdef Q_OS_WIN
    if (handle != 0) {
        CloseHandle(reinterpret_cast<HANDLE>(handle));
    }
#else
    Q_UNUSED(handle)
#endif
}

} // namespace AiEditor
} // namespace Kdenlive
