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

#include "designmodewidget.h"
#include "switchsplittabwidget.h"

#include <designeractionmanager.h>

#include "qmldesignerplugin.h"
#include "crumblebar.h"
#include "documentwarningwidget.h"

#include <texteditor/textdocument.h>
#include <nodeinstanceview.h>
#include <itemlibrarywidget.h>
#include <theme.h>

#include <coreplugin/outputpane.h>
#include <coreplugin/modemanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/designmode.h>
#include <coreplugin/icore.h>
#include <coreplugin/minisplitter.h>
#include <coreplugin/sidebar.h>
#include <coreplugin/editortoolbar.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/idocument.h>
#include <coreplugin/inavigationwidgetfactory.h>

#include <utils/fileutils.h>
#include <utils/qtcassert.h>

#include <QSettings>
#include <QToolBar>
#include <QLayout>
#include <QBoxLayout>

using Core::MiniSplitter;
using Core::IEditor;
using Core::EditorManager;

using namespace QmlDesigner;

enum {
    debug = false
};

const char SB_PROJECTS[] = "Projects";
const char SB_FILESYSTEM[] = "FileSystem";
const char SB_OPENDOCUMENTS[] = "OpenDocuments";

static void hideToolButtons(QList<QToolButton*> &buttons)
{
    foreach (QToolButton *button, buttons)
        button->hide();
}

namespace QmlDesigner {
namespace Internal {

class ItemLibrarySideBarItem : public Core::SideBarItem
{
public:
    explicit ItemLibrarySideBarItem(QWidget *widget, const QString &id);
    ~ItemLibrarySideBarItem() override;

    QList<QToolButton *> createToolBarWidgets() override;
};

ItemLibrarySideBarItem::ItemLibrarySideBarItem(QWidget *widget, const QString &id) : Core::SideBarItem(widget, id) {}

ItemLibrarySideBarItem::~ItemLibrarySideBarItem() = default;

QList<QToolButton *> ItemLibrarySideBarItem::createToolBarWidgets()
{
    return qobject_cast<ItemLibraryWidget*>(widget())->createToolBarWidgets();
}

class DesignerSideBarItem : public Core::SideBarItem
{
public:
    explicit DesignerSideBarItem(QWidget *widget, WidgetInfo::ToolBarWidgetFactoryInterface *createToolBarWidgets, const QString &id);
    ~DesignerSideBarItem() override;

