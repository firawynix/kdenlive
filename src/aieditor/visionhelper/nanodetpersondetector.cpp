/*
    NanoDet preprocessing follows the OpenCV Zoo reference implementation.

    SPDX-FileCopyrightText: 2022 OpenCV Zoo contributors
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: Apache-2.0
*/

#include "nanodetpersondetector.hpp"

#include <opencv2/core/ocl.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

NanoDetPersonDetector::NanoDetPersonDetector(const QString &modelPath, const QString &requestedDevice, float confidenceThreshold)
    : m_net(cv::dnn::readNet(modelPath.toStdString()))
    , m_confidenceThreshold(confidenceThreshold)
{
    if (m_net.empty()) {
        throw std::runtime_error("NanoDet model could not be loaded");
    }
    configurePreferredDevice(requestedDevice);
}

void NanoDetPersonDetector::configureCpu()
{
    m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    m_backendName = QStringLiteral("CPU");
}

void NanoDetPersonDetector::configurePreferredDevice(const QString &requestedDevice)
{
    if (requestedDevice == QLatin1String("cuda")) {
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA_FP16);
        m_backendName = QStringLiteral("CUDA FP16");
        return;
    }
    if (requestedDevice == QLatin1String("auto") && cv::ocl::haveOpenCL()) {
        cv::ocl::setUseOpenCL(true);
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);
        m_backendName = QStringLiteral("OpenCL");
        return;
    }
    configureCpu();
}

cv::Mat NanoDetPersonDetector::prepare(const cv::Mat &rgbImage) const
{
    if (rgbImage.empty()) {
        return {};
    }
    constexpr int target = 416;
    const double scale = std::min(double(target) / double(rgbImage.cols), double(target) / double(rgbImage.rows));
    const int width = std::max(1, int(std::round(double(rgbImage.cols) * scale)));
    const int height = std::max(1, int(std::round(double(rgbImage.rows) * scale)));
    cv::Mat resized;
    cv::resize(rgbImage, resized, cv::Size(width, height), 0.0, 0.0, cv::INTER_AREA);
    const int left = (target - width) / 2;
    const int right = target - width - left;
    const int top = (target - height) / 2;
    const int bottom = target - height - top;
    cv::Mat padded;
    cv::copyMakeBorder(resized, padded, top, bottom, left, right, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

    cv::Mat floating;
    padded.convertTo(floating, CV_32FC3);
    std::vector<cv::Mat> channels;
    cv::split(floating, channels);
    const float means[] = {103.53F, 116.28F, 123.675F};
    const float scales[] = {1.0F / 57.375F, 1.0F / 57.12F, 1.0F / 58.395F};
    for (int index = 0; index < 3; ++index) {
        channels[index] = (channels[index] - means[index]) * scales[index];
    }
    cv::merge(channels, floating);
    return cv::dnn::blobFromImage(floating, 1.0, cv::Size(target, target), cv::Scalar(), false, false, CV_32F);
}

bool NanoDetPersonDetector::inspectOutputs(const std::vector<cv::Mat> &outputs)
{
    // NanoDet exposes alternating classification and box-regression tensors.
    // COCO class zero is "person". For presence detection, box decoding and NMS
    // are unnecessary: one confident person anchor is sufficient.
    m_lastPersonConfidence = 0.0F;
    for (size_t index = 0; index < outputs.size(); index += 2) {
        cv::Mat scores = outputs[index];
        if (scores.empty()) {
            continue;
        }
        if (scores.dims == 3) {
            scores = scores.reshape(0, scores.size[1]);
        } else if (scores.dims > 2) {
            scores = scores.reshape(0, int(scores.total() / scores.size[scores.dims - 1]));
        }
        if (scores.cols < 1) {
            continue;
        }
        double maximum = 0.0;
        cv::minMaxLoc(scores.col(0), nullptr, &maximum);
        m_lastPersonConfidence = std::max(m_lastPersonConfidence, float(maximum));
        if (maximum >= m_confidenceThreshold) {
            return true;
        }
    }
    return false;
}

bool NanoDetPersonDetector::detectsPerson(const cv::Mat &rgbImage)
{
    const cv::Mat blob = prepare(rgbImage);
    if (blob.empty()) {
        return false;
    }
    auto run = [&]() {
        m_net.setInput(blob);
        std::vector<cv::Mat> outputs;
        m_net.forward(outputs, m_net.getUnconnectedOutLayersNames());
        return inspectOutputs(outputs);
    };
    try {
        const bool result = run();
        m_hasRun = true;
        return result;
    } catch (const cv::Exception &) {
        if (m_backendName == QLatin1String("CPU") || m_hasRun) {
            throw;
        }
        configureCpu();
        const bool result = run();
        m_hasRun = true;
        return result;
    }
}

QString NanoDetPersonDetector::backendName() const
{
    return m_backendName;
}

float NanoDetPersonDetector::lastPersonConfidence() const
{
    return m_lastPersonConfidence;
}
