/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "securecredentialstore.hpp"

#ifdef Q_OS_WIN
// wincred.h depends on Windows base types and must follow windows.h with MinGW.
// clang-format off
#include <windows.h>
#include <wincred.h>
// clang-format on
#endif

namespace Kdenlive {
namespace AiEditor {

QString SecureCredentialStore::credentialTarget(AiProvider provider)
{
    QString suffix;
    switch (provider) {
    case AiProvider::OpenRouter:
        suffix = QStringLiteral("OpenRouter");
        break;
    case AiProvider::OpenAI:
        suffix = QStringLiteral("OpenAI");
        break;
    case AiProvider::Anthropic:
        suffix = QStringLiteral("Anthropic");
        break;
    case AiProvider::Ollama:
        return {};
    }
    return QStringLiteral("Firawynix.Kdenlive.AiEditor.%1").arg(suffix);
}

QString SecureCredentialStore::backendName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("Windows Credential Manager");
#else
    return QStringLiteral("environment variables");
#endif
}

bool SecureCredentialStore::isAvailable()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QByteArray SecureCredentialStore::read(AiProvider provider, QString *error)
{
    if (error) {
        error->clear();
    }
#ifdef Q_OS_WIN
    const QString target = credentialTarget(provider);
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0, &credential)) {
        if (GetLastError() != ERROR_NOT_FOUND && error) {
            *error = QStringLiteral("Windows could not read the saved credential (error %1).").arg(GetLastError());
        }
        return {};
    }
    const QByteArray result(reinterpret_cast<const char *>(credential->CredentialBlob), int(credential->CredentialBlobSize));
    CredFree(credential);
    return result;
#else
    Q_UNUSED(provider)
    return {};
#endif
}

bool SecureCredentialStore::write(AiProvider provider, const QByteArray &apiKey, QString *error)
{
    if (error) {
        error->clear();
    }
    const QByteArray cleaned = apiKey.trimmed();
    if (cleaned.isEmpty() || cleaned.size() > 2048) {
        if (error) {
            *error = QStringLiteral("Enter a valid API key no longer than 2048 bytes.");
        }
        return false;
    }
#ifdef Q_OS_WIN
    const QString target = credentialTarget(provider);
    const QString username = AiProviderClient::displayName(provider);
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(target.utf16()));
    credential.CredentialBlobSize = DWORD(cleaned.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(cleaned.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(username.utf16()));
    if (!CredWriteW(&credential, 0)) {
        if (error) {
            *error = QStringLiteral("Windows could not save the credential (error %1).").arg(GetLastError());
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(provider)
    if (error) {
        *error = QStringLiteral("Secure in-app credential storage is not available on this platform; use the provider environment variable.");
    }
    return false;
#endif
}

bool SecureCredentialStore::remove(AiProvider provider, QString *error)
{
    if (error) {
        error->clear();
    }
#ifdef Q_OS_WIN
    const QString target = credentialTarget(provider);
    if (CredDeleteW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("Windows could not remove the credential (error %1).").arg(GetLastError());
    }
    return false;
#else
    Q_UNUSED(provider)
    if (error) {
        *error = QStringLiteral("There is no saved in-app credential on this platform.");
    }
    return false;
#endif
}

} // namespace AiEditor
} // namespace Kdenlive
