/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "aiproviderclient.hpp"
#include "aisessionstore.hpp"
#include "editplan.hpp"

#include <QElapsedTimer>
#include <QWidget>

class MainWindow;
class QComboBox;
class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QSpinBox;

namespace Kdenlive {
namespace AiEditor {

class LocalTimelineTranscriber;
class LocalAiManager;

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
    void saveCredential();
    void removeCredential();
    void testCredential();
    void updatePerformanceSummary();
    void generatePlan();
    void requestProviderPlan(const QString &transcript = QString(), const QString &timelineFingerprint = QString());
    void requestNextTranscriptChunk();
    void retryWithSmallerTranscriptChunks(const QString &message);
    void handleProviderPlan(const QByteArray &planJson);
    void finishChunkedPlan();
    void resetPlanPreview();
    void showPlan(const QByteArray &planJson);
    void applyPlan();
    void discardPlan();
    void setStatus(const QString &message, bool error = false);
    void setBusy(bool busy);
    void showProgress(int percent, qint64 remainingSeconds, const QString &phase);
    void hideProgress();
    QString formatFrames(int frames, double fps) const;

    MainWindow *m_mainWindow{nullptr};
    AiProviderClient *m_client{nullptr};
    LocalTimelineTranscriber *m_transcriber{nullptr};
    LocalAiManager *m_localAi{nullptr};
    QComboBox *m_provider{nullptr};
    QLineEdit *m_model{nullptr};
    QLabel *m_keyStatus{nullptr};
    QLineEdit *m_keyInput{nullptr};
    QLabel *m_keyLabel{nullptr};
    QPushButton *m_saveKey{nullptr};
    QPushButton *m_testKey{nullptr};
    QPushButton *m_removeKey{nullptr};
    QWidget *m_credentialButtons{nullptr};
    QLabel *m_privacy{nullptr};
    QGroupBox *m_localAiGroup{nullptr};
    QLabel *m_localHardware{nullptr};
    QLabel *m_localStatus{nullptr};
    QPushButton *m_localSetup{nullptr};
    QSlider *m_performance{nullptr};
    QLabel *m_performanceSummary{nullptr};
    QSpinBox *m_cpuThreads{nullptr};
    QComboBox *m_processingDevice{nullptr};
    QComboBox *m_preset{nullptr};
    QCheckBox *m_analyzeAudio{nullptr};
    QPlainTextEdit *m_prompt{nullptr};
    QPushButton *m_generate{nullptr};
    QPushButton *m_cancel{nullptr};
    QPlainTextEdit *m_preview{nullptr};
    QLabel *m_status{nullptr};
    QProgressBar *m_progress{nullptr};
    QLabel *m_progressDetails{nullptr};
    QPushButton *m_apply{nullptr};
    QPushButton *m_discard{nullptr};
    EditPlan m_plan;
    bool m_hasPlan{false};
    QString m_pendingPrompt;
    QVector<TranscriptChunk> m_transcriptChunks;
    AiSessionCheckpoint m_checkpoint;
    bool m_chunkedRequest{false};
    bool m_applyInProgress{false};
    QElapsedTimer m_providerTimer;
    int m_providerStartChunk{0};
};

} // namespace AiEditor
} // namespace Kdenlive
