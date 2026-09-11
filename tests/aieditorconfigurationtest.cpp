/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"

#include "aieditor/aiproviderclient.hpp"
#include "aieditor/localaimanager.hpp"
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

TEST_CASE("Local AI connection check stays on loopback and needs no key", "[AIEditor][Configuration][Local]")
{
    const auto request = AiProviderClient::buildConnectionTestRequest(AiProvider::Ollama, {});
    REQUIRE(request.isValid());
    REQUIRE(request.url.host() == QLatin1String("127.0.0.1"));
    REQUIRE(request.url.port() == 11434);
    REQUIRE(request.body.isEmpty());
}

TEST_CASE("Local AI model recommendation scales with system memory", "[AIEditor][Configuration][Local]")
{
    constexpr quint64 GiB = 1024ULL * 1024ULL * 1024ULL;
    REQUIRE(LocalAiManager::recommendedModelForMemory(8 * GiB) == QStringLiteral("qwen3:4b"));
    REQUIRE(LocalAiManager::recommendedModelForMemory(16 * GiB) == QStringLiteral("qwen3:8b"));
    REQUIRE(LocalAiManager::recommendedModelForMemory(32 * GiB) == QStringLiteral("qwen3:14b"));
    REQUIRE(LocalAiManager::recommendedModelForMemory(64 * GiB) == QStringLiteral("qwen3:30b"));
    REQUIRE(LocalAiManager::recommendedModelForHardware({32, 128 * GiB, QStringLiteral("AMD Radeon RX 9060 XT")}) == QStringLiteral("qwen3:8b"));
    REQUIRE(LocalAiManager::recommendedModelForHardware({8, 8 * GiB, QStringLiteral("NVIDIA GeForce GTX")}) == QStringLiteral("qwen3:4b"));
}
