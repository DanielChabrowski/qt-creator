/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include "pythonsettings.h"

#include "pythonconstants.h"

#include <coreplugin/dialogs/ioptionspage.h>
#include "coreplugin/icore.h"

#include <utils/algorithm.h>
#include <utils/qtcassert.h>
#include <utils/detailswidget.h>
#include <utils/environment.h>
#include <utils/listmodel.h>
#include <utils/pathchooser.h>
#include <utils/synchronousprocess.h>
#include <utils/treemodel.h>

#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QSettings>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

namespace Python {
namespace Internal {

using namespace Utils;

class InterpreterDetailsWidget : public QWidget
{
public:
    InterpreterDetailsWidget()
        : m_name(new QLineEdit)
        , m_executable(new Utils::PathChooser())
    {
        auto mainLayout = new QGridLayout();
        mainLayout->addWidget(new QLabel(tr("Name:")), 0, 0);
        mainLayout->addWidget(m_name, 0, 1);
        mainLayout->addWidget(new QLabel(tr("Executable")), 1, 0);
        mainLayout->addWidget(m_executable, 1, 1);
        m_executable->setExpectedKind(Utils::PathChooser::ExistingCommand);
        setLayout(mainLayout);
    }

    void updateInterpreter(const Interpreter &interpreter)
    {
        m_name->setText(interpreter.name);
        m_executable->setPath(interpreter.command.toString());
        m_currentId = interpreter.id;
    }

    Interpreter toInterpreter()
    {
        return {m_currentId, m_name->text(), FilePath::fromUserInput(m_executable->path())};
    }
    QLineEdit *m_name = nullptr;
    PathChooser *m_executable = nullptr;
    QString m_currentId;
};


class InterpreterOptionsWidget : public QWidget
{
public:
    InterpreterOptionsWidget(const QList<Interpreter> &interpreters,
                             const QString &defaultInterpreter);

    void apply();

private:
    QTreeView m_view;
    ListModel<Interpreter> m_model;
    InterpreterDetailsWidget *m_detailsWidget = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_makeDefaultButton = nullptr;
    QString m_defaultId;

