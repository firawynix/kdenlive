/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"

#include "aieditor/aiproviderclient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Kdenlive::AiEditor;

namespace {
const QByteArray ValidPlan =
    R"({"version":1,"operations":[{"type":"retime_range","start_frame":250,"end_frame":3250,"target_duration_frames":1000,"preserve_pitch":true}]})";

QByteArray chatResponse(const QByteArray &plan)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("choices"),
                                      QJsonArray{QJsonObject{{QStringLiteral("message"), QJsonObject{{QStringLiteral("content"), QString::fromUtf8(plan)}}}}}}})
        .toJson(QJsonDocument::Compact);
}

QByteArray anthropicResponse(const QByteArray &plan)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("content"), QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                                                                        {QStringLiteral("text"), QString::fromUtf8(plan)}}}}})
        .toJson(QJsonDocument::Compact);
}

QByteArray ollamaResponse(const QByteArray &plan)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("message"),
                                      QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")}, {QStringLiteral("content"), QString::fromUtf8(plan)}}},
                                     {QStringLiteral("done"), true}})
        .toJson(QJsonDocument::Compact);
}
} // namespace

TEST_CASE("AI provider request construction", "[AIEditor][Provider]")
{
    const AiProvider provider = GENERATE(AiProvider::OpenRouter, AiProvider::OpenAI, AiProvider::Anthropic);
    const auto request = AiProviderClient::buildRequest(provider, AiProviderClient::defaultModel(provider), QByteArrayLiteral("secret"),
                                                        QStringLiteral("Make 00:10 to 02:10 last 40 seconds"), 5000, 25.0);
    INFO(request.error.toStdString());
    REQUIRE(request.isValid());
    REQUIRE(request.url.scheme() == QLatin1String("https"));
    REQUIRE(QJsonDocument::fromJson(request.body).isObject());

    const QJsonObject body = QJsonDocument::fromJson(request.body).object();
    REQUIRE(body.value(QStringLiteral("model")).toString() == AiProviderClient::defaultModel(provider));
    if (provider == AiProvider::Anthropic) {
        REQUIRE(request.url.host() == QLatin1String("api.anthropic.com"));
        REQUIRE(body.contains(QStringLiteral("output_config")));
        REQUIRE(body.value(QStringLiteral("max_tokens")).toInt() == 16384);
    } else {
        REQUIRE(body.contains(QStringLiteral("response_format")));
        REQUIRE(body.value(QStringLiteral("messages")).toArray().size() == 2);
        if (provider == AiProvider::OpenRouter) {
            REQUIRE(body.value(QStringLiteral("max_tokens")).toInt() == 16384);
            REQUIRE(body.value(QStringLiteral("provider")).toObject().value(QStringLiteral("require_parameters")).toBool());
        } else {
            REQUIRE(body.value(QStringLiteral("max_completion_tokens")).toInt() == 16384);
        }
    }
}

TEST_CASE("AI provider sends transcript text without media", "[AIEditor][Provider]")
{
    const auto request =
        AiProviderClient::buildRequest(AiProvider::OpenRouter, QStringLiteral("test/model"), QByteArrayLiteral("secret"),
                                       QStringLiteral("Mute off-topic dialogue"), 5000, 25.0, QStringLiteral("[25-50] conversa sobre trabalho"));
    REQUIRE(request.isValid());
    const QJsonObject body = QJsonDocument::fromJson(request.body).object();
    const QString message = body.value(QStringLiteral("messages")).toArray().at(1).toObject().value(QStringLiteral("content")).toString();
    REQUIRE(message.contains(QStringLiteral("[25-50] conversa sobre trabalho")));
    REQUIRE_FALSE(message.contains(QStringLiteral("file://")));
    REQUIRE_FALSE(message.contains(QStringLiteral("data:")));
}

TEST_CASE("AI provider builds and validates prompt suggestions", "[AIEditor][Provider][Prompts]")
{
    const auto request =
        AiProviderClient::buildPromptSuggestionRequest(AiProvider::Ollama, QStringLiteral("qwen3:8b"), {}, QStringLiteral("[0-100] reunião sobre Salesforce"));
    INFO(request.error.toStdString());
    REQUIRE(request.isValid());
    REQUIRE(request.url == QUrl(QStringLiteral("http://127.0.0.1:11434/api/chat")));
    const QJsonObject body = QJsonDocument::fromJson(request.body).object();
    REQUIRE(body.value(QStringLiteral("format")).toObject().value(QStringLiteral("properties")).toObject().contains(QStringLiteral("suggestions")));
    REQUIRE(
        body.value(QStringLiteral("messages")).toArray().at(1).toObject().value(QStringLiteral("content")).toString().contains(QStringLiteral("Salesforce")));

    const QByteArray suggestions = R"({"suggestions":[{"title":"Foco no trabalho","prompt":"Silencie conversas fora do tema Salesforce."}]})";
    const auto parsed = AiProviderClient::parsePromptSuggestionResponse(AiProvider::Ollama, ollamaResponse(suggestions));
    INFO(parsed.error.toStdString());
    REQUIRE(parsed.isValid());
    REQUIRE(parsed.suggestions.size() == 1);
    REQUIRE(parsed.suggestions.constFirst().title == QStringLiteral("Foco no trabalho"));
    REQUIRE(parsed.suggestions.constFirst().prompt.contains(QStringLiteral("Salesforce")));
}

