/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "autotestconstants.h"

#include <QString>
#include <QColor>
#include <QMetaType>
#include <QSharedPointer>

namespace Autotest {

class TestTreeItem;

enum class ResultType {
    // result types (have icon, color, short text)
    Pass, FIRST_TYPE = Pass,
    Fail,
    ExpectedFail,
    UnexpectedPass,
    Skip,
    BlacklistedPass,
    BlacklistedFail,
    BlacklistedXPass,
    BlacklistedXFail,

    // special (message) types (have icon, color, short text)
    Benchmark,
    MessageDebug,
    MessageInfo,
    MessageWarn,
    MessageFatal,
    MessageSystem,

    // special message - get's icon (but no color/short text) from parent
    MessageLocation,
    // anything below is an internal message (or a pure message without icon)
    MessageInternal, INTERNAL_MESSAGES_BEGIN = MessageInternal,
    // start item (get icon/short text depending on children)
    TestStart,
    // usually no icon/short text - more or less an indicator (and can contain test duration)
    TestEnd,
    // special global (temporary) message
    MessageCurrentTest, INTERNAL_MESSAGES_END = MessageCurrentTest,

    Application,        // special.. not to be used outside of testresultmodel
    Invalid,            // indicator for unknown result items
    LAST_TYPE = Invalid
};

inline uint qHash(const ResultType &result)
{
    return QT_PREPEND_NAMESPACE(qHash(int(result)));
}

class TestResult
{
public:
    TestResult() = default;
    TestResult(const QString &id, const QString &name);
    virtual ~TestResult() {}

    virtual const QString outputString(bool selected) const;
    virtual const TestTreeItem *findTestTreeItem() const;

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    ResultType result() const { return m_result; }
    QString description() const { return m_description; }
    QString fileName() const { return m_file; }
    int line() const { return m_line; }

    void setDescription(const QString &description) { m_description = description; }
    void setFileName(const QString &fileName) { m_file = fileName; }
    void setLine(int line) { m_line = line; }
    void setResult(ResultType type) { m_result = type; }

    static ResultType resultFromString(const QString &resultString);
    static ResultType toResultType(int rt);
    static QString resultToString(const ResultType type);
    static QColor colorForType(const ResultType type);

    virtual bool isDirectParentOf(const TestResult *other, bool *needsIntermediate) const;
    virtual bool isIntermediateFor(const TestResult *other) const;
    virtual TestResult *createIntermediateResultFor(const TestResult *other);
private:
    QString m_id;
    QString m_name;
    ResultType m_result = ResultType::Invalid;  // the real result..
    QString m_description;
    QString m_file;
    int m_line = 0;
};

using TestResultPtr = QSharedPointer<TestResult>;

} // namespace Autotest

Q_DECLARE_METATYPE(Autotest::TestResult)
Q_DECLARE_METATYPE(Autotest::ResultType)