    void currentChanged(const QModelIndex &index, const QModelIndex &previous);
    void addItem();
    void deleteItem();
    void makeDefault();
};

InterpreterOptionsWidget::InterpreterOptionsWidget(const QList<Interpreter> &interpreters, const QString &defaultInterpreter)
    : m_detailsWidget(new InterpreterDetailsWidget())
    , m_defaultId(defaultInterpreter)
{
    m_model.setDataAccessor([this](const Interpreter &interpreter, int, int role) -> QVariant {
        if (role == Qt::DisplayRole)
            return interpreter.name;
        if (role == Qt::FontRole) {
            QFont f = font();
            f.setBold(interpreter.id == m_defaultId);
            return f;
        }
        return {};
    });

    for (const Interpreter &interpreter : interpreters)
        m_model.appendItem(interpreter);

    auto mainLayout = new QVBoxLayout();
    auto layout = new QHBoxLayout();
    m_view.setModel(&m_model);
    m_view.setHeaderHidden(true);
    m_view.setSelectionMode(QAbstractItemView::SingleSelection);
    m_view.setSelectionBehavior(QAbstractItemView::SelectItems);
    connect(m_view.selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &InterpreterOptionsWidget::currentChanged);
    auto buttonLayout = new QVBoxLayout();
    auto addButton = new QPushButton(InterpreterOptionsWidget::tr("&Add"));
    connect(addButton, &QPushButton::pressed, this, &InterpreterOptionsWidget::addItem);
    m_deleteButton = new QPushButton(InterpreterOptionsWidget::tr("&Delete"));
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::pressed, this, &InterpreterOptionsWidget::deleteItem);
    m_makeDefaultButton = new QPushButton(InterpreterOptionsWidget::tr("&Make Default"));
    m_makeDefaultButton->setEnabled(false);
    connect(m_makeDefaultButton, &QPushButton::pressed, this, &InterpreterOptionsWidget::makeDefault);
    mainLayout->addLayout(layout);
    mainLayout->addWidget(m_detailsWidget);
    m_detailsWidget->hide();
    setLayout(mainLayout);
    layout->addWidget(&m_view);
    layout->addLayout(buttonLayout);
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(m_makeDefaultButton);
    buttonLayout->addStretch(10);
}

void InterpreterOptionsWidget::apply()
{
    const QModelIndex &index = m_view.currentIndex();
    if (index.isValid()) {
        m_model.itemAt(index.row())->itemData = m_detailsWidget->toInterpreter();
        emit m_model.dataChanged(index, index);
    }

    QList<Interpreter> interpreters;
    for (const ListItem<Interpreter> *treeItem : m_model)
        interpreters << treeItem->itemData;
    PythonSettings::setInterpreter(interpreters, m_defaultId);
}

void InterpreterOptionsWidget::currentChanged(const QModelIndex &index, const QModelIndex &previous)
{
    if (previous.isValid()) {
        m_model.itemAt(previous.row())->itemData = m_detailsWidget->toInterpreter();
        emit m_model.dataChanged(previous, previous);
    }
    if (index.isValid()) {
        m_detailsWidget->updateInterpreter(m_model.itemAt(index.row())->itemData);
        m_detailsWidget->show();
    } else {
        m_detailsWidget->hide();
    }
    m_deleteButton->setEnabled(index.isValid());
    m_makeDefaultButton->setEnabled(index.isValid());
}

void InterpreterOptionsWidget::addItem()
{
    const QModelIndex &index = m_model.indexForItem(
                m_model.appendItem({QUuid::createUuid().toString(), QString("Python"), FilePath()}));
    QTC_ASSERT(index.isValid(), return);
    m_view.setCurrentIndex(index);
}

void InterpreterOptionsWidget::deleteItem()
{
    const QModelIndex &index = m_view.currentIndex();
    if (index.isValid())
        m_model.destroyItem(m_model.itemAt(index.row()));
}

class InterpreterOptionsPage : public Core::IOptionsPage
{
    Q_OBJECT

public:
    InterpreterOptionsPage();

    void setInterpreter(const QList<Interpreter> &interpreters) { m_interpreters = interpreters; }
    QList<Interpreter> interpreters() const { return m_interpreters; }
    void setDefaultInterpreter(const QString &defaultId)
    { m_defaultInterpreterId = defaultId; }
    Interpreter defaultInterpreter() const;