TEST_CASE("AI prompt suggestions reject malformed content", "[AIEditor][Provider][Prompts]")
{
    const auto parsed = AiProviderClient::parsePromptSuggestionResponse(AiProvider::OpenRouter, chatResponse(QByteArrayLiteral(R"({"suggestions":[]})")));
    REQUIRE_FALSE(parsed.isValid());
    REQUIRE_FALSE(parsed.error.isEmpty());
}

TEST_CASE("AI provider response validation", "[AIEditor][Provider]")
{
    SECTION("accepts OpenAI-compatible structured output")
    {
        const auto result = AiProviderClient::completeResponse(AiProvider::OpenRouter, 200, QNetworkReply::NoError, false, chatResponse(ValidPlan));
        INFO(result.error.toStdString());
        REQUIRE(result.isValid());
        REQUIRE(result.plan.operations.constFirst().retimeRange.speedMultiplier() == Approx(3.0));
    }

    SECTION("accepts Anthropic structured output")
    {
        const auto result = AiProviderClient::completeResponse(AiProvider::Anthropic, 200, QNetworkReply::NoError, false, anthropicResponse(ValidPlan));
        INFO(result.error.toStdString());
        REQUIRE(result.isValid());
    }

    SECTION("reports timeout")
    {
        const auto result = AiProviderClient::completeResponse(AiProvider::OpenAI, 0, QNetworkReply::TimeoutError, false, {});
        REQUIRE_FALSE(result.isValid());
        REQUIRE(result.error.contains(QStringLiteral("timed out")));
    }

    SECTION("reports cancellation separately")
    {
        const auto result = AiProviderClient::completeResponse(AiProvider::OpenAI, 0, QNetworkReply::OperationCanceledError, true, {});
        REQUIRE_FALSE(result.isValid());
        REQUIRE(result.cancelled);
    }

    SECTION("includes provider HTTP error")
    {
        const QByteArray payload = R"({"error":{"message":"invalid key"}})";
        const auto result = AiProviderClient::completeResponse(AiProvider::OpenRouter, 401, QNetworkReply::NoError, false, payload);
        REQUIRE_FALSE(result.isValid());
        REQUIRE(result.error.contains(QStringLiteral("API key")));
        REQUIRE(result.error.contains(QStringLiteral("rejected")));
    }

    SECTION("prefers an HTTP provider error over Qt's generic network label")
    {
        const QByteArray payload = R"({"error":{"message":"insufficient credits"}})";
        const auto result = AiProviderClient::completeResponse(AiProvider::OpenRouter, 402, QNetworkReply::UnknownContentError, false, payload);
        REQUIRE_FALSE(result.isValid());
        REQUIRE(result.error.contains(QStringLiteral("credits")));
        REQUIRE(result.error.contains(QStringLiteral("local model")));
    }

    SECTION("rejects unsafe plan")
    {
        const auto result = AiProviderClient::completeResponse(AiProvider::OpenAI, 200, QNetworkReply::NoError, false,
                                                               chatResponse(QByteArrayLiteral(R"({"version":9,"operations":[]})")));
        REQUIRE_FALSE(result.isValid());
        REQUIRE(result.error.contains(QStringLiteral("unsafe edit plan")));
    }

    SECTION("reports output token exhaustion")
    {
        const QByteArray payload = R"({"choices":[{"message":{"content":""},"finish_reason":"length"}]})";
        const auto result = AiProviderClient::completeResponse(AiProvider::OpenRouter, 200, QNetworkReply::NoError, false, payload);
        REQUIRE_FALSE(result.isValid());
        REQUIRE(result.outputLimitReached);
        REQUIRE(result.error.contains(QStringLiteral("token limit")));
    }

    SECTION("classifies a truncated partial plan as output exhaustion")
    {
        const QByteArray payload = R"({"choices":[{"message":{"content":"{\"version\":1"},"finish_reason":"length"}]})";
        const auto result = AiProviderClient::completeResponse(AiProvider::OpenRouter, 200, QNetworkReply::NoError, false, payload);
        REQUIRE_FALSE(result.isValid());
        REQUIRE(result.outputLimitReached);
        REQUIRE(result.error.contains(QStringLiteral("complete edit plan")));
    }

    SECTION("allows an empty plan for a transcript chunk")
    {
        const auto result = AiProviderClient::completeResponse(AiProvider::OpenRouter, 200, QNetworkReply::NoError, false,
                                                               chatResponse(QByteArrayLiteral(R"({"version":1,"operations":[]})")), true);
        REQUIRE(result.isValid());
        REQUIRE(result.plan.operations.isEmpty());
    }
}

