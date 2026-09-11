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
#include <algorithm>
#include <utility>

namespace Kdenlive {
namespace AiEditor {

namespace {
QJsonObject editPlanSchema(bool allowEmptyPlan)
{
    const QJsonObject rangeProperties{
        {QStringLiteral("start_frame"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("minimum"), 0}}},
        {QStringLiteral("end_frame"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("minimum"), 1}}},
    };
    QJsonObject retimeProperties = rangeProperties;
    retimeProperties.insert(QStringLiteral("type"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                                                {QStringLiteral("enum"), QJsonArray{QStringLiteral("retime_range")}}});
    retimeProperties.insert(QStringLiteral("target_duration_frames"),
                            QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("minimum"), 1}});
    retimeProperties.insert(QStringLiteral("preserve_pitch"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}});
    QJsonObject muteProperties = rangeProperties;
    muteProperties.insert(QStringLiteral("type"),
                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}, {QStringLiteral("enum"), QJsonArray{QStringLiteral("mute_range")}}});

    const QJsonObject retimeOperation{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("additionalProperties"), false},
        {QStringLiteral("properties"), retimeProperties},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("type"), QStringLiteral("start_frame"), QStringLiteral("end_frame"),
                                                QStringLiteral("target_duration_frames"), QStringLiteral("preserve_pitch")}}};
    const QJsonObject muteOperation{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("additionalProperties"), false},
        {QStringLiteral("properties"), muteProperties},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("type"), QStringLiteral("start_frame"), QStringLiteral("end_frame")}}};
    const QJsonObject operation{{QStringLiteral("anyOf"), QJsonArray{retimeOperation, muteOperation}}};
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("additionalProperties"), false},
        {QStringLiteral("properties"),
         QJsonObject{{QStringLiteral("version"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("enum"), QJsonArray{1}}}},
                     {QStringLiteral("operations"), QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                                                                {QStringLiteral("minItems"), allowEmptyPlan ? 0 : 1},
                                                                {QStringLiteral("maxItems"), 256},
                                                                {QStringLiteral("items"), operation}}}}},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("version"), QStringLiteral("operations")}}};
}

QString systemPrompt()
{
    return QStringLiteral(
        "You translate a user's Kdenlive editing instruction into a safe edit plan. Output only the requested JSON schema. "
        "Supported operations are retime_range and mute_range. Use mute_range for dialogue the user wants silent, without removing video. "
        "Use retime_range to compress silent gaps or for exact-duration speed changes. A transcript line [start-end] is audible dialogue in exactly that "
        "frame range. A silent gap is only the interval from one transcript line's end to the next line's start. If the user asks to clean dialogue, create "
        "mute_range only for transcript speech whose words are outside the requested topics. Never mute silence, relevant speech, or a leading/trailing "
        "interval without dialogue on both sides. If the user asks to compress silence, create retime_range only for qualifying gaps between transcript lines, "
        "never over speech; calculate the requested target duration in frames and preserve pitch. Interpret explicit timecodes using the supplied FPS, round "
        "to "
        "the nearest frame, make end_frame exclusive, and use target_duration_frames for the requested final duration. Operations must not overlap. For "
        "every operation, end_frame must be strictly greater than start_frame; never reverse the values or emit a zero-length range. Never invent edits "
        "that the user did not request. Example at 25 FPS: instruction 'mute vacation talk and compress silence over 2 seconds to 0.5 seconds', "
        "transcript '[0-40] vacation plans' and '[110-160] Salesforce work' produces mute_range 0-40 and retime_range 40-110 with "
        "target_duration_frames=13 and preserve_pitch=true. It does not edit 110-160 or trailing silence, and never emits a no-op retime. If the supplied "
        "transcript segment contains no requested edit, return version 1 with an empty operations array.");
}

