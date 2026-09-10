/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"

#include "aieditor/aiproviderclient.hpp"
#include "aieditor/resourcebudget.hpp"
#include "aieditor/securecredentialstore.hpp"

using namespace Kdenlive::AiEditor;

TEST_CASE("AI provider connection checks use read-only authenticated endpoints", "[AIEditor][Configuration]")
{
    const AiProvider provider = GENERATE(AiProvider::OpenRouter, AiProvider::OpenAI, AiProvider::Anthropic);
    const auto request = AiProviderClient::buildConnectionTestRequest(provider, QByteArrayLiteral("secret"));
    REQUIRE(request.isValid());
    REQUIRE(request.url.scheme() == QLatin1String("https"));
    REQUIRE(request.body.isEmpty());
    if (provider == AiProvider::OpenRouter) {
        REQUIRE(request.url.path() == QLatin1String("/api/v1/key"));
    } else {
        REQUIRE(request.url.path() == QLatin1String("/v1/models"));
    }
}

TEST_CASE("AI credentials have isolated secure-store targets", "[AIEditor][Configuration]")
{
    const QString openRouter = SecureCredentialStore::credentialTarget(AiProvider::OpenRouter);
    const QString openAi = SecureCredentialStore::credentialTarget(AiProvider::OpenAI);
    const QString anthropic = SecureCredentialStore::credentialTarget(AiProvider::Anthropic);
    REQUIRE(openRouter.startsWith(QStringLiteral("Firawynix.Kdenlive.AiEditor.")));
    REQUIRE(openRouter != openAi);
    REQUIRE(openAi != anthropic);
    REQUIRE_FALSE(openRouter.contains(QStringLiteral("secret"), Qt::CaseInsensitive));
}

TEST_CASE("AI resource budget discovers safe host bounds", "[AIEditor][Configuration]")
{
    REQUIRE(ResourceBudget::logicalCpuCount() >= 1);
#ifdef Q_OS_WIN
    REQUIRE(ResourceBudget::totalMemoryBytes() > 0);
    REQUIRE(SecureCredentialStore::isAvailable());
    REQUIRE(SecureCredentialStore::backendName().contains(QStringLiteral("Windows")));
#endif
}
