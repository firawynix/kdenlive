/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "editplan.hpp"

#include <QByteArray>
#include <QList>
#include <QNetworkReply>
#include <QObject>
#include <QPair>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

namespace Kdenlive {
namespace AiEditor {

enum class AiProvider { OpenRouter, OpenAI, Anthropic };

struct BuiltAiRequest
{
    QUrl url;
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
    QString error;

    bool isValid() const;
};

struct AiProviderResponse
{
    EditPlan plan;
    QByteArray planJson;
    QString error;
    bool cancelled{false};

    bool isValid() const;
};

class AiProviderClient : public QObject
{
    Q_OBJECT

public:
    explicit AiProviderClient(QObject *parent = nullptr);

    static QString displayName(AiProvider provider);
    static QString environmentVariable(AiProvider provider);
    static QString defaultModel(AiProvider provider);
    static BuiltAiRequest buildRequest(AiProvider provider, const QString &model, const QByteArray &apiKey, const QString &prompt, int timelineFrames,
                                       double fps);
    static AiProviderResponse parseSuccessfulResponse(AiProvider provider, const QByteArray &payload);
    static AiProviderResponse completeResponse(AiProvider provider, int httpStatus, QNetworkReply::NetworkError networkError, bool wasCancelled,
                                               const QByteArray &payload);

    bool isBusy() const;
    void requestPlan(AiProvider provider, const QString &model, const QByteArray &apiKey, const QString &prompt, int timelineFrames, double fps);
    void cancel();

Q_SIGNALS:
    void planReady(const QByteArray &validatedPlanJson);
    void errorOccurred(const QString &message);
    void requestCancelled();
    void busyChanged(bool busy);

private:
    void setBusy(bool busy);

    QNetworkAccessManager *m_networkManager{nullptr};
    QNetworkReply *m_reply{nullptr};
    AiProvider m_activeProvider{AiProvider::OpenRouter};
    bool m_cancelRequested{false};
};

} // namespace AiEditor
} // namespace Kdenlive

