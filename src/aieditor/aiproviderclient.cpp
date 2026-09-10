/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "aiproviderclient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>

namespace Kdenlive {
namespace AiEditor {

namespace {
QJsonObject editPlanSchema()
{
    const QJsonObject operationProperties{
        {QStringLiteral("type"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                              {QStringLiteral("enum"), QJsonArray{QStringLiteral("retime_range")}}}},
        {QStringLiteral("start_frame"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("minimum"), 0}}},
        {QStringLiteral("end_frame"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("minimum"), 1}}},
        {QStringLiteral("target_duration_frames"),
         QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("minimum"), 1}}},
        {QStringLiteral("preserve_pitch"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
    };
    const QJsonObject operation{{QStringLiteral("type"), QStringLiteral("object")},
                                {QStringLiteral("additionalProperties"), false},
                                {QStringLiteral("properties"), operationProperties},
                                {QStringLiteral("required"),
                                 QJsonArray{QStringLiteral("type"), QStringLiteral("start_frame"), QStringLiteral("end_frame"),
                                            QStringLiteral("target_duration_frames"), QStringLiteral("preserve_pitch")}}};
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                       {QStringLiteral("additionalProperties"), false},
                       {QStringLiteral("properties"),
                        QJsonObject{{QStringLiteral("version"),
                                     QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")},
                                                 {QStringLiteral("enum"), QJsonArray{1}}}},
                                    {QStringLiteral("operations"),
                                     QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                                                 {QStringLiteral("minItems"), 1},
                                                 {QStringLiteral("maxItems"), 1},
                                                 {QStringLiteral("items"), operation}}}}},
                       {QStringLiteral("required"), QJsonArray{QStringLiteral("version"), QStringLiteral("operations")}}};
}

QString systemPrompt()
{
    return QStringLiteral(
        "You translate a user's Kdenlive editing instruction into one safe edit plan. Output only the requested JSON schema. "
        "The only supported operation is retime_range. Interpret timecodes using the supplied FPS, round to the nearest frame, make end_frame exclusive, "
        "and use target_duration_frames for the requested final duration. Never invent edits that the user did not request.");
}

QString userMessage(const QString &prompt, int timelineFrames, double fps)
{
    return QStringLiteral("Project context: FPS=%1; timeline_duration_frames=%2.\nUser instruction: %3")
        .arg(QString::number(fps, 'g', 12))
        .arg(timelineFrames)
        .arg(prompt);
}

QByteArray extractApiError(const QByteArray &payload)
{
    const QJsonObject root = QJsonDocument::fromJson(payload).object();
    const QJsonValue error = root.value(QStringLiteral("error"));
    if (error.isObject()) {
        return error.toObject().value(QStringLiteral("message")).toString().toUtf8();
    }
    if (error.isString()) {
        return error.toString().toUtf8();
    }
    return {};
}
} // namespace

bool BuiltAiRequest::isValid() const
{
    return error.isEmpty();
}

bool AiProviderResponse::isValid() const
{
    return error.isEmpty() && !cancelled;
}

AiProviderClient::AiProviderClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

QString AiProviderClient::displayName(AiProvider provider)
{
    switch (provider) {
    case AiProvider::OpenRouter:
        return QStringLiteral("OpenRouter");
    case AiProvider::OpenAI:
        return QStringLiteral("OpenAI");
    case AiProvider::Anthropic:
        return QStringLiteral("Anthropic Claude");
    }
    return {};
}

QString AiProviderClient::environmentVariable(AiProvider provider)
{
    switch (provider) {
    case AiProvider::OpenRouter:
        return QStringLiteral("OPENROUTER_API_KEY");
    case AiProvider::OpenAI:
        return QStringLiteral("OPENAI_API_KEY");
    case AiProvider::Anthropic:
        return QStringLiteral("ANTHROPIC_API_KEY");
    }
    return {};
}