QString userMessage(const QString &prompt, int timelineFrames, double fps, const QString &transcript)
{
    QString message = QStringLiteral("Project context: FPS=%1; timeline_duration_frames=%2.\nUser instruction: %3")
                          .arg(QString::number(fps, 'g', 12))
                          .arg(timelineFrames)
                          .arg(prompt);
    if (!transcript.isEmpty()) {
        message += QStringLiteral("\nTimestamped local transcript (media was not uploaded):\n%1").arg(transcript);
    }
    return message;
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

QByteArray normalizeLocalPlanImpl(const QByteArray &planJson)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(planJson, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return planJson;
    }
    QJsonObject root = document.object();
    const QJsonValue operationsValue = root.value(QStringLiteral("operations"));
    if (!operationsValue.isArray()) {
        return planJson;
    }

    struct LocalOperation
    {
        QJsonObject object;
        qint64 startFrame{0};
        qint64 endFrame{0};
    };
    QVector<LocalOperation> muteOperations;
    QVector<LocalOperation> retimeOperations;
    for (const QJsonValue &value : operationsValue.toArray()) {
        if (!value.isObject()) {
            return planJson;
        }
        QJsonObject operation = value.toObject();
        const QJsonValue startValue = operation.value(QStringLiteral("start_frame"));
        const QJsonValue endValue = operation.value(QStringLiteral("end_frame"));
        if (!startValue.isDouble() || !endValue.isDouble()) {
            return planJson;
        }
        qint64 startFrame = startValue.toInteger(-1);
        qint64 endFrame = endValue.toInteger(-1);
        if (startFrame < 0 || endFrame < 0) {
            return planJson;
        }
        if (startFrame == endFrame) {
            // A zero-length edit changes nothing and is safe to omit.
            continue;
        }
        if (endFrame < startFrame) {
            std::swap(startFrame, endFrame);
            operation.insert(QStringLiteral("start_frame"), startFrame);
            operation.insert(QStringLiteral("end_frame"), endFrame);
        }
        const QString type = operation.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("mute_range")) {
            muteOperations.push_back({operation, startFrame, endFrame});
        } else if (type == QLatin1String("retime_range")) {
            retimeOperations.push_back({operation, startFrame, endFrame});
        } else {
            return planJson;
        }
    }

    const auto byStartFrame = [](const LocalOperation &left, const LocalOperation &right) { return left.startFrame < right.startFrame; };
    std::stable_sort(muteOperations.begin(), muteOperations.end(), byStartFrame);
    QVector<LocalOperation> mergedMutes;
    for (const LocalOperation &operation : std::as_const(muteOperations)) {
        if (mergedMutes.isEmpty() || operation.startFrame > mergedMutes.constLast().endFrame) {
            mergedMutes.push_back(operation);
            continue;
        }
        LocalOperation &previous = mergedMutes.last();
        previous.endFrame = qMax(previous.endFrame, operation.endFrame);
        previous.object.insert(QStringLiteral("end_frame"), previous.endFrame);
    }

    std::stable_sort(retimeOperations.begin(), retimeOperations.end(), byStartFrame);
    QVector<LocalOperation> acceptedRetimes;
    for (const LocalOperation &operation : std::as_const(retimeOperations)) {
        const bool crossesSpeech = std::any_of(mergedMutes.cbegin(), mergedMutes.cend(), [&operation](const LocalOperation &mute) {
            return operation.startFrame < mute.endFrame && mute.startFrame < operation.endFrame;
        });
        if (crossesSpeech || (!acceptedRetimes.isEmpty() && operation.startFrame < acceptedRetimes.constLast().endFrame)) {
            // Conflicting speed changes are ambiguous. Keeping the earlier valid range is safer than inventing a split.
            continue;
        }
        acceptedRetimes.push_back(operation);
    }

    QVector<LocalOperation> accepted = mergedMutes;
    accepted += acceptedRetimes;
    std::stable_sort(accepted.begin(), accepted.end(), byStartFrame);
    QJsonArray normalized;
    for (const LocalOperation &operation : std::as_const(accepted)) {
        normalized.append(operation.object);
    }
    root.insert(QStringLiteral("operations"), normalized);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
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
    case AiProvider::Ollama:
        return QStringLiteral("Local AI (Ollama)");
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
    case AiProvider::Ollama:
        return {};
    }
    return {};
}

