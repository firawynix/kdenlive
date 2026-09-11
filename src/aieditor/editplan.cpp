/*
    SPDX-FileCopyrightText: 2026 Kdenlive AI Editor contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "editplan.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <algorithm>
#include <limits>

namespace Kdenlive {
namespace AiEditor {

namespace {
constexpr qsizetype MaxPlanSize = 256 * 1024;
constexpr qsizetype MaxOperationCount = 4096;

bool readFrame(const QJsonObject &object, const QString &name, int minimum, int &result, QString &error)
{
    const QJsonValue value = object.value(name);
    if (!value.isDouble()) {
        error = QStringLiteral("Operation field '%1' must be an integer.").arg(name);
        return false;
    }

    const qint64 invalid = std::numeric_limits<qint64>::min();
    const qint64 frame = value.toInteger(invalid);
    if (frame == invalid || frame < minimum || frame > std::numeric_limits<int>::max()) {
        error = QStringLiteral("Operation field '%1' is outside the supported frame range.").arg(name);
        return false;
    }

    result = int(frame);
    return true;
}

bool parseRetimeRange(const QJsonObject &object, RetimeRangeOperation &operation, QString &error)
{
    if (!readFrame(object, QStringLiteral("start_frame"), 0, operation.startFrame, error) ||
        !readFrame(object, QStringLiteral("end_frame"), 0, operation.endFrame, error) ||
        !readFrame(object, QStringLiteral("target_duration_frames"), 1, operation.targetDurationFrames, error)) {
        return false;
    }

    if (operation.endFrame <= operation.startFrame) {
        error = QStringLiteral("Operation field 'end_frame' must be greater than 'start_frame'.");
        return false;
    }

    const QJsonValue preservePitch = object.value(QStringLiteral("preserve_pitch"));
    if (!preservePitch.isUndefined()) {
        if (!preservePitch.isBool()) {
            error = QStringLiteral("Operation field 'preserve_pitch' must be a boolean.");
            return false;
        }
        operation.preservePitch = preservePitch.toBool();
    }
    return true;
}

bool parseMuteRange(const QJsonObject &object, MuteRangeOperation &operation, QString &error)
{
    if (!readFrame(object, QStringLiteral("start_frame"), 0, operation.startFrame, error) ||
        !readFrame(object, QStringLiteral("end_frame"), 0, operation.endFrame, error)) {
        return false;
    }
    if (operation.endFrame <= operation.startFrame) {
        error = QStringLiteral("Operation field 'end_frame' must be greater than 'start_frame'.");
        return false;
    }
    return true;
}
} // namespace

double RetimeRangeOperation::speedMultiplier() const
{
    return double(endFrame - startFrame) / double(targetDurationFrames);
}

int EditOperation::startFrame() const
{
    return type == EditOperationType::RetimeRange ? retimeRange.startFrame : muteRange.startFrame;
}

int EditOperation::endFrame() const
{
    return type == EditOperationType::RetimeRange ? retimeRange.endFrame : muteRange.endFrame;
}

bool EditPlanParseResult::isValid() const
{
    return error.isEmpty();
}

EditPlanParseResult parseEditPlan(const QByteArray &json, bool allowEmpty)
{
    EditPlanParseResult result;
    if (json.isEmpty()) {
        result.error = QStringLiteral("The edit plan is empty.");
        return result;
    }
    if (json.size() > MaxPlanSize) {
        result.error = QStringLiteral("The edit plan exceeds the 256 KiB size limit.");
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.error = QStringLiteral("Invalid edit plan JSON at byte %1: %2").arg(parseError.offset).arg(parseError.errorString());
        return result;
    }
    if (!document.isObject()) {
        result.error = QStringLiteral("The edit plan root must be an object.");
        return result;
    }

    const QJsonObject root = document.object();
    const QJsonValue versionValue = root.value(QStringLiteral("version"));
    if (!versionValue.isDouble() || versionValue.toInteger(-1) != 1) {
        result.error = QStringLiteral("Unsupported or missing edit plan version; expected version 1.");
        return result;
    }
    result.plan.version = 1;

    const QJsonValue operationsValue = root.value(QStringLiteral("operations"));
    if (!operationsValue.isArray()) {
        result.error = QStringLiteral("Edit plan field 'operations' must be an array.");
        return result;
    }

    const QJsonArray operations = operationsValue.toArray();
    if ((!allowEmpty && operations.isEmpty()) || operations.size() > MaxOperationCount) {
        result.error = allowEmpty ? QStringLiteral("Edit plan must contain at most 4096 operations.")
                                  : QStringLiteral("Edit plan must contain between 1 and 4096 operations.");
        return result;
    }

    result.plan.operations.reserve(operations.size());
    for (qsizetype index = 0; index < operations.size(); ++index) {
        if (!operations.at(index).isObject()) {
            result.error = QStringLiteral("Operation %1 must be an object.").arg(index + 1);
            result.plan.operations.clear();
            return result;
        }

        const QJsonObject object = operations.at(index).toObject();
        const QJsonValue typeValue = object.value(QStringLiteral("type"));
        if (!typeValue.isString()) {
            result.error = QStringLiteral("Operation %1 has an unsupported or missing type.").arg(index + 1);
            result.plan.operations.clear();
            return result;
        }

        EditOperation operation;
        QString operationError;
        const QString type = typeValue.toString();
        if (type == QLatin1String("retime_range")) {
            operation.type = EditOperationType::RetimeRange;
            if (!parseRetimeRange(object, operation.retimeRange, operationError)) {
                result.error = QStringLiteral("Operation %1 is invalid: %2").arg(index + 1).arg(operationError);
                result.plan.operations.clear();
                return result;
            }
        } else if (type == QLatin1String("mute_range")) {
            operation.type = EditOperationType::MuteRange;
            if (!parseMuteRange(object, operation.muteRange, operationError)) {
                result.error = QStringLiteral("Operation %1 is invalid: %2").arg(index + 1).arg(operationError);
                result.plan.operations.clear();
                return result;
            }
        } else {
            result.error = QStringLiteral("Operation %1 has an unsupported or missing type.").arg(index + 1);
            result.plan.operations.clear();
            return result;
        }
        result.plan.operations.push_back(operation);
    }

    QVector<EditOperation> sorted = result.plan.operations;
    std::sort(sorted.begin(), sorted.end(), [](const EditOperation &left, const EditOperation &right) { return left.startFrame() < right.startFrame(); });
    for (qsizetype index = 1; index < sorted.size(); ++index) {
        if (sorted.at(index).startFrame() < sorted.at(index - 1).endFrame()) {
            result.error = QStringLiteral("Edit plan operations must not overlap.");
            result.plan.operations.clear();
            return result;
        }
    }
    return result;
}

} // namespace AiEditor
} // namespace Kdenlive