QString AiProviderClient::defaultModel(AiProvider provider)
{
    switch (provider) {
    case AiProvider::OpenRouter:
        return QStringLiteral("openai/gpt-4o-mini");
    case AiProvider::OpenAI:
        return QStringLiteral("gpt-4o-mini");
    case AiProvider::Anthropic:
        return QStringLiteral("claude-sonnet-4-5");
    }
    return {};
}

BuiltAiRequest AiProviderClient::buildRequest(AiProvider provider, const QString &model, const QByteArray &apiKey, const QString &prompt, int timelineFrames,
                                              double fps)
{
    BuiltAiRequest result;
    if (model.trimmed().isEmpty()) {
        result.error = QStringLiteral("Choose a model before sending the request.");
        return result;
    }
    if (apiKey.trimmed().isEmpty()) {
        result.error = QStringLiteral("The API key is missing. Set %1 and restart Kdenlive.").arg(environmentVariable(provider));
        return result;
    }
    if (prompt.trimmed().isEmpty()) {
        result.error = QStringLiteral("Describe the edit you want before sending the request.");
        return result;
    }
    if (timelineFrames < 1 || fps <= 0.0) {
        result.error = QStringLiteral("The active project has invalid timeline timing information.");
        return result;
    }

    result.headers.push_back({QByteArrayLiteral("Content-Type"), QByteArrayLiteral("application/json")});
    const QJsonObject schema = editPlanSchema();
    const QJsonArray messages{
        QJsonObject{{QStringLiteral("role"), QStringLiteral("system")}, {QStringLiteral("content"), systemPrompt()}},
        QJsonObject{{QStringLiteral("role"), QStringLiteral("user")}, {QStringLiteral("content"), userMessage(prompt.trimmed(), timelineFrames, fps)}},
    };

    QJsonObject body;
    body.insert(QStringLiteral("model"), model.trimmed());
    if (provider == AiProvider::Anthropic) {
        result.url = QUrl(QStringLiteral("https://api.anthropic.com/v1/messages"));
        result.headers.push_back({QByteArrayLiteral("x-api-key"), apiKey});
        result.headers.push_back({QByteArrayLiteral("anthropic-version"), QByteArrayLiteral("2023-06-01")});
        body.insert(QStringLiteral("max_tokens"), 2048);
        body.insert(QStringLiteral("system"), systemPrompt());
        body.insert(QStringLiteral("messages"), QJsonArray{messages.at(1)});
        body.insert(QStringLiteral("output_config"),
                    QJsonObject{{QStringLiteral("format"),
                                 QJsonObject{{QStringLiteral("type"), QStringLiteral("json_schema")}, {QStringLiteral("schema"), schema}}}});
    } else {
        result.url = provider == AiProvider::OpenRouter ? QUrl(QStringLiteral("https://openrouter.ai/api/v1/chat/completions"))
                                                       : QUrl(QStringLiteral("https://api.openai.com/v1/chat/completions"));
        result.headers.push_back({QByteArrayLiteral("Authorization"), QByteArrayLiteral("Bearer ") + apiKey});
        body.insert(QStringLiteral("messages"), messages);
        body.insert(QStringLiteral("response_format"),
                    QJsonObject{{QStringLiteral("type"), QStringLiteral("json_schema")},
                                {QStringLiteral("json_schema"),
                                 QJsonObject{{QStringLiteral("name"), QStringLiteral("kdenlive_edit_plan")},
                                             {QStringLiteral("strict"), true},
                                             {QStringLiteral("schema"), schema}}}});
    }
    result.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return result;
}