QString AiProviderClient::defaultModel(AiProvider provider)
{
    switch (provider) {
    case AiProvider::OpenRouter:
        return QStringLiteral("nex-agi/nex-n2.5-mini:free");
    case AiProvider::OpenAI:
        return QStringLiteral("gpt-5-mini");
    case AiProvider::Anthropic:
        return QStringLiteral("claude-sonnet-5");
    case AiProvider::Ollama:
        return QStringLiteral("qwen3:8b");
    }
    return {};
}

BuiltAiRequest AiProviderClient::buildConnectionTestRequest(AiProvider provider, const QByteArray &apiKey)
{
    BuiltAiRequest result;
    if (provider == AiProvider::Ollama) {
        result.url = QUrl(QStringLiteral("http://127.0.0.1:11434/api/tags"));
        return result;
    }
    if (apiKey.trimmed().isEmpty()) {
        result.error = QStringLiteral("The API key is missing.");
        return result;
    }
    if (provider == AiProvider::OpenRouter) {
        result.url = QUrl(QStringLiteral("https://openrouter.ai/api/v1/key"));
        result.headers.push_back({QByteArrayLiteral("Authorization"), QByteArrayLiteral("Bearer ") + apiKey});
    } else if (provider == AiProvider::OpenAI) {
        result.url = QUrl(QStringLiteral("https://api.openai.com/v1/models"));
        result.headers.push_back({QByteArrayLiteral("Authorization"), QByteArrayLiteral("Bearer ") + apiKey});
    } else {
        result.url = QUrl(QStringLiteral("https://api.anthropic.com/v1/models?limit=1"));
        result.headers.push_back({QByteArrayLiteral("x-api-key"), apiKey});
        result.headers.push_back({QByteArrayLiteral("anthropic-version"), QByteArrayLiteral("2023-06-01")});
    }
    return result;
}

BuiltAiRequest AiProviderClient::buildRequest(AiProvider provider, const QString &model, const QByteArray &apiKey, const QString &prompt, int timelineFrames,
                                              double fps, const QString &transcript, bool allowEmptyPlan)
{
    BuiltAiRequest result;
    if (model.trimmed().isEmpty()) {
        result.error = QStringLiteral("Choose a model before sending the request.");
        return result;
    }
    if (provider != AiProvider::Ollama && apiKey.trimmed().isEmpty()) {
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
    const QJsonObject schema = editPlanSchema(allowEmptyPlan);
    const QJsonArray messages{
        QJsonObject{{QStringLiteral("role"), QStringLiteral("system")}, {QStringLiteral("content"), systemPrompt()}},
        QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                    {QStringLiteral("content"), userMessage(prompt.trimmed(), timelineFrames, fps, transcript)}},
    };

    QJsonObject body;
    body.insert(QStringLiteral("model"), model.trimmed());
    if (provider == AiProvider::Ollama) {
        result.url = QUrl(QStringLiteral("http://127.0.0.1:11434/api/chat"));
        body.insert(QStringLiteral("messages"), messages);
        body.insert(QStringLiteral("stream"), false);
        body.insert(QStringLiteral("think"), false);
        body.insert(QStringLiteral("format"), schema);
        body.insert(QStringLiteral("options"), QJsonObject{{QStringLiteral("temperature"), 0}, {QStringLiteral("num_predict"), 16384}});
    } else if (provider == AiProvider::Anthropic) {
        result.url = QUrl(QStringLiteral("https://api.anthropic.com/v1/messages"));
        result.headers.push_back({QByteArrayLiteral("x-api-key"), apiKey});
        result.headers.push_back({QByteArrayLiteral("anthropic-version"), QByteArrayLiteral("2023-06-01")});
        body.insert(QStringLiteral("max_tokens"), 16384);
        body.insert(QStringLiteral("system"), systemPrompt());
        body.insert(QStringLiteral("messages"), QJsonArray{messages.at(1)});
        body.insert(QStringLiteral("output_config"), QJsonObject{{QStringLiteral("format"), QJsonObject{{QStringLiteral("type"), QStringLiteral("json_schema")},
                                                                                                        {QStringLiteral("schema"), schema}}}});
    } else {
        result.url = provider == AiProvider::OpenRouter ? QUrl(QStringLiteral("https://openrouter.ai/api/v1/chat/completions"))
                                                        : QUrl(QStringLiteral("https://api.openai.com/v1/chat/completions"));
        result.headers.push_back({QByteArrayLiteral("Authorization"), QByteArrayLiteral("Bearer ") + apiKey});
        if (provider == AiProvider::OpenRouter) {
            body.insert(QStringLiteral("max_tokens"), 16384);
            body.insert(QStringLiteral("provider"), QJsonObject{{QStringLiteral("require_parameters"), true}});
        } else {
            body.insert(QStringLiteral("max_completion_tokens"), 16384);
        }
        body.insert(QStringLiteral("messages"), messages);
        body.insert(QStringLiteral("response_format"),
                    QJsonObject{{QStringLiteral("type"), QStringLiteral("json_schema")},
                                {QStringLiteral("json_schema"), QJsonObject{{QStringLiteral("name"), QStringLiteral("kdenlive_edit_plan")},
                                                                            {QStringLiteral("strict"), true},
                                                                            {QStringLiteral("schema"), schema}}}});
    }
    result.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return result;
}

