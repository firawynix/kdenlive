/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "aiproviderclient.hpp"
#include "editplan.hpp"

#include <QWidget>

class MainWindow;
class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace Kdenlive {
namespace AiEditor {

class LocalTimelineTranscriber;

class AssistantDock : public QWidget
{
    Q_OBJECT

public:
    explicit AssistantDock(MainWindow *mainWindow);

private:
    AiProvider selectedProvider() const;
    QByteArray selectedApiKey() const;
    void updateProvider();
    void updateCredentialStatus();
    void generatePlan();
    void requestProviderPlan(const QString &transcript = QString());
    void showPlan(const QByteArray &planJson);
    void applyPlan();
    void discardPlan();
    void setStatus(const QString &message, bool error = false);
    void setBusy(bool busy);
    QString formatFrames(int frames, double fps) const;

    MainWindow *m_mainWindow{nullptr};
    AiProviderClient *m_client{nullptr};
    LocalTimelineTranscriber *m_transcriber{nullptr};
    QComboBox *m_provider{nullptr};
    QLineEdit *m_model{nullptr};
    QLabel *m_keyStatus{nullptr};
    QComboBox *m_preset{nullptr};
    QCheckBox *m_analyzeAudio{nullptr};
    QPlainTextEdit *m_prompt{nullptr};
    QPushButton *m_generate{nullptr};
    QPushButton *m_cancel{nullptr};
    QPlainTextEdit *m_preview{nullptr};
    QLabel *m_status{nullptr};
    QPushButton *m_apply{nullptr};
    QPushButton *m_discard{nullptr};
    EditPlan m_plan;
    bool m_hasPlan{false};
    QString m_pendingPrompt;
};

} // namespace AiEditor
} // namespace Kdenlive