AiProviderResponse AiProviderClient::parseSuccessfulResponse(AiProvider provider, const QByteArray &payload)
{
    AiProviderResponse result;
    QJsonParseError jsonError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = QStringLiteral("The AI provider returned an invalid JSON response.");
        return result;
    }

    const QJsonObject root = document.object();
    QString content;
    if (provider == AiProvider::Anthropic) {
        const QJsonArray blocks = root.value(QStringLiteral("content")).toArray();
        for (const QJsonValue &blockValue : blocks) {
            const QJsonObject block = blockValue.toObject();
            if (block.value(QStringLiteral("type")).toString() == QLatin1String("text")) {
                content = block.value(QStringLiteral("text")).toString();
                break;
            }
        }
    } else {
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (!choices.isEmpty()) {
            content = choices.at(0).toObject().value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
        }
    }
    if (content.isEmpty()) {
        result.error = QStringLiteral("The AI provider response did not contain an edit plan.");
        return result;
    }

    result.planJson = content.toUtf8();
    const auto parsed = parseEditPlan(result.planJson);
    if (!parsed.isValid()) {
        result.planJson.clear();
        result.error = QStringLiteral("The AI provider returned an unsafe edit plan: %1").arg(parsed.error);
        return result;
    }
    result.plan = parsed.plan;
    return result;
}

AiProviderResponse AiProviderClient::completeResponse(AiProvider provider, int httpStatus, QNetworkReply::NetworkError networkError, bool wasCancelled,
                                                      const QByteArray &payload)
{
    AiProviderResponse result;
    if (wasCancelled || networkError == QNetworkReply::OperationCanceledError) {
        result.cancelled = true;
        result.error = QStringLiteral("The AI request was cancelled.");
        return result;
    }
    if (networkError == QNetworkReply::TimeoutError) {
        result.error = QStringLiteral("The AI request timed out. Check the connection and try again.");
        return result;
    }
    if (networkError != QNetworkReply::NoError) {
        result.error = QStringLiteral("The AI request failed because of a network error.");
        return result;
    }
    if (httpStatus < 200 || httpStatus >= 300) {
        const QByteArray apiMessage = extractApiError(payload);
        result.error = apiMessage.isEmpty() ? QStringLiteral("The AI provider returned HTTP %1.").arg(httpStatus)
                                            : QStringLiteral("The AI provider returned HTTP %1: %2").arg(httpStatus).arg(QString::fromUtf8(apiMessage));
        return result;
    }
    return parseSuccessfulResponse(provider, payload);
}

bool AiProviderClient::isBusy() const
{
    return m_reply != nullptr;
}

void AiProviderClient::requestPlan(AiProvider provider, const QString &model, const QByteArray &apiKey, const QString &prompt, int timelineFrames, double fps)
{
    if (isBusy()) {
        Q_EMIT errorOccurred(QStringLiteral("An AI request is already running."));
        return;
    }
    const BuiltAiRequest built = buildRequest(provider, model, apiKey, prompt, timelineFrames, fps);
    if (!built.isValid()) {
        Q_EMIT errorOccurred(built.error);
        return;
    }

    QNetworkRequest request(built.url);
    for (const auto &header : built.headers) {
        request.setRawHeader(header.first, header.second);
    }
    request.setTransferTimeout(60000);
    m_activeProvider = provider;
    m_cancelRequested = false;
    m_reply = m_networkManager->post(request, built.body);
    setBusy(true);

    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *finishedReply = m_reply;
        const int status = finishedReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto networkError = finishedReply->error();
        const QByteArray payload = finishedReply->readAll();
        const auto result = completeResponse(m_activeProvider, status, networkError, m_cancelRequested, payload);
        finishedReply->deleteLater();
        m_reply = nullptr;
        setBusy(false);
        if (result.cancelled) {
            Q_EMIT requestCancelled();
        } else if (!result.isValid()) {
            Q_EMIT errorOccurred(result.error);
        } else {
            Q_EMIT planReady(result.planJson);
        }
    });
}

void AiProviderClient::cancel()
{
    if (m_reply) {
        m_cancelRequested = true;
        m_reply->abort();
    }
}

void AiProviderClient::setBusy(bool busy)
{
    Q_EMIT busyChanged(busy);
}

} // namespace AiEditor
} // namespace Kdenlive