AiProviderResponse AiProviderClient::parseSuccessfulResponse(AiProvider provider, const QByteArray &payload, bool allowEmptyPlan)
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
    QString finishReason;
    if (provider == AiProvider::Ollama) {
        finishReason = root.value(QStringLiteral("done_reason")).toString();
        content = root.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
    } else if (provider == AiProvider::Anthropic) {
        finishReason = root.value(QStringLiteral("stop_reason")).toString();
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
            const QJsonObject choice = choices.at(0).toObject();
            content = choice.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
            finishReason = choice.value(QStringLiteral("finish_reason")).toString();
        }
    }
    const bool responseWasTruncated = finishReason == QLatin1String("length") || finishReason == QLatin1String("max_tokens");
    if (content.isEmpty()) {
        result.outputLimitReached = responseWasTruncated;
        result.error = responseWasTruncated
                           ? QStringLiteral("The AI model reached its output token limit before returning an edit plan.")
                       : finishReason.isEmpty() ? QStringLiteral("The AI provider response did not contain an edit plan.")
                                                : QStringLiteral("The AI provider returned no edit plan (finish reason: %1).").arg(finishReason);
        return result;
    }

    result.planJson = content.toUtf8();
    if (provider == AiProvider::Ollama && allowEmptyPlan) {
        result.planJson = normalizeLocalPlan(result.planJson);
    }
    const auto parsed = parseEditPlan(result.planJson, allowEmptyPlan);
    if (!parsed.isValid()) {
        result.planJson.clear();
        result.outputLimitReached = responseWasTruncated;
        result.error = responseWasTruncated ? QStringLiteral("The AI model reached its output token limit before returning a complete edit plan.")
                                             : QStringLiteral("The AI provider returned an unsafe edit plan: %1").arg(parsed.error);
        return result;
    }
    result.plan = parsed.plan;
    return result;
}

QByteArray AiProviderClient::normalizeLocalPlan(const QByteArray &planJson)
{
    return normalizeLocalPlanImpl(planJson);
}