    QWidget *widget() override;
    void apply() override;
    void finish() override;

private:
    QPointer<InterpreterOptionsWidget> m_widget;
    QList<Interpreter> m_interpreters;
    QString m_defaultInterpreterId;
};

InterpreterOptionsPage::InterpreterOptionsPage()
{
    setId(Constants::C_PYTHONOPTIONS_PAGE_ID);
    setDisplayName(tr("Interpreters"));
    setCategory(Constants::C_PYTHON_SETTINGS_CATEGORY);
    setDisplayCategory(tr("Python"));
    setCategoryIcon(Utils::Icon({{":/python/images/settingscategory_python.png",
                                  Utils::Theme::PanelTextColorDark}}, Utils::Icon::Tint));
}

Interpreter InterpreterOptionsPage::defaultInterpreter() const
{
    if (m_defaultInterpreterId.isEmpty())
        return {};
    return Utils::findOrDefault(m_interpreters, [this](const Interpreter &interpreter) {
        return interpreter.id == m_defaultInterpreterId;
    });
}

QWidget *InterpreterOptionsPage::widget()
{
    if (!m_widget)
        m_widget = new InterpreterOptionsWidget(m_interpreters, m_defaultInterpreterId);
    return m_widget;
}

void InterpreterOptionsPage::apply()
{
    if (m_widget)
        m_widget->apply();
}

void InterpreterOptionsPage::finish()
{
    delete m_widget;
    m_widget = nullptr;
}

static bool alreadyRegistered(const QList<Interpreter> &pythons, const FilePath &pythonExecutable)
{
    return Utils::anyOf(pythons, [pythonExecutable](const Interpreter &interpreter) {
        return interpreter.command.toFileInfo().canonicalFilePath()
               == pythonExecutable.toFileInfo().canonicalFilePath();
    });
}

Interpreter interpreterForPythonExecutable(const FilePath &python,
                                           const QString &defaultName,
                                           bool windowedSuffix = false)
{
    SynchronousProcess pythonProcess;
    pythonProcess.setProcessChannelMode(QProcess::MergedChannels);
    SynchronousProcessResponse response = pythonProcess.runBlocking(
        CommandLine(python, {"--version"}));
    QString name;
    if (response.result == SynchronousProcessResponse::Finished)
        name = response.stdOut().trimmed();
    if (name.isEmpty())
        name = defaultName;
    if (windowedSuffix)
        name += " (Windowed)";
    return Interpreter{QUuid::createUuid().toString(), name, python};
}

static InterpreterOptionsPage &interpreterOptionsPage()
{
    static InterpreterOptionsPage page;
    return page;
}

void InterpreterOptionsWidget::makeDefault()
{
    const QModelIndex &index = m_view.currentIndex();
    if (index.isValid()) {
        QModelIndex defaultIndex;
        if (auto *defaultItem = m_model.findItemByData(
                [this](const Interpreter &interpreter) { return interpreter.id == m_defaultId; })) {
            defaultIndex = m_model.indexForItem(defaultItem);
        }
        m_defaultId = m_model.itemAt(index.row())->itemData.id;
        emit m_model.dataChanged(index, index, {Qt::FontRole});
        if (defaultIndex.isValid())
            emit m_model.dataChanged(defaultIndex, defaultIndex, {Qt::FontRole});
    }
}

constexpr char settingsGroupKey[] = "Python";
constexpr char interpreterKey[] = "Interpeter";
constexpr char defaultKey[] = "DefaultInterpeter";

struct SavedSettings
{
    QList<Interpreter> pythons;
    QString defaultId;
};

static SavedSettings fromSettings(QSettings *settings)
{
    QList<Interpreter> pythons;
    settings->beginGroup(settingsGroupKey);
    const QVariantList interpreters = settings->value(interpreterKey).toList();
    for (const QVariant &interpreterVar : interpreters) {
        auto interpreterList = interpreterVar.toList();
        if (interpreterList.size() != 3)
            continue;
        pythons << Interpreter{interpreterList.value(0).toString(),
                               interpreterList.value(1).toString(),
                               FilePath::fromVariant(interpreterList.value(2))};
    }

    const QString defaultId = settings->value(defaultKey).toString();

    settings->endGroup();

    return {pythons, defaultId};
}

static void toSettings(QSettings *settings, const SavedSettings &savedSettings)
{
    settings->beginGroup(settingsGroupKey);
    const QVariantList interpretersVar
        = Utils::transform(savedSettings.pythons, [](const Interpreter &interpreter) {
              return QVariant({interpreter.id, interpreter.name, interpreter.command.toVariant()});
          });
    settings->setValue(interpreterKey, interpretersVar);
    settings->setValue(defaultKey, savedSettings.defaultId);
    settings->endGroup();
}

static void addPythonsFromRegistry(QList<Interpreter> &pythons)
{
    QSettings pythonRegistry("HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore",
                             QSettings::NativeFormat);
    for (const QString &versionGroup : pythonRegistry.childGroups()) {
        pythonRegistry.beginGroup(versionGroup);
        QString name = pythonRegistry.value("DisplayName").toString();
        QVariant regVal = pythonRegistry.value("InstallPath/ExecutablePath");
        if (regVal.isValid()) {
            const FilePath &executable = FilePath::fromUserInput(regVal.toString());
            if (executable.exists() && !alreadyRegistered(pythons, executable)) {
                pythons << Interpreter{QUuid::createUuid().toString(),
                                       name,
                                       FilePath::fromUserInput(regVal.toString())};
            }
        }
        regVal = pythonRegistry.value("InstallPath/WindowedExecutablePath");
        if (regVal.isValid()) {
            const FilePath &executable = FilePath::fromUserInput(regVal.toString());
            if (executable.exists() && !alreadyRegistered(pythons, executable)) {
                pythons << Interpreter{QUuid::createUuid().toString(),
                                       name + InterpreterOptionsPage::tr(" (Windowed)"),
                                       FilePath::fromUserInput(regVal.toString())};
            }
        }
        regVal = pythonRegistry.value("InstallPath/.");
        if (regVal.isValid()) {
            const FilePath &path = FilePath::fromUserInput(regVal.toString());
            const FilePath &python = path.pathAppended(HostOsInfo::withExecutableSuffix("python"));
            if (python.exists() && !alreadyRegistered(pythons, python))
                pythons << interpreterForPythonExecutable(python, "Python " + versionGroup);
            const FilePath &pythonw = path.pathAppended(
                HostOsInfo::withExecutableSuffix("pythonw"));
            if (pythonw.exists() && !alreadyRegistered(pythons, pythonw))
                pythons << interpreterForPythonExecutable(pythonw, "Python " + versionGroup, true);
        }
        pythonRegistry.endGroup();
    }
}

static void addPythonsFromPath(QList<Interpreter> &pythons)
{
    const auto &env = Environment::systemEnvironment();

    if (HostOsInfo::isWindowsHost()) {
        for (const FilePath &executable : env.findAllInPath("python")) {
            if (executable.exists() && !alreadyRegistered(pythons, executable))
                pythons << interpreterForPythonExecutable(executable, "Python from Path");
        }
        for (const FilePath &executable : env.findAllInPath("pythonw")) {
            if (executable.exists() && !alreadyRegistered(pythons, executable))
                pythons << interpreterForPythonExecutable(executable, "Python from Path", true);
        }
    } else {
        const QStringList filters = {"python",
                                     "python[1-9].[0-9]",
                                     "python[1-9].[1-9][0-9]",
                                     "python[1-9]"};
        for (const FilePath &path : env.path()) {
            const QDir dir(path.toString());
            for (const QFileInfo &fi : dir.entryInfoList(filters)) {
                const FilePath executable = Utils::FilePath::fromFileInfo(fi);
                if (executable.exists() && !alreadyRegistered(pythons, executable))
                    pythons << interpreterForPythonExecutable(executable, "Python from Path");
            }
        }
    }
}

static QString idForPythonFromPath(QList<Interpreter> pythons)
{
    const FilePath &pythonFromPath = Environment::systemEnvironment().searchInPath("python");
    if (pythonFromPath.isEmpty())
        return {};
    const Interpreter &defaultInterpreter
        = findOrDefault(pythons, [pythonFromPath](const Interpreter &interpreter) {
              return interpreter.command == pythonFromPath;
          });
    return defaultInterpreter.id;
}

static PythonSettings *settingsInstance = nullptr;
PythonSettings::PythonSettings() = default;

void PythonSettings::init()
{
    QTC_ASSERT(!settingsInstance, return );
    settingsInstance = new PythonSettings();

    const SavedSettings &settings = fromSettings(Core::ICore::settings());
    QList<Interpreter> pythons = settings.pythons;

    if (HostOsInfo::isWindowsHost())
        addPythonsFromRegistry(pythons);
    addPythonsFromPath(pythons);

    const QString &defaultId = !settings.defaultId.isEmpty() ? settings.defaultId
                                                             : idForPythonFromPath(pythons);
    setInterpreter(pythons, defaultId);
}

void PythonSettings::setInterpreter(const QList<Interpreter> &interpreters, const QString &defaultId)
{
    interpreterOptionsPage().setInterpreter(interpreters);
    interpreterOptionsPage().setDefaultInterpreter(defaultId);
    toSettings(Core::ICore::settings(), {interpreters, defaultId});
    if (QTC_GUARD(settingsInstance))
        emit settingsInstance->interpretersChanged(interpreters, defaultId);
}

PythonSettings *PythonSettings::instance()
{
    QTC_CHECK(settingsInstance);
    return settingsInstance;
}

QList<Interpreter> PythonSettings::interpreters()
{
    return interpreterOptionsPage().interpreters();
}

Interpreter PythonSettings::defaultInterpreter()
{
    return interpreterOptionsPage().defaultInterpreter();
}

} // namespace Internal
} // namespace Python

#include "pythonsettings.moc"