    QList<QToolButton *> createToolBarWidgets() override;

private:
    WidgetInfo::ToolBarWidgetFactoryInterface *m_toolBarWidgetFactory;

};

DesignerSideBarItem::DesignerSideBarItem(QWidget *widget, WidgetInfo::ToolBarWidgetFactoryInterface *toolBarWidgetFactory, const QString &id)
    : Core::SideBarItem(widget, id) , m_toolBarWidgetFactory(toolBarWidgetFactory)
{
}

DesignerSideBarItem::~DesignerSideBarItem()
{
    delete m_toolBarWidgetFactory;
}

QList<QToolButton *> DesignerSideBarItem::createToolBarWidgets()
{
    if (m_toolBarWidgetFactory)
        return m_toolBarWidgetFactory->createToolBarWidgets();

    return QList<QToolButton *>();
}

// ---------- DesignModeWidget
DesignModeWidget::DesignModeWidget()
    : m_toolBar(new Core::EditorToolBar(this))
    , m_crumbleBar(new CrumbleBar(this))
{
}

DesignModeWidget::~DesignModeWidget()
{
    m_leftSideBar.reset();
    m_rightSideBar.reset();

    foreach (QPointer<QWidget> widget, m_viewWidgets) {
        if (widget)
            widget.clear();
    }
}

void DesignModeWidget::restoreDefaultView()
{
    QSettings *settings = Core::ICore::settings();
    m_leftSideBar->closeAllWidgets();
    m_rightSideBar->closeAllWidgets();
    m_leftSideBar->readSettings(settings,  "none.LeftSideBar");
    m_rightSideBar->readSettings(settings, "none.RightSideBar");
    m_leftSideBar->show();
    m_rightSideBar->show();
}

void DesignModeWidget::toggleLeftSidebar()
{
    if (m_leftSideBar)
        m_leftSideBar->setVisible(!m_leftSideBar->isVisible());
}

void DesignModeWidget::toggleRightSidebar()
{
    if (m_rightSideBar)
        m_rightSideBar->setVisible(!m_rightSideBar->isVisible());
}

QWidget *DesignModeWidget::createProjectExplorerWidget(QWidget *parent)
{
    const QList<Core::INavigationWidgetFactory *> factories =
            Core::INavigationWidgetFactory::allNavigationFactories();

    Core::NavigationView navigationView;
    navigationView.widget = nullptr;

    foreach (Core::INavigationWidgetFactory *factory, factories) {
        if (factory->id() == "Projects") {
            navigationView = factory->createWidget();
            hideToolButtons(navigationView.dockToolBarWidgets);
        }
    }

    if (navigationView.widget) {
        QByteArray sheet = Utils::FileReader::fetchQrc(":/qmldesigner/stylesheet.css");
        sheet += Utils::FileReader::fetchQrc(":/qmldesigner/scrollbar.css");
        sheet += "QLabel { background-color: #4f4f4f; }";
        navigationView.widget->setStyleSheet(Theme::replaceCssColors(QString::fromUtf8(sheet)));
        navigationView.widget->setParent(parent);
    }

    return navigationView.widget;
}

void DesignModeWidget::readSettings()
{
    QSettings *settings = Core::ICore::settings();

    settings->beginGroup("Bauhaus");
    m_leftSideBar->readSettings(settings, QStringLiteral("LeftSideBar"));
    m_rightSideBar->readSettings(settings, QStringLiteral("RightSideBar"));
    if (settings->contains("MainSplitter")) {
        const QByteArray splitterState = settings->value("MainSplitter").toByteArray();
        m_mainSplitter->restoreState(splitterState);
        m_mainSplitter->setOpaqueResize(); // force opaque resize since it used to be off
    }
    settings->endGroup();
}

void DesignModeWidget::saveSettings()
{
    QSettings *settings = Core::ICore::settings();

    settings->beginGroup("Bauhaus");
    m_leftSideBar->saveSettings(settings, QStringLiteral("LeftSideBar"));
    m_rightSideBar->saveSettings(settings, QStringLiteral("RightSideBar"));
    settings->setValue("MainSplitter", m_mainSplitter->saveState());
    settings->endGroup();
}

void DesignModeWidget::enableWidgets()
{
    if (debug)
        qDebug() << Q_FUNC_INFO;

    viewManager().enableWidgets();
    m_isDisabled = false;
}

void DesignModeWidget::disableWidgets()
{
    if (debug)
        qDebug() << Q_FUNC_INFO;

    viewManager().disableWidgets();
    m_isDisabled = true;
}

void DesignModeWidget::switchTextOrForm()
{
    if (m_centralTabWidget->currentWidget() == viewManager().widget("TextEditor"))
        m_centralTabWidget->switchTo(viewManager().widget("FormEditor"));
    else
        m_centralTabWidget->switchTo(viewManager().widget("TextEditor"));
}

void DesignModeWidget::setup()
{
    auto &actionManager = viewManager().designerActionManager();
    actionManager.createDefaultDesignerActions();
    actionManager.createDefaultAddResourceHandler();
    actionManager.polishActions();

    QList<Core::INavigationWidgetFactory *> factories =
            Core::INavigationWidgetFactory::allNavigationFactories();

    QWidget *openDocumentsWidget = nullptr;
    QWidget *projectsExplorer = nullptr;
    QWidget *fileSystemExplorer = nullptr;

    foreach (Core::INavigationWidgetFactory *factory, factories) {
        Core::NavigationView navigationView;
        navigationView.widget = nullptr;
        if (factory->id() == "Projects") {
            navigationView = factory->createWidget();
            projectsExplorer = navigationView.widget;
            hideToolButtons(navigationView.dockToolBarWidgets);
            projectsExplorer->setWindowTitle(tr("Projects"));
        } else if (factory->id() == "File System") {
            navigationView = factory->createWidget();
            fileSystemExplorer = navigationView.widget;
            hideToolButtons(navigationView.dockToolBarWidgets);
            fileSystemExplorer->setWindowTitle(tr("File System"));
        } else if (factory->id() == "Open Documents") {
            navigationView = factory->createWidget();
            openDocumentsWidget = navigationView.widget;
            hideToolButtons(navigationView.dockToolBarWidgets);
            openDocumentsWidget->setWindowTitle(tr("Open Documents"));
        }

        if (navigationView.widget) {
            QByteArray sheet = Utils::FileReader::fetchQrc(":/qmldesigner/stylesheet.css");
            sheet += Utils::FileReader::fetchQrc(":/qmldesigner/scrollbar.css");
            sheet += "QLabel { background-color: #4f4f4f; }";
            navigationView.widget->setStyleSheet(Theme::replaceCssColors(QString::fromUtf8(sheet)));
        }
    }

    auto toolBar = new QToolBar;
    toolBar->addAction(viewManager().componentViewAction());
    toolBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    DesignerActionToolBar *designerToolBar = QmlDesignerPlugin::instance()->viewManager().designerActionManager().createToolBar(m_toolBar);

    designerToolBar->layout()->addWidget(toolBar);

    m_toolBar->addCenterToolBar(designerToolBar);

    m_toolBar->setMinimumWidth(320);

    m_mainSplitter = new MiniSplitter(this);
    m_mainSplitter->setObjectName("mainSplitter");

    QList<Core::SideBarItem*> sideBarItems;
    QList<Core::SideBarItem*> leftSideBarItems;
    QList<Core::SideBarItem*> rightSideBarItems;

    foreach (const WidgetInfo &widgetInfo, viewManager().widgetInfos()) {
        if (widgetInfo.placementHint == widgetInfo.LeftPane) {
            Core::SideBarItem *sideBarItem = new DesignerSideBarItem(widgetInfo.widget, widgetInfo.toolBarWidgetFactory, widgetInfo.uniqueId);
            sideBarItems.append(sideBarItem);
            leftSideBarItems.append(sideBarItem);
        }

        if (widgetInfo.placementHint == widgetInfo.RightPane) {
            Core::SideBarItem *sideBarItem = new DesignerSideBarItem(widgetInfo.widget, widgetInfo.toolBarWidgetFactory, widgetInfo.uniqueId);
            sideBarItems.append(sideBarItem);
            rightSideBarItems.append(sideBarItem);

        }
        m_viewWidgets.append(widgetInfo.widget);
    }

    if (projectsExplorer) {
        Core::SideBarItem *projectExplorerItem = new Core::SideBarItem(projectsExplorer, QLatin1String(SB_PROJECTS));
        sideBarItems.append(projectExplorerItem);
    }

    if (fileSystemExplorer) {
        Core::SideBarItem *fileSystemExplorerItem = new Core::SideBarItem(fileSystemExplorer, QLatin1String(SB_FILESYSTEM));
        sideBarItems.append(fileSystemExplorerItem);
    }

    if (openDocumentsWidget) {
        Core::SideBarItem *openDocumentsItem = new Core::SideBarItem(openDocumentsWidget, QLatin1String(SB_OPENDOCUMENTS));
        sideBarItems.append(openDocumentsItem);
    }

    m_leftSideBar.reset(new Core::SideBar(sideBarItems, leftSideBarItems));
    m_rightSideBar.reset(new Core::SideBar(sideBarItems, rightSideBarItems));

    connect(m_leftSideBar.data(), &Core::SideBar::availableItemsChanged, [=](){
        // event comes from m_leftSidebar, so update right side.
        m_rightSideBar->setUnavailableItemIds(m_leftSideBar->unavailableItemIds());
    });

    connect(m_rightSideBar.data(), &Core::SideBar::availableItemsChanged, [=](){
        // event comes from m_rightSidebar, so update left side.
        m_leftSideBar->setUnavailableItemIds(m_rightSideBar->unavailableItemIds());
    });

    connect(Core::ICore::instance(), &Core::ICore::coreAboutToClose, [=](){
        m_leftSideBar.reset();
        m_rightSideBar.reset();
    });

    m_toolBar->setToolbarCreationFlags(Core::EditorToolBar::FlagsStandalone);
    m_toolBar->setNavigationVisible(true);

    connect(m_toolBar, &Core::EditorToolBar::goForwardClicked, this, &DesignModeWidget::toolBarOnGoForwardClicked);
    connect(m_toolBar, &Core::EditorToolBar::goBackClicked, this, &DesignModeWidget::toolBarOnGoBackClicked);

    if (currentDesignDocument())
        setupNavigatorHistory(currentDesignDocument()->textEditor());

    // m_mainSplitter area:
    m_mainSplitter->addWidget(m_leftSideBar.data());
    m_mainSplitter->addWidget(createCenterWidget());
    m_mainSplitter->addWidget(m_rightSideBar.data());

    // Finishing touches:
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setSizes({150, 300, 150});

    QLayout *mainLayout = new QBoxLayout(QBoxLayout::RightToLeft, this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_mainSplitter);

    viewManager().enableWidgets();
    m_leftSideBar->setEnabled(true);
    m_rightSideBar->setEnabled(true);
    m_leftSideBar->setCloseWhenEmpty(false);
    m_rightSideBar->setCloseWhenEmpty(false);

    readSettings();

    show();
}

void DesignModeWidget::toolBarOnGoBackClicked()
{
    if (m_navigatorHistoryCounter > 0) {
        --m_navigatorHistoryCounter;
        m_keepNavigatorHistory = true;
        Core::EditorManager::openEditor(m_navigatorHistory.at(m_navigatorHistoryCounter),
                                        Core::Id(), Core::EditorManager::DoNotMakeVisible);
        m_keepNavigatorHistory = false;
    }
}

void DesignModeWidget::toolBarOnGoForwardClicked()
{
    if (m_navigatorHistoryCounter < (m_navigatorHistory.size() - 1)) {
        ++m_navigatorHistoryCounter;
        m_keepNavigatorHistory = true;
        Core::EditorManager::openEditor(m_navigatorHistory.at(m_navigatorHistoryCounter),
                                        Core::Id(), Core::EditorManager::DoNotMakeVisible);
        m_keepNavigatorHistory = false;
    }
}

DesignDocument *DesignModeWidget::currentDesignDocument() const
{
    return QmlDesignerPlugin::instance()->documentManager().currentDesignDocument();
}

ViewManager &DesignModeWidget::viewManager()
{
    return QmlDesignerPlugin::instance()->viewManager();
}

void DesignModeWidget::setupNavigatorHistory(Core::IEditor *editor)
{
    if (!m_keepNavigatorHistory)
        addNavigatorHistoryEntry(editor->document()->filePath());

    const bool canGoBack = m_navigatorHistoryCounter > 0;
    const bool canGoForward = m_navigatorHistoryCounter < (m_navigatorHistory.size() - 1);
    m_toolBar->setCanGoBack(canGoBack);
    m_toolBar->setCanGoForward(canGoForward);
    m_toolBar->setCurrentEditor(editor);
}

void DesignModeWidget::addNavigatorHistoryEntry(const Utils::FilePath &fileName)
{
    if (m_navigatorHistoryCounter > 0)
        m_navigatorHistory.insert(m_navigatorHistoryCounter + 1, fileName.toString());
    else
        m_navigatorHistory.append(fileName.toString());

    ++m_navigatorHistoryCounter;
}

static QTabWidget *createWidgetsInTabWidget(const QList<WidgetInfo> &widgetInfos)
{
    auto tabWidget = new QTabWidget;

    foreach (const WidgetInfo &widgetInfo, widgetInfos)
        tabWidget->addTab(widgetInfo.widget, widgetInfo.tabName);

    return tabWidget;
}

static QWidget *createbottomSideBarWidget(const QList<WidgetInfo> &widgetInfos)
{
    //### we now own these here
    QList<WidgetInfo> topWidgetInfos;
    foreach (const WidgetInfo &widgetInfo, widgetInfos) {
        if (widgetInfo.placementHint == widgetInfo.BottomPane)
            topWidgetInfos.append(widgetInfo);
    }

    QWidget *widget = topWidgetInfos.constFirst().widget;
    if (topWidgetInfos.count() > 1) {
        QWidget *background = new QWidget();
        background->setProperty("designerBackgroundColor", true);

        QString sheet = QString::fromUtf8(Utils::FileReader::fetchQrc(":/qmldesigner/stylesheet.css"));
        sheet.prepend("QWidget[designerBackgroundColor=\"true\"] {background-color: creatorTheme.QmlDesigner_BackgroundColorDarkAlternate;}");

        background->setStyleSheet(Theme::replaceCssColors(sheet));
        background->setLayout(new QVBoxLayout);
        background->layout()->setContentsMargins(0, 0, 0, 0);

        background->layout()->addWidget(createWidgetsInTabWidget(topWidgetInfos));
        widget = background;
    }
    return widget;
}

static Core::MiniSplitter *createCentralSplitter(const QList<WidgetInfo> &widgetInfos)
{
    // editor and output panes
    auto outputPlaceholderSplitter = new Core::MiniSplitter;
    outputPlaceholderSplitter->setOrientation(Qt::Vertical);

    auto switchSplitTabWidget = new SwitchSplitTabWidget();

    foreach (const WidgetInfo &widgetInfo, widgetInfos) {
        if (widgetInfo.placementHint == widgetInfo.CentralPane)
            switchSplitTabWidget->addTab(widgetInfo.widget, widgetInfo.tabName);
    }

    outputPlaceholderSplitter->addWidget(switchSplitTabWidget);

    QWidget *bottomSideBar = createbottomSideBarWidget(widgetInfos);
    bottomSideBar->setObjectName("bottomSideBar");
    outputPlaceholderSplitter->addWidget(bottomSideBar);

    auto outputPanePlaceholder = new Core::OutputPanePlaceHolder(Core::Constants::MODE_DESIGN, outputPlaceholderSplitter);
    outputPlaceholderSplitter->addWidget(outputPanePlaceholder);

    outputPlaceholderSplitter->setStretchFactor(0, 10);
    outputPlaceholderSplitter->setStretchFactor(1, 1);
    return outputPlaceholderSplitter;
}

QWidget *DesignModeWidget::createCenterWidget()
{
    QWidget *centerWidget = new QWidget;

    auto horizontalLayout = new QVBoxLayout(centerWidget);
    horizontalLayout->setContentsMargins(0, 0, 0, 0);
    horizontalLayout->setSpacing(0);

    horizontalLayout->addWidget(m_toolBar);
    horizontalLayout->addWidget(createCrumbleBarFrame());

    Core::MiniSplitter *centralSplitter = createCentralSplitter(viewManager().widgetInfos());
    m_centralTabWidget = centralSplitter->findChild<SwitchSplitTabWidget*>();
    Q_ASSERT(m_centralTabWidget);
    m_centralTabWidget->switchTo(viewManager().widget("FormEditor"));

    m_bottomSideBar = centralSplitter->findChild<QWidget*>("bottomSideBar");
    Q_ASSERT(m_bottomSideBar);
    horizontalLayout->addWidget(centralSplitter);

    return centerWidget;
}

QWidget *DesignModeWidget::createCrumbleBarFrame()
{
    auto frame = new Utils::StyledBar(this);
    frame->setSingleRow(false);
    auto layout = new QHBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_crumbleBar->crumblePath());

    return frame;
}

CrumbleBar *DesignModeWidget::crumbleBar() const
{
    return m_crumbleBar;
}

void DesignModeWidget::showInternalTextEditor()
{
    m_centralTabWidget->switchTo(viewManager().widget("TextEditor"));
}

void DesignModeWidget::contextHelp(const Core::IContext::HelpCallback &callback) const
{
    if (currentDesignDocument())
        currentDesignDocument()->contextHelp(callback);
    else
        callback({});
}

void DesignModeWidget::initialize()
{
    if (m_initStatus == NotInitialized) {
        m_initStatus = Initializing;
        setup();
    }

    m_initStatus = Initialized;
}

} // namespace Internal
} // namespace Designer
