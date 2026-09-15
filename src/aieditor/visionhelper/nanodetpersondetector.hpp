/*
    SPDX-FileCopyrightText: 2022 OpenCV Zoo contributors
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: Apache-2.0
*/

#pragma once

#include <QString>
#include <opencv2/core/mat.hpp>
#include <opencv2/dnn.hpp>

class NanoDetPersonDetector
{
public:
    NanoDetPersonDetector(const QString &modelPath, const QString &requestedDevice, float confidenceThreshold = 0.35F);

    bool detectsPerson(const cv::Mat &rgbImage);
    QString backendName() const;
    float lastPersonConfidence() const;

private:
    cv::Mat prepare(const cv::Mat &rgbImage) const;
    void configureCpu();
    void configurePreferredDevice(const QString &requestedDevice);
    bool inspectOutputs(const std::vector<cv::Mat> &outputs);

    cv::dnn::Net m_net;
    float m_confidenceThreshold{0.35F};
    QString m_backendName{QStringLiteral("CPU")};
    bool m_hasRun{false};
    float m_lastPersonConfidence{0.0F};
};
