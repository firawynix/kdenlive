/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "nanodetpersondetector.hpp"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>
#include <memory>
#include <mlt++/Mlt.h>
#include <opencv2/imgproc.hpp>

namespace {
QString configureMltEnvironment()
{
    const QDir applicationDir(QCoreApplication::applicationDirPath());
    const QString kdeRoot = qEnvironmentVariable("KDEROOT");
    QStringList repositoryCandidates;
    if (!kdeRoot.isEmpty()) {
        repositoryCandidates << QDir(kdeRoot).filePath(QStringLiteral("lib/mlt")) << QDir(kdeRoot).filePath(QStringLiteral("lib/mlt-7"));
    }
    repositoryCandidates << applicationDir.filePath(QStringLiteral("../lib/mlt")) << applicationDir.filePath(QStringLiteral("../lib/mlt-7"));
    QString repositoryPath;
    for (const QString &candidate : repositoryCandidates) {
        if (QFileInfo(candidate).isDir()) {
            qputenv("MLT_REPOSITORY", QDir::toNativeSeparators(QDir(candidate).absolutePath()).toUtf8());
            repositoryPath = QDir(candidate).absolutePath();
            break;
        }
    }
    const QStringList dataCandidates{QDir(kdeRoot).filePath(QStringLiteral("share/mlt")), applicationDir.filePath(QStringLiteral("../share/mlt"))};
    for (const QString &data : dataCandidates) {
        if (!data.isEmpty() && QFileInfo(data).isDir() && QFileInfo(QDir(data).filePath(QStringLiteral("profiles"))).isDir()) {
            qputenv("MLT_DATA", QDir::toNativeSeparators(QDir(data).absolutePath()).toUtf8());
            break;
        }
    }
    return repositoryPath;
}

int fail(const QString &message)
{
    QTextStream(stderr) << "ERROR " << message << Qt::endl;
    return 1;
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Firawynix Kdenlive local visual analyzer"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("scene"), QStringLiteral("MLT timeline scene"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("model"), QStringLiteral("NanoDet ONNX model"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("output"), QStringLiteral("Detection JSON output"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("sample-step"), QStringLiteral("Frames between samples"), QStringLiteral("frames")});
    parser.addOption({QStringLiteral("timeline-frames"), QStringLiteral("Timeline duration in frames"), QStringLiteral("frames")});
    parser.addOption({QStringLiteral("device"), QStringLiteral("auto, cpu, or cuda"), QStringLiteral("device"), QStringLiteral("auto")});
    parser.process(app);

    const QString scenePath = parser.value(QStringLiteral("scene"));
    const QString modelPath = parser.value(QStringLiteral("model"));
    const QString outputPath = parser.value(QStringLiteral("output"));
    const int sampleStep = parser.value(QStringLiteral("sample-step")).toInt();
    const int requestedFrames = parser.value(QStringLiteral("timeline-frames")).toInt();
    if (!QFileInfo(scenePath).isFile() || !QFileInfo(modelPath).isFile() || outputPath.isEmpty() || sampleStep < 1 || requestedFrames < 1) {
        return fail(QStringLiteral("invalid or missing command-line arguments"));
    }

    const QString repositoryPath = configureMltEnvironment();
    Mlt::Repository *repository = Mlt::Factory::init(repositoryPath.isEmpty() ? nullptr : repositoryPath.toUtf8().constData());
    if (!repository) {
        return fail(QStringLiteral("MLT could not be initialized"));
    }
    int resultCode = 0;
    try {
        Mlt::Profile profile;
        profile.set_explicit(0);
        Mlt::Producer producer(profile, "xml", scenePath.toUtf8().constData());
        if (!producer.is_valid()) {
            Mlt::Factory::close();
            return fail(QStringLiteral("the timeline scene is not a valid MLT producer"));
        }
        NanoDetPersonDetector detector(modelPath, parser.value(QStringLiteral("device")));
        const int timelineFrames = qMin(requestedFrames, qMax(1, producer.get_length()));
        const int totalSamples = (timelineFrames + sampleStep - 1) / sampleStep;
        QJsonArray personFrames;
        int completed = 0;
        for (int position = 0; position < timelineFrames; position += sampleStep) {
            producer.seek(position);
            std::unique_ptr<Mlt::Frame> frame(producer.get_frame());
            if (!frame || !frame->is_valid()) {
                throw std::runtime_error("MLT returned an invalid video frame");
            }
            int width = 640;
            int height = 360;
            mlt_image_format format = mlt_image_rgba;
            const uint8_t *pixels = frame->get_image(format, width, height);
            if (pixels) {
                cv::Mat rgba(height, width, CV_8UC4, const_cast<uint8_t *>(pixels));
                cv::Mat rgb;
                cv::cvtColor(rgba, rgb, cv::COLOR_RGBA2RGB);
                if (detector.detectsPerson(rgb)) {
                    personFrames.append(position);
                }
                if (qEnvironmentVariableIsSet("KDENLIVE_AI_VISION_DEBUG")) {
                    const cv::Scalar mean = cv::mean(rgb);
                    QTextStream(stderr) << "DEBUG frame=" << position << " mean=" << mean[0] << ',' << mean[1] << ',' << mean[2]
                                        << " person=" << detector.lastPersonConfidence() << " service=" << frame->get("mlt_service")
                                        << " resource=" << frame->get("resource") << " test=" << frame->get_int("test_image") << " first=" << int(pixels[0])
                                        << ',' << int(pixels[1]) << ',' << int(pixels[2]) << ',' << int(pixels[3]) << Qt::endl;
                }
            }
            ++completed;
            if (completed == totalSamples || completed % qMax(1, totalSamples / 200) == 0) {
                QTextStream(stdout) << "PROGRESS " << completed << ' ' << totalSamples << Qt::endl;
            }
        }

        const QJsonObject root{{QStringLiteral("version"), 1},
                               {QStringLiteral("sample_step_frames"), sampleStep},
                               {QStringLiteral("backend"), detector.backendName()},
                               {QStringLiteral("person_frames"), personFrames}};
        QSaveFile output(outputPath);
        if (!output.open(QIODevice::WriteOnly) || output.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0 || !output.commit()) {
            throw std::runtime_error("the detection output could not be saved");
        }
        QTextStream(stdout) << "DONE " << personFrames.size() << Qt::endl;
    } catch (const cv::Exception &error) {
        resultCode = fail(QString::fromUtf8(error.what()));
    } catch (const std::exception &error) {
        resultCode = fail(QString::fromUtf8(error.what()));
    }
    Mlt::Factory::close();
    return resultCode;
}