AiProviderResponse AiProviderClient::completeResponse(AiProvider provider, int httpStatus, QNetworkReply::NetworkError networkError, bool wasCancelled,
                                                      const QByteArray &payload, bool allowEmptyPlan)
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
    if (httpStatus > 0 && (httpStatus < 200 || httpStatus >= 300)) {
        const QByteArray apiMessage = extractApiError(payload);
        result.error = apiMessage.isEmpty() ? QStringLiteral("The AI provider returned HTTP %1.").arg(httpStatus)
                                            : QStringLiteral("The AI provider returned HTTP %1: %2").arg(httpStatus).arg(QString::fromUtf8(apiMessage));
        return result;
    }
    if (networkError != QNetworkReply::NoError) {
        result.error = QStringLiteral("The AI request failed because of a network error.");
        return result;
    }
    if (httpStatus < 200 || httpStatus >= 300) {
        result.error = QStringLiteral("The AI provider returned an invalid HTTP response.");
        return result;
    }
    return parseSuccessfulResponse(provider, payload, allowEmptyPlan);
}

bool AiProviderClient::isBusy() const
{
    return m_reply != nullptr;
}

void AiProviderClient::requestPlan(AiProvider provider, const QString &model, const QByteArray &apiKey, const QString &prompt, int timelineFrames, double fps,
                                   const QString &transcript, bool allowEmptyPlan)
{
    if (isBusy()) {
        Q_EMIT errorOccurred(QStringLiteral("An AI request is already running."));
        return;
    }
    const BuiltAiRequest built = buildRequest(provider, model, apiKey, prompt, timelineFrames, fps, transcript, allowEmptyPlan);
    if (!built.isValid()) {
        Q_EMIT errorOccurred(built.error);
        return;
    }

    m_allowEmptyPlan = allowEmptyPlan;
    startRequest(built, provider, RequestKind::EditPlan);
}

void AiProviderClient::testConnection(AiProvider provider, const QByteArray &apiKey)
{
    if (isBusy()) {
        Q_EMIT connectionTested(false, QStringLiteral("Another provider request is already running."));
        return;
    }
    const BuiltAiRequest built = buildConnectionTestRequest(provider, apiKey);
    if (!built.isValid()) {
        Q_EMIT connectionTested(false, built.error);
        return;
    }
    startRequest(built, provider, RequestKind::ConnectionTest);
}

void AiProviderClient::startRequest(const BuiltAiRequest &built, AiProvider provider, RequestKind kind)
{
    QNetworkRequest request(built.url);
    for (const auto &header : built.headers) {
        request.setRawHeader(header.first, header.second);
    }
    request.setTransferTimeout(provider == AiProvider::Ollama ? 30 * 60 * 1000 : 180000);
    m_activeProvider = provider;
    m_requestKind = kind;
    m_cancelRequested = false;
    m_reply = kind == RequestKind::EditPlan ? m_networkManager->post(request, built.body) : m_networkManager->get(request);
    setBusy(true);

    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *finishedReply = m_reply;
        const int status = finishedReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto networkError = finishedReply->error();
        const QByteArray payload = finishedReply->readAll();
        const RequestKind requestKind = m_requestKind;
        finishedReply->deleteLater();
        m_reply = nullptr;
        setBusy(false);
        if (requestKind == RequestKind::ConnectionTest) {
            if (m_cancelRequested || networkError == QNetworkReply::OperationCanceledError) {
                Q_EMIT requestCancelled();
            } else if (status >= 200 && status < 300 && networkError == QNetworkReply::NoError) {
                Q_EMIT connectionTested(true, QStringLiteral("Connection successful."));
            } else {
                const QByteArray apiMessage = extractApiError(payload);
                const QString message = apiMessage.isEmpty()
                                            ? QStringLiteral("Connection test failed (HTTP %1).").arg(status)
                                            : QStringLiteral("Connection test failed (HTTP %1): %2").arg(status).arg(QString::fromUtf8(apiMessage));
                Q_EMIT connectionTested(false, message);
            }
            return;
        }
        const auto result = completeResponse(m_activeProvider, status, networkError, m_cancelRequested, payload, m_allowEmptyPlan);
        if (result.cancelled) {
            Q_EMIT requestCancelled();
        } else if (!result.isValid()) {
            if (result.outputLimitReached) {
                Q_EMIT outputLimitReached(result.error);
            } else {
                Q_EMIT errorOccurred(result.error);
            }
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