TEST_CASE("AI provider rejects missing credentials", "[AIEditor][Provider]")
{
    const auto result = AiProviderClient::buildRequest(AiProvider::OpenAI, QStringLiteral("gpt-4o-mini"), {}, QStringLiteral("edit"), 100, 25.0);
    REQUIRE_FALSE(result.isValid());
    REQUIRE(result.error.contains(QStringLiteral("OPENAI_API_KEY")));
}

TEST_CASE("Local Ollama provider is loopback-only and needs no credential", "[AIEditor][Provider][Local]")
{
    const auto request = AiProviderClient::buildRequest(AiProvider::Ollama, QStringLiteral("qwen3:8b"), {}, QStringLiteral("edit"), 100, 25.0);
    INFO(request.error.toStdString());
    REQUIRE(request.isValid());
    REQUIRE(request.url == QUrl(QStringLiteral("http://127.0.0.1:11434/api/chat")));
    const QJsonObject body = QJsonDocument::fromJson(request.body).object();
    REQUIRE(body.value(QStringLiteral("stream")).toBool() == false);
    REQUIRE(body.value(QStringLiteral("think")).toBool() == false);
    REQUIRE(body.value(QStringLiteral("format")).toObject().value(QStringLiteral("type")).toString() == QLatin1String("object"));
    REQUIRE(request.headers.size() == 1);

    const auto result = AiProviderClient::completeResponse(AiProvider::Ollama, 200, QNetworkReply::NoError, false, ollamaResponse(ValidPlan));
    INFO(result.error.toStdString());
    REQUIRE(result.isValid());
}

TEST_CASE("Local Ollama chunk responses safely normalize reversed or empty ranges", "[AIEditor][Provider][Local]")
{
    const QByteArray plan =
        R"({"version":1,"operations":[{"type":"mute_range","start_frame":300,"end_frame":200},{"type":"mute_range","start_frame":400,"end_frame":400}]})";
    const auto result = AiProviderClient::completeResponse(AiProvider::Ollama, 200, QNetworkReply::NoError, false, ollamaResponse(plan), true);
    INFO(result.error.toStdString());
    REQUIRE(result.isValid());
    REQUIRE(result.plan.operations.size() == 1);
    REQUIRE(result.plan.operations.constFirst().muteRange.startFrame == 200);
    REQUIRE(result.plan.operations.constFirst().muteRange.endFrame == 300);
}

TEST_CASE("Local Ollama chunk responses consolidate overlapping operations", "[AIEditor][Provider][Local]")
{
    const QByteArray plan = R"({"version":1,"operations":[
        {"type":"mute_range","start_frame":100,"end_frame":200},
        {"type":"mute_range","start_frame":150,"end_frame":250},
        {"type":"retime_range","start_frame":120,"end_frame":180,"target_duration_frames":10,"preserve_pitch":true},
        {"type":"retime_range","start_frame":300,"end_frame":500,"target_duration_frames":20,"preserve_pitch":true},
        {"type":"retime_range","start_frame":450,"end_frame":550,"target_duration_frames":10,"preserve_pitch":true}
    ]})";
    const auto result = AiProviderClient::completeResponse(AiProvider::Ollama, 200, QNetworkReply::NoError, false, ollamaResponse(plan), true);
    INFO(result.error.toStdString());
    REQUIRE(result.isValid());
    REQUIRE(result.plan.operations.size() == 2);
    REQUIRE(result.plan.operations.at(0).type == EditOperationType::MuteRange);
    REQUIRE(result.plan.operations.at(0).muteRange.startFrame == 100);
    REQUIRE(result.plan.operations.at(0).muteRange.endFrame == 250);
    REQUIRE(result.plan.operations.at(1).type == EditOperationType::RetimeRange);
    REQUIRE(result.plan.operations.at(1).retimeRange.startFrame == 300);
    REQUIRE(result.plan.operations.at(1).retimeRange.endFrame == 500);

    const auto combined = parseEditPlan(AiProviderClient::normalizeLocalPlan(plan), true);
    REQUIRE(combined.isValid());
    REQUIRE(combined.plan.operations.size() == 2);
}
