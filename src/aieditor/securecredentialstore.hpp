/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "aiproviderclient.hpp"

#include <QByteArray>
#include <QString>

namespace Kdenlive {
namespace AiEditor {

class SecureCredentialStore
{
public:
    static QString credentialTarget(AiProvider provider);
    static QString backendName();
    static bool isAvailable();
    static QByteArray read(AiProvider provider, QString *error = nullptr);
    static bool write(AiProvider provider, const QByteArray &apiKey, QString *error = nullptr);
    static bool remove(AiProvider provider, QString *error = nullptr);
};

} // namespace AiEditor
} // namespace Kdenlive
