/*=====================================================================

QGroundControl Open Source Ground Control Station

(c) 2009 - 2011 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>

This file is part of the QGROUNDCONTROL project

    QGROUNDCONTROL is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    QGROUNDCONTROL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with QGROUNDCONTROL. If not, see <http://www.gnu.org/licenses/>.

======================================================================*/

/**
 * @file
 *   @brief Implementation of class MainWindow
 *   @author Lorenz Meier <mail@qgroundcontrol.org>
 */

#include <QSettings>
#include <QDockWidget>
#include <QNetworkInterface>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QHostInfo>
#include <QSplashScreen>
#include <qmath.h>                      // Beginn nd Ende Code MA (07.03.2012)

#include "QGC.h"
#include "MAVLinkSimulationLink.h"
#include "SerialLink.h"
#include "UDPLink.h"
#include "MAVLinkProtocol.h"
#include "CommConfigurationWindow.h"
#include "QGCWaypointListMulti.h"
#include "MainWindow.h"
#include "GAudioOutput.h"
#include "QGCToolWidget.h"
#include "QGCMAVLinkLogPlayer.h"
#include "QGCSettingsWidget.h"
#include "QGCMapTool.h"
#include "MAVLinkDecoder.h"
#include "QGCMAVLinkMessageSender.h"
#include "QGCRGBDView.h"
#include "QGCFirmwareUpdate.h"
#include "SkyeMAV.h"

#ifdef QGC_OSG_ENABLED
#include "Q3DWidgetFactory.h"
#endif

// FIXME Move
#include "PxQuadMAV.h"
#include "SlugsMAV.h"


#include "LogCompressor.h"

#ifdef MOUSE_ENABLED                    // Beginn Code MA (06.03.2012)
#include <QX11Info>
#include <X11/Xlib.h>
#undef Success              // Eigen library doesn't work if Success is defined
extern "C"
{
#include "xdrvlib.h"
}
#endif // MOUSE_ENABLED                 // Ende Code MA (06.03.2012)

MainWindow* MainWindow::instance(QSplashScreen* screen)
{
    static MainWindow* _instance = 0;
    if(_instance == 0)
    {
        _instance = new MainWindow();
        if (screen) connect(_instance, SIGNAL(initStatusChanged(QString)), screen, SLOT(showMessage(QString)));

        /* Set the application as parent to ensure that this object
                 * will be destroyed when the main application exits */
        //_instance->setParent(qApp);
    }
    return _instance;
}

/**
* Create new mainwindow. The constructor instantiates all parts of the user
* interface. It does NOT show the mainwindow. To display it, call the show()
* method.
*
* @see QMainWindow::show()
**/
MainWindow::MainWindow(QWidget *parent):
    QMainWindow(parent),
    currentView(VIEW_UNCONNECTED),
    currentStyle(QGC_MAINWINDOW_STYLE_INDOOR),
    aboutToCloseFlag(false),
    changingViewsFlag(false),
    centerStackActionGroup(new QActionGroup(this)),
    styleFileName(QCoreApplication::applicationDirPath() + "/style-indoor.css"),
    autoReconnect(false),
    lowPowerMode(false),
    inputMode(UASSkyeControlWidget::QGC_INPUT_MODE_NONE),
    #ifdef MOUSE_ENABLED                                        // Beginn Code MA
    mouseTranslationEnable(true),
    mouseRotationEnable(true),
    mouseInitialized(false),
    #endif                                                      // Ende Code MA (21.03.2012)
    keyXValue(0),
    keyYValue(0),
    keyZValue(0),
    keyPitchValue(0),
    keyYawValue(0)
{
    hide();
    emit initStatusChanged("Loading UI Settings..");
    loadSettings();
    if (!settings.contains("CURRENT_VIEW"))
    {
        // Set this view as default view
        settings.setValue("CURRENT_VIEW", currentView);
    }
    else
    {
        // LOAD THE LAST VIEW
        VIEW_SECTIONS currentViewCandidate = (VIEW_SECTIONS) settings.value("CURRENT_VIEW", currentView).toInt();
        if (currentViewCandidate != VIEW_ENGINEER &&
                currentViewCandidate != VIEW_OPERATOR &&
                currentViewCandidate != VIEW_SKYE && //Beginn und Ende Code AL (01.03.12)--------------------------------------------------
                currentViewCandidate != VIEW_PILOT &&
                currentViewCandidate != VIEW_FULL)
        {
            currentView = currentViewCandidate;
        }
    }

    settings.sync();

    emit initStatusChanged("Loading Style.");
    loadStyle(currentStyle);

    emit initStatusChanged("Setting up user interface.");

    // Setup user interface
    ui.setupUi(this);
    hide();

    // Set dock options
    setDockOptions(AnimatedDocks | AllowTabbedDocks | AllowNestedDocks);
    statusBar()->setSizeGripEnabled(true);

    configureWindowName();

    // Setup corners
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    // Setup UI state machines
	centerStackActionGroup->setExclusive(true);

    centerStack = new QStackedWidget(this);
    setCentralWidget(centerStack);

    // Load Toolbar
    toolBar = new QGCToolBar(this);
    this->addToolBar(toolBar);
    // Add actions
    toolBar->addPerspectiveChangeAction(ui.actionOperatorsView);
    toolBar->addPerspectiveChangeAction(ui.actionEngineersView);
    toolBar->addPerspectiveChangeAction(ui.actionPilotsView);
    toolBar->addPerspectiveChangeAction(ui.actionSkyeView);                 // Beginn und Ende Code MA (15.05.2012)

    emit initStatusChanged("Building common widgets.");

    buildCommonWidgets();
    connectCommonWidgets();

    emit initStatusChanged("Building common actions.");

    // Create actions
    connectCommonActions();

    // Populate link menu
    emit initStatusChanged("Populating link menu");
    QList<LinkInterface*> links = LinkManager::instance()->getLinks();
    foreach(LinkInterface* link, links)
    {
        this->addLink(link);
    }

    connect(LinkManager::instance(), SIGNAL(newLink(LinkInterface*)), this, SLOT(addLink(LinkInterface*)));

    // Connect user interface devices
    emit initStatusChanged("Initializing joystick interface.");
    joystickWidget = 0;
    //joystick = new JoystickInput();                               // Modified Code MA (13.03.2012) ----- Disabled JoystickThread

    testphaseWidget = 0;                                            //Start Ende Code AL (19.03.12)
    skyeCameraReconfigureWidget = 0;                                //Beginn und Ende Code MA (20.03.2012)

    // Connect link
    if (autoReconnect)
    {
        SerialLink* link = new SerialLink();
        // Add to registry
        LinkManager::instance()->add(link);
        LinkManager::instance()->addProtocol(link, mavlink);
        link->connect();
    }

    // Set low power mode
    enableLowPowerMode(lowPowerMode);

    // Initialize window state
    windowStateVal = windowState();

    emit initStatusChanged("Restoring last view state.");

    // Restore the window setup
    loadViewState();

    emit initStatusChanged("Restoring last window size.");
    // Restore the window position and size
    if (settings.contains(getWindowGeometryKey()))
    {
        // Restore the window geometry
        restoreGeometry(settings.value(getWindowGeometryKey()).toByteArray());
        show();
    }
    else
    {
        // Adjust the size
        const int screenWidth = QApplication::desktop()->width();
        const int screenHeight = QApplication::desktop()->height();

        if (screenWidth < 1200)
        {
            showFullScreen();
        }
        else
        {
            resize(screenWidth*0.67f, qMin(screenHeight, (int)(screenWidth*0.67f*0.67f)));
            show();
        }

    }

    connect(&windowNameUpdateTimer, SIGNAL(timeout()), this, SLOT(configureWindowName()));
    windowNameUpdateTimer.start(15000);
    emit initStatusChanged("Done.");

#ifdef MOUSE_ENABLED                    // Beginn Code MA (21.03.2012)
    start3dMouse();
#endif // MOUSE_ENABLED                 // Ende Code MA

    show();
}

MainWindow::~MainWindow()
{
    if (mavlink)
    {
        delete mavlink;
        mavlink = NULL;
    }
//    if (simulationLink)
//    {
//        simulationLink->deleteLater();
//        simulationLink = NULL;
//    }
    if (joystick)
    {
        delete joystick;
        joystick = NULL;
    }

    // Get and delete all dockwidgets and contained
    // widgets
    QObjectList childList(this->children());

    QObjectList::iterator i;
    QDockWidget* dockWidget;
    for (i = childList.begin(); i != childList.end(); ++i)
    {
        dockWidget = dynamic_cast<QDockWidget*>(*i);
        if (dockWidget)
        {
            // Remove dock widget from main window
            // removeDockWidget(dockWidget);
            // delete dockWidget->widget();
            delete dockWidget;
            dockWidget = NULL;
        }
        else if (dynamic_cast<QWidget*>(*i))
        {
            delete dynamic_cast<QWidget*>(*i);
            *i = NULL;
        }
    }
    // Delete all UAS objects
}

void MainWindow::resizeEvent(QResizeEvent * event)
{
    if (height() < 800)
    {
        ui.statusBar->setVisible(false);
    }
    else
    {
        ui.statusBar->setVisible(true);
        ui.statusBar->setSizeGripEnabled(true);
    }

    if (width() > 1200)
    {
        toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    else
    {
        toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    }

    QMainWindow::resizeEvent(event);
}

QString MainWindow::getWindowStateKey()
{
    return QString::number(currentView)+"_windowstate";
}

QString MainWindow::getWindowGeometryKey()
{
    //return QString::number(currentView)+"_geometry";
    return "_geometry";
}

void MainWindow::buildCustomWidget()
{
    // Create custom widgets
    QList<QGCToolWidget*> widgets = QGCToolWidget::createWidgetsFromSettings(this);

    if (widgets.size() > 0)
    {
        ui.menuTools->addSeparator();
    }

    for(int i = 0; i < widgets.size(); ++i)
    {
        // Check if this widget already has a parent, do not create it in this case
        QGCToolWidget* tool = widgets.at(i);
        QDockWidget* dock = dynamic_cast<QDockWidget*>(tool->parentWidget());
        if (!dock)
        {
            QDockWidget* dock = new QDockWidget(tool->windowTitle(), this);
            dock->setObjectName(tool->objectName()+"_DOCK");
            dock->setWidget(tool);
            connect(tool, SIGNAL(destroyed()), dock, SLOT(deleteLater()));
            QAction* showAction = new QAction(widgets.at(i)->windowTitle(), this);
            showAction->setCheckable(true);
            connect(showAction, SIGNAL(triggered(bool)), dock, SLOT(setVisible(bool)));
            connect(dock, SIGNAL(visibilityChanged(bool)), showAction, SLOT(setChecked(bool)));
            widgets.at(i)->setMainMenuAction(showAction);
            ui.menuTools->addAction(showAction);

            // Load dock widget location (default is bottom)
            Qt::DockWidgetArea location = static_cast <Qt::DockWidgetArea>(tool->getDockWidgetArea(currentView));

            addDockWidget(location, dock);
        }
    }
}

void MainWindow::buildCommonWidgets()
{
    //TODO:  move protocol outside UI
    mavlink     = new MAVLinkProtocol();
    connect(mavlink, SIGNAL(protocolStatusMessage(QString,QString)), this, SLOT(showCriticalMessage(QString,QString)), Qt::QueuedConnection);
    // Add generic MAVLink decoder
    mavlinkDecoder = new MAVLinkDecoder(mavlink, this);

    // Dock widgets
    if (!controlDockWidget)
    {
        controlDockWidget = new QDockWidget(tr("Control"), this);
        controlDockWidget->setObjectName("UNMANNED_SYSTEM_CONTROL_DOCKWIDGET");
        controlDockWidget->setWidget( new UASControlWidget(this) );
        addTool(controlDockWidget, tr("Control"), Qt::LeftDockWidgetArea);
    }

#ifdef MAVLINK_ENABLED_SKYE
    if (!skyeControlDockWidget)         // Beginn Code MA (06.03.2012) -----------------------
    {
        skyeControlDockWidget = new QDockWidget(tr("Skye Control"), this);
        skyeControlDockWidget->setObjectName("SKYE_CONTROL_DOCKWIDGET");
        skyeControlDockWidget->setWidget( new UASSkyeControlWidget(this) );

        UASSkyeControlWidget *uasSkyeControl = dynamic_cast<UASSkyeControlWidget*>(skyeControlDockWidget->widget());
        connect(uasSkyeControl, SIGNAL(changedInput(int)), this, SLOT(setInputMode(int)));
        connect(this, SIGNAL(mouseTranslationEnabledChanged(bool)), uasSkyeControl, SLOT(changeMouseTranslationEnabled(bool)));
        connect(this, SIGNAL(mouseRotationEnabledChanged(bool)), uasSkyeControl, SLOT(changeMouseRoatationEnabled(bool)));
        addTool(skyeControlDockWidget, tr("Skye Control"), Qt::LeftDockWidgetArea);
    }                                   // Ende Code MA (06.03.2012) --------------------------
#endif // MAVLINK_ENABLED_SKYE

    if (!listDockWidget)
    {
        listDockWidget = new QDockWidget(tr("Unmanned Systems"), this);
        listDockWidget->setWidget( new UASListWidget(this) );
        listDockWidget->setObjectName("UNMANNED_SYSTEMS_LIST_DOCKWIDGET");
        addTool(listDockWidget, tr("Unmanned Systems"), Qt::RightDockWidgetArea);
    }

    if (!waypointsDockWidget)
    {
        waypointsDockWidget = new QDockWidget(tr("Mission Plan"), this);
        waypointsDockWidget->setWidget( new QGCWaypointListMulti(this) );
        waypointsDockWidget->setObjectName("WAYPOINT_LIST_DOCKWIDGET");
        addTool(waypointsDockWidget, tr("Mission Plan"), Qt::BottomDockWidgetArea);
    }

    if (!infoDockWidget)
    {
        infoDockWidget = new QDockWidget(tr("Status Details"), this);
        infoDockWidget->setWidget( new UASInfoWidget(this) );
        infoDockWidget->setObjectName("UAS_STATUS_DETAILS_DOCKWIDGET");
        addTool(infoDockWidget, tr("Status Details"), Qt::RightDockWidgetArea);
    }

    if (!debugConsoleDockWidget)
    {
        debugConsoleDockWidget = new QDockWidget(tr("Communication Console"), this);
        debugConsoleDockWidget->setWidget( new DebugConsole(this) );
        debugConsoleDockWidget->setObjectName("COMMUNICATION_DEBUG_CONSOLE_DOCKWIDGET");

        DebugConsole *debugConsole = dynamic_cast<DebugConsole*>(debugConsoleDockWidget->widget());
        connect(mavlinkDecoder, SIGNAL(textMessageReceived(int, int, int, const QString)), debugConsole, SLOT(receiveTextMessage(int, int, int, const QString)));

        addTool(debugConsoleDockWidget, tr("Communication Console"), Qt::BottomDockWidgetArea);
    }

    if (!logPlayerDockWidget)
    {
        logPlayerDockWidget = new QDockWidget(tr("MAVLink Log Player"), this);
        logPlayer = new QGCMAVLinkLogPlayer(mavlink, this);
        toolBar->setLogPlayer(logPlayer);
        logPlayerDockWidget->setWidget(logPlayer);
        logPlayerDockWidget->setObjectName("MAVLINK_LOG_PLAYER_DOCKWIDGET");
        addTool(logPlayerDockWidget, tr("MAVLink Log Replay"), Qt::RightDockWidgetArea);
    }

    if (!mavlinkInspectorWidget)
    {
        mavlinkInspectorWidget = new QDockWidget(tr("MAVLink Message Inspector"), this);
        mavlinkInspectorWidget->setWidget( new QGCMAVLinkInspector(mavlink, this) );
        mavlinkInspectorWidget->setObjectName("MAVLINK_INSPECTOR_DOCKWIDGET");
        addTool(mavlinkInspectorWidget, tr("MAVLink Inspector"), Qt::RightDockWidgetArea);
    }

    if (!mavlinkSenderWidget)
    {
        mavlinkSenderWidget = new QDockWidget(tr("MAVLink Message Sender"), this);
        mavlinkSenderWidget->setWidget( new QGCMAVLinkMessageSender(mavlink, this) );
        mavlinkSenderWidget->setObjectName("MAVLINK_SENDER_DOCKWIDGET");
        addTool(mavlinkSenderWidget, tr("MAVLink Sender"), Qt::RightDockWidgetArea);
    }

    //FIXME: memory of acceptList will never be freed again
    QStringList* acceptList = new QStringList();
    acceptList->append("-3.3,ATTITUDE.roll,rad,+3.3,s");
    acceptList->append("-3.3,ATTITUDE.pitch,deg,+3.3,s");
    acceptList->append("-3.3,ATTITUDE.yaw,deg,+3.3,s");

    //FIXME: memory of acceptList2 will never be freed again
    QStringList* acceptList2 = new QStringList();
    acceptList2->append("0,RAW_PRESSURE.pres_abs,hPa,65500");

    if (!parametersDockWidget)
    {
        parametersDockWidget = new QDockWidget(tr("Calibration and Onboard Parameters"), this);
        parametersDockWidget->setWidget( new ParameterInterface(this) );
        parametersDockWidget->setObjectName("PARAMETER_INTERFACE_DOCKWIDGET");
        addTool(parametersDockWidget, tr("Calibration and Parameters"), Qt::RightDockWidgetArea);
    }
	
    if (!hsiDockWidget)
    {
        hsiDockWidget = new QDockWidget(tr("Horizontal Situation Indicator"), this);
        hsiDockWidget->setWidget( new HSIDisplay(this) );
        hsiDockWidget->setObjectName("HORIZONTAL_SITUATION_INDICATOR_DOCK_WIDGET");
        addTool(hsiDockWidget, tr("Horizontal Situation"), Qt::BottomDockWidgetArea);
    }
	
    if (!headDown1DockWidget)
    {
        headDown1DockWidget = new QDockWidget(tr("Flight Display"), this);
        HDDisplay* hdDisplay = new HDDisplay(acceptList, "Flight Display", this);
        hdDisplay->addSource(mavlinkDecoder);
        headDown1DockWidget->setWidget(hdDisplay);
        headDown1DockWidget->setObjectName("HEAD_DOWN_DISPLAY_1_DOCK_WIDGET");
        addTool(headDown1DockWidget, tr("Flight Display"), Qt::RightDockWidgetArea);
    }

    if (!headDown2DockWidget)
    {
        headDown2DockWidget = new QDockWidget(tr("Actuator Status"), this);
        HDDisplay* hdDisplay = new HDDisplay(acceptList2, "Actuator Status", this);
        hdDisplay->addSource(mavlinkDecoder);
        headDown2DockWidget->setWidget(hdDisplay);
        headDown2DockWidget->setObjectName("HEAD_DOWN_DISPLAY_2_DOCK_WIDGET");
        addTool(headDown2DockWidget, tr("Actuator Status"), Qt::RightDockWidgetArea);
    }
	
    if (!rcViewDockWidget)
    {
        rcViewDockWidget = new QDockWidget(tr("Radio Control"), this);
        rcViewDockWidget->setWidget( new QGCRemoteControlView(this) );
        rcViewDockWidget->setObjectName("RADIO_CONTROL_CHANNELS_DOCK_WIDGET");
        addTool(rcViewDockWidget, tr("Radio Control"), Qt::BottomDockWidgetArea);
    }

    if (!headUpDockWidget)
    {
        headUpDockWidget = new QDockWidget(tr("HUD"), this);
        headUpDockWidget->setWidget( new HUD(320, 240, this));
        headUpDockWidget->setObjectName("HEAD_UP_DISPLAY_DOCK_WIDGET");
        addTool(headUpDockWidget, tr("Head Up Display"), Qt::RightDockWidgetArea);
    }

    if (!video1DockWidget)
    {
        video1DockWidget = new QDockWidget(tr("Video Stream 1"), this);
        QGCRGBDView* video1 =  new QGCRGBDView(160, 120, this);
        video1->enableHUDInstruments(false);
        video1->enableVideo(false);
        // FIXME select video stream as well
        video1DockWidget->setWidget(video1);
        video1DockWidget->setObjectName("VIDEO_STREAM_1_DOCK_WIDGET");
        addTool(video1DockWidget, tr("Video Stream 1"), Qt::LeftDockWidgetArea);
    }

    if (!video2DockWidget)
    {
        video2DockWidget = new QDockWidget(tr("Video Stream 2"), this);
        QGCRGBDView* video2 =  new QGCRGBDView(160, 120, this);
        video2->enableHUDInstruments(false);
        video2->enableVideo(false);
        // FIXME select video stream as well
        video2DockWidget->setWidget(video2);
        video2DockWidget->setObjectName("VIDEO_STREAM_2_DOCK_WIDGET");
        addTool(video2DockWidget, tr("Video Stream 2"), Qt::LeftDockWidgetArea);
    }

//    if (!rgbd1DockWidget) {
//        rgbd1DockWidget = new QDockWidget(tr("Video Stream 1"), this);
//        HUD* video1 =  new HUD(160, 120, this);
//        video1->enableHUDInstruments(false);
//        video1->enableVideo(true);
//        // FIXME select video stream as well
//        video1DockWidget->setWidget(video1);
//        video1DockWidget->setObjectName("VIDEO_STREAM_1_DOCK_WIDGET");
//        addTool(video1DockWidget, tr("Video Stream 1"), Qt::LeftDockWidgetArea);
//    }

//    if (!rgbd2DockWidget) {
//        video2DockWidget = new QDockWidget(tr("Video Stream 2"), this);
//        HUD* video2 =  new HUD(160, 120, this);
//        video2->enableHUDInstruments(false);
//        video2->enableVideo(true);
//        // FIXME select video stream as well
//        video2DockWidget->setWidget(video2);
//        video2DockWidget->setObjectName("VIDEO_STREAM_2_DOCK_WIDGET");
//        addTool(video2DockWidget, tr("Video Stream 2"), Qt::LeftDockWidgetArea);
//    }

    // Custom widgets, added last to all menus and layouts
    buildCustomWidget();

    // Center widgets
    if (!mapWidget)
    {
        mapWidget = new QGCMapTool(this);
        addCentralWidget(mapWidget, "Maps");
    }

    if (!protocolWidget)
    {
        protocolWidget    = new XMLCommProtocolWidget(this);
        addCentralWidget(protocolWidget, "Mavlink Generator");
    }

    if (!firmwareUpdateWidget)
    {
        firmwareUpdateWidget    = new QGCFirmwareUpdate(this);
        addCentralWidget(firmwareUpdateWidget, "Firmware Update");
    }

    if (!hudWidget) {
        hudWidget         = new HUD(320, 240, this);
        addCentralWidget(hudWidget, tr("Head Up Display"));
    }

    if (!dataplotWidget) {
        dataplotWidget    = new QGCDataPlot2D(this);
        addCentralWidget(dataplotWidget, tr("Logfile Plot"));
    }

#ifdef QGC_OSG_ENABLED
    if (!_3DWidget) {
        _3DWidget         = Q3DWidgetFactory::get("PIXHAWK", this);
        addCentralWidget(_3DWidget, tr("Local 3D"));
    }
#endif

#if (defined _MSC_VER) | (defined Q_OS_MAC)
    if (!gEarthWidget) {
        gEarthWidget = new QGCGoogleEarthView(this);
        addCentralWidget(gEarthWidget, tr("Google Earth"));
    }
#endif
}

void MainWindow::addTool(QDockWidget* widget, const QString& title, Qt::DockWidgetArea area)
{
    QAction* tempAction = ui.menuTools->addAction(title);

    tempAction->setCheckable(true);
    QVariant var;
    var.setValue((QWidget*)widget);
    tempAction->setData(var);
    connect(tempAction,SIGNAL(triggered(bool)),this, SLOT(showTool(bool)));
    connect(widget, SIGNAL(visibilityChanged(bool)), tempAction, SLOT(setChecked(bool)));
    tempAction->setChecked(widget->isVisible());
    addDockWidget(area, widget);
}


void MainWindow::showTool(bool show)
{
    QAction* act = qobject_cast<QAction *>(sender());
    QWidget* widget = qVariantValue<QWidget *>(act->data());
    widget->setVisible(show);
}

void MainWindow::addCentralWidget(QWidget* widget, const QString& title)
{
    // Check if this widget already has been added
    if (centerStack->indexOf(widget) == -1)
    {
        centerStack->addWidget(widget);

        QAction* tempAction = ui.menuMain->addAction(title);

        tempAction->setCheckable(true);
        QVariant var;
        var.setValue((QWidget*)widget);
        tempAction->setData(var);
        centerStackActionGroup->addAction(tempAction);
        connect(tempAction,SIGNAL(triggered()),this, SLOT(showCentralWidget()));
        connect(widget, SIGNAL(visibilityChanged(bool)), tempAction, SLOT(setChecked(bool)));
        tempAction->setChecked(widget->isVisible());
    }
}


void MainWindow::showCentralWidget()
{
    QAction* act = qobject_cast<QAction *>(sender());
    QWidget* widget = qVariantValue<QWidget *>(act->data());
    centerStack->setCurrentWidget(widget);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (isVisible()) storeViewState();
    storeSettings();
    aboutToCloseFlag = true;
    mavlink->storeSettings();
    UASManager::instance()->storeSettings();
    QMainWindow::closeEvent(event);
}

/**
 * Connect the signals and slots of the common window widgets
 */
void MainWindow::connectCommonWidgets()
{
    if (infoDockWidget && infoDockWidget->widget())
    {
        connect(mavlink, SIGNAL(receiveLossChanged(int, float)),
                infoDockWidget->widget(), SLOT(updateSendLoss(int, float)));
    }
}

void MainWindow::createCustomWidget()
{
    QDockWidget* dock = new QDockWidget("Unnamed Tool", this);
    QGCToolWidget* tool = new QGCToolWidget("Unnamed Tool", dock);

    if (QGCToolWidget::instances()->size() < 2)
    {
        // This is the first widget
        ui.menuTools->addSeparator();
    }

    connect(tool, SIGNAL(destroyed()), dock, SLOT(deleteLater()));
    dock->setWidget(tool);

    QAction* showAction = new QAction(tool->getTitle(), this);
    showAction->setCheckable(true);
    connect(dock, SIGNAL(visibilityChanged(bool)), showAction, SLOT(setChecked(bool)));
    connect(showAction, SIGNAL(triggered(bool)), dock, SLOT(setVisible(bool)));
    tool->setMainMenuAction(showAction);
    ui.menuTools->addAction(showAction);
    this->addDockWidget(Qt::BottomDockWidgetArea, dock);
    dock->setVisible(true);
}

void MainWindow::loadCustomWidget()
{
    QString widgetFileExtension(".qgw");
    QString fileName = QFileDialog::getOpenFileName(this, tr("Specify Widget File Name"), QDesktopServices::storageLocation(QDesktopServices::DesktopLocation), tr("QGroundControl Widget (*%1);;").arg(widgetFileExtension));
    if (fileName != "") loadCustomWidget(fileName);
}

void MainWindow::loadCustomWidget(const QString& fileName, bool singleinstance)
{
    QGCToolWidget* tool = new QGCToolWidget("", this);
    if (tool->loadSettings(fileName, true) || !singleinstance)
    {
        // Add widget to UI
        QDockWidget* dock = new QDockWidget(tool->getTitle(), this);
        connect(tool, SIGNAL(destroyed()), dock, SLOT(deleteLater()));
        dock->setWidget(tool);
        tool->setParent(dock);

        QAction* showAction = new QAction(tool->getTitle(), this);
        showAction->setCheckable(true);
        connect(dock, SIGNAL(visibilityChanged(bool)), showAction, SLOT(setChecked(bool)));
        connect(showAction, SIGNAL(triggered(bool)), dock, SLOT(setVisible(bool)));
        tool->setMainMenuAction(showAction);
        ui.menuTools->addAction(showAction);
        this->addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->setVisible(true);
    }
    else
    {
        return;
    }
}

void MainWindow::loadCustomWidgetsFromDefaults(const QString& systemType, const QString& autopilotType)
{
    QString defaultsDir = qApp->applicationDirPath() + "/files/" + autopilotType.toLower() + "/" + systemType.toLower() + "/widgets/";

    QDir widgets(defaultsDir);
    QStringList files = widgets.entryList();
    if (files.count() == 0)
    {
        qDebug() << "No default custom widgets for system " << systemType << "autopilot" << autopilotType << " found";
        qDebug() << "Tried with path: " << defaultsDir;
        showStatusMessage(tr("Did not find any custom widgets in %1").arg(defaultsDir));
    }

    // Load all custom widgets found in the AP folder
    for(int i = 0; i < files.count(); ++i)
    {
        QString file = files[i];
        if (file.endsWith(".qgw"))
        {
            // Will only be loaded if not already a custom widget with
            // the same name is present
            loadCustomWidget(defaultsDir+"/"+file, true);
            showStatusMessage(tr("Loaded custom widget %1").arg(defaultsDir+"/"+file));
        }
    }
}

void MainWindow::loadSettings()
{
    QSettings settings;
    settings.beginGroup("QGC_MAINWINDOW");
    autoReconnect = settings.value("AUTO_RECONNECT", autoReconnect).toBool();
    currentStyle = (QGC_MAINWINDOW_STYLE)settings.value("CURRENT_STYLE", currentStyle).toInt();
    lowPowerMode = settings.value("LOW_POWER_MODE", lowPowerMode).toBool();
    settings.endGroup();
}

void MainWindow::storeSettings()
{
    QSettings settings;
    settings.beginGroup("QGC_MAINWINDOW");
    settings.setValue("AUTO_RECONNECT", autoReconnect);
    settings.setValue("CURRENT_STYLE", currentStyle);
    settings.endGroup();
    if (!aboutToCloseFlag && isVisible())
    {
        settings.setValue(getWindowGeometryKey(), saveGeometry());
        // Save the last current view in any case
        settings.setValue("CURRENT_VIEW", currentView);
        // Save the current window state, but only if a system is connected (else no real number of widgets would be present)
        if (UASManager::instance()->getUASList().length() > 0) settings.setValue(getWindowStateKey(), saveState(QGC::applicationVersion()));
        // Save the current view only if a UAS is connected
        if (UASManager::instance()->getUASList().length() > 0) settings.setValue("CURRENT_VIEW_WITH_UAS_CONNECTED", currentView);
        // Save the current power mode
    }
    settings.setValue("LOW_POWER_MODE", lowPowerMode);
    settings.sync();
}

void MainWindow::configureWindowName()
{
    QList<QHostAddress> hostAddresses = QNetworkInterface::allAddresses();
    QString windowname = qApp->applicationName() + " " + qApp->applicationVersion();
    bool prevAddr = false;

    windowname.append(" (" + QHostInfo::localHostName() + ": ");

    for (int i = 0; i < hostAddresses.size(); i++)
    {
        // Exclude loopback IPv4 and all IPv6 addresses
        if (hostAddresses.at(i) != QHostAddress("127.0.0.1") && !hostAddresses.at(i).toString().contains(":"))
        {
            if(prevAddr) windowname.append("/");
            windowname.append(hostAddresses.at(i).toString());
            prevAddr = true;
        }
    }

    windowname.append(")");

    setWindowTitle(windowname);

#ifndef Q_WS_MAC
    //qApp->setWindowIcon(QIcon(":/core/images/qtcreator_logo_128.png"));
#endif
}

void MainWindow::startVideoCapture()
{
    QString format = "bmp";
    QString initialPath = QDir::currentPath() + tr("/untitled.") + format;

    QString screenFileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                          initialPath,
                                                          tr("%1 Files (*.%2);;All Files (*)")
                                                          .arg(format.toUpper())
                                                          .arg(format));
    delete videoTimer;
    videoTimer = new QTimer(this);
    //videoTimer->setInterval(40);
    //connect(videoTimer, SIGNAL(timeout()), this, SLOT(saveScreen()));
    //videoTimer->stop();
}

void MainWindow::stopVideoCapture()
{
    videoTimer->stop();

    // TODO Convert raw images to PNG
}

void MainWindow::saveScreen()
{
    QPixmap window = QPixmap::grabWindow(this->winId());
    QString format = "bmp";

    if (!screenFileName.isEmpty())
    {
        window.save(screenFileName, format.toAscii());
    }
}

void MainWindow::enableAutoReconnect(bool enabled)
{
    autoReconnect = enabled;
}

void MainWindow::loadNativeStyle()
{
    loadStyle(QGC_MAINWINDOW_STYLE_NATIVE);
}

void MainWindow::loadIndoorStyle()
{
    loadStyle(QGC_MAINWINDOW_STYLE_INDOOR);
}

void MainWindow::loadOutdoorStyle()
{
    loadStyle(QGC_MAINWINDOW_STYLE_OUTDOOR);
}

void MainWindow::loadSkyeStyle()                            //Code AL (14.03.12)
{
    loadStyle(QGC_MAINWINDOW_STYLE_SKYE);
}

void MainWindow::loadStyle(QGC_MAINWINDOW_STYLE style)
{
    switch (style) {
    case QGC_MAINWINDOW_STYLE_NATIVE: {
        // Native mode means setting no style
        // so if we were already in native mode
        // take no action
        // Only if a style was set, remove it.
        if (style != currentStyle) {
            qApp->setStyleSheet("");
            showInfoMessage(tr("Please restart QGroundControl"), tr("Please restart QGroundControl to switch to fully native look and feel. Currently you have loaded Qt's plastique style."));
        }
    }
    break;
    case QGC_MAINWINDOW_STYLE_INDOOR:
        qApp->setStyle("plastique");
        styleFileName = ":/images/style-mission.css";
        reloadStylesheet();
        break;
    case QGC_MAINWINDOW_STYLE_OUTDOOR:
        qApp->setStyle("plastique");
        styleFileName = ":/images/style-outdoor.css";
        reloadStylesheet();
        break;
    case QGC_MAINWINDOW_STYLE_SKYE:
        qApp->setStyle("plastique");
        styleFileName = ":/images/style-skye.css";
        reloadStylesheet();
    }
    currentStyle = style;
}

void MainWindow::selectStylesheet()
{
    // Let user select style sheet
    styleFileName = QFileDialog::getOpenFileName(this, tr("Specify stylesheet"), styleFileName, tr("CSS Stylesheet (*.css);;"));

    if (!styleFileName.endsWith(".css"))
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText(tr("QGroundControl did lot load a new style"));
        msgBox.setInformativeText(tr("No suitable .css file selected. Please select a valid .css file."));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
        return;
    }

    // Load style sheet
    reloadStylesheet();
}

void MainWindow::reloadStylesheet()
{
    // Load style sheet
    QFile* styleSheet = new QFile(styleFileName);
    if (!styleSheet->exists())
    {
        styleSheet = new QFile(":/images/style-mission.css");
    }
    if (styleSheet->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString style = QString(styleSheet->readAll());
        style.replace("ICONDIR", QCoreApplication::applicationDirPath()+ "/images/");
        qApp->setStyleSheet(style);
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText(tr("QGroundControl did lot load a new style"));
        msgBox.setInformativeText(tr("Stylesheet file %1 was not readable").arg(styleFileName));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
    }
    delete styleSheet;
}

/**
 * The status message will be overwritten if a new message is posted to this function
 *
 * @param status message text
 * @param timeout how long the status should be displayed
 */
void MainWindow::showStatusMessage(const QString& status, int timeout)
{
    statusBar()->showMessage(status, timeout);
}

/**
 * The status message will be overwritten if a new message is posted to this function.
 * it will be automatically hidden after 5 seconds.
 *
 * @param status message text
 */
void MainWindow::showStatusMessage(const QString& status)
{
    statusBar()->showMessage(status, 20000);
}

void MainWindow::showCriticalMessage(const QString& title, const QString& message)
{
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText(title);
    msgBox.setInformativeText(message);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
}

void MainWindow::showInfoMessage(const QString& title, const QString& message)
{
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(title);
    msgBox.setInformativeText(message);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
}

/**
* @brief Create all actions associated to the main window
*
**/
void MainWindow::connectCommonActions()
{
    // Bind together the perspective actions
    QActionGroup* perspectives = new QActionGroup(ui.menuPerspectives);
    perspectives->addAction(ui.actionEngineersView);
    perspectives->addAction(ui.actionMavlinkView);
    perspectives->addAction(ui.actionPilotsView);
    perspectives->addAction(ui.actionOperatorsView);
    perspectives->addAction(ui.actionSkyeView);                                 //Beginn und Ende Code AL (01.03.12) ------------------------------------------
    perspectives->addAction(ui.actionFirmwareUpdateView);
    perspectives->addAction(ui.actionUnconnectedView);
    perspectives->setExclusive(true);

    // Mark the right one as selected
    if (currentView == VIEW_ENGINEER) ui.actionEngineersView->setChecked(true);
    if (currentView == VIEW_MAVLINK) ui.actionMavlinkView->setChecked(true);
    if (currentView == VIEW_PILOT) ui.actionPilotsView->setChecked(true);
    if (currentView == VIEW_OPERATOR) ui.actionOperatorsView->setChecked(true);
    if (currentView == VIEW_SKYE) ui.actionSkyeView->setChecked(true);              //Beginn und Ende Code AL (01.03.12)---------------------------------
    if (currentView == VIEW_FIRMWAREUPDATE) ui.actionFirmwareUpdateView->setChecked(true);
    if (currentView == VIEW_UNCONNECTED) ui.actionUnconnectedView->setChecked(true);

    // The UAS actions are not enabled without connection to system
    ui.actionLiftoff->setEnabled(false);
    ui.actionLand->setEnabled(false);
    ui.actionEmergency_Kill->setEnabled(false);
    ui.actionEmergency_Land->setEnabled(false);
    ui.actionShutdownMAV->setEnabled(false);

    // Connect actions from ui
    connect(ui.actionAdd_Link, SIGNAL(triggered()), this, SLOT(addLink()));

    // Connect internal actions
    connect(UASManager::instance(), SIGNAL(UASCreated(UASInterface*)), this, SLOT(UASCreated(UASInterface*)));
    connect(UASManager::instance(), SIGNAL(activeUASSet(UASInterface*)), this, SLOT(setActiveUAS(UASInterface*)));

    // Unmanned System controls
    connect(ui.actionLiftoff, SIGNAL(triggered()), UASManager::instance(), SLOT(launchActiveUAS()));
    connect(ui.actionLand, SIGNAL(triggered()), UASManager::instance(), SLOT(returnActiveUAS()));
    connect(ui.actionEmergency_Land, SIGNAL(triggered()), UASManager::instance(), SLOT(stopActiveUAS()));
    connect(ui.actionEmergency_Kill, SIGNAL(triggered()), UASManager::instance(), SLOT(killActiveUAS()));
    connect(ui.actionShutdownMAV, SIGNAL(triggered()), UASManager::instance(), SLOT(shutdownActiveUAS()));
    connect(ui.actionConfiguration, SIGNAL(triggered()), UASManager::instance(), SLOT(configureActiveUAS()));

    // Views actions
    connect(ui.actionPilotsView, SIGNAL(triggered()), this, SLOT(loadPilotView()));
    connect(ui.actionEngineersView, SIGNAL(triggered()), this, SLOT(loadEngineerView()));
    connect(ui.actionOperatorsView, SIGNAL(triggered()), this, SLOT(loadOperatorView()));
    connect(ui.actionSkyeView, SIGNAL(triggered()), this, SLOT(loadSkyeView()));                        //Beginn und Ende Code AL (01.03.12)------------------------------
    connect(ui.actionUnconnectedView, SIGNAL(triggered()), this, SLOT(loadUnconnectedView()));

    connect(ui.actionFirmwareUpdateView, SIGNAL(triggered()), this, SLOT(loadFirmwareUpdateView()));
    connect(ui.actionMavlinkView, SIGNAL(triggered()), this, SLOT(loadMAVLinkView()));

    connect(ui.actionReloadStylesheet, SIGNAL(triggered()), this, SLOT(reloadStylesheet()));
    connect(ui.actionSelectStylesheet, SIGNAL(triggered()), this, SLOT(selectStylesheet()));

    // Help Actions
    connect(ui.actionOnline_Documentation, SIGNAL(triggered()), this, SLOT(showHelp()));
    connect(ui.actionDeveloper_Credits, SIGNAL(triggered()), this, SLOT(showCredits()));
    connect(ui.actionProject_Roadmap_2, SIGNAL(triggered()), this, SLOT(showRoadMap()));

    // Custom widget actions
    connect(ui.actionNewCustomWidget, SIGNAL(triggered()), this, SLOT(createCustomWidget()));
    connect(ui.actionLoadCustomWidgetFile, SIGNAL(triggered()), this, SLOT(loadCustomWidget()));

    // Audio output
    ui.actionMuteAudioOutput->setChecked(GAudioOutput::instance()->isMuted());
    connect(GAudioOutput::instance(), SIGNAL(mutedChanged(bool)), ui.actionMuteAudioOutput, SLOT(setChecked(bool)));
    connect(ui.actionMuteAudioOutput, SIGNAL(triggered(bool)), GAudioOutput::instance(), SLOT(mute(bool)));

    // User interaction
    // NOTE: Joystick thread is not started and
    // configuration widget is not instantiated
    // unless it is actually used
    // so no ressources spend on this.
    ui.actionJoystickSettings->setVisible(true);

    // Configuration
    // Joystick
    connect(ui.actionJoystickSettings, SIGNAL(triggered()), this, SLOT(configure()));
    // Application Settings
    connect(ui.actionSettings, SIGNAL(triggered()), this, SLOT(showSettings()));

    ui.actionTestphase->setVisible(true);                                           //Beginn Code AL (01.03.12)----------
    connect(ui.actionTestphase, SIGNAL(triggered()), this, SLOT(showTestphase()));  //Ende Code AL ----------------------
    ui.actionCamera_Reconfigure->setVisible(true);                                           //Beginn Code MA (20.03.12)----------
    connect(ui.actionCamera_Reconfigure, SIGNAL(triggered()), this, SLOT(showSkyeCamReconfig()));  //Ende Code MA ----------------------
}

void MainWindow::showHelp()
{
    if(!QDesktopServices::openUrl(QUrl("http://qgroundcontrol.org/users/start")))
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText("Could not open help in browser");
        msgBox.setInformativeText("To get to the online help, please open http://qgroundcontrol.org/user_guide in a browser.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
    }
}

void MainWindow::showCredits()
{
    if(!QDesktopServices::openUrl(QUrl("http://qgroundcontrol.org/credits")))
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText("Could not open credits in browser");
        msgBox.setInformativeText("To get to the online help, please open http://qgroundcontrol.org/credits in a browser.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
    }
}

void MainWindow::showRoadMap()
{
    if(!QDesktopServices::openUrl(QUrl("http://qgroundcontrol.org/dev/roadmap")))
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText("Could not open roadmap in browser");
        msgBox.setInformativeText("To get to the online help, please open http://qgroundcontrol.org/roadmap in a browser.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
    }
}

void MainWindow::configure()
{
    if (!joystickWidget)
    {
        if (!joystick->isRunning())
        {
            joystick->start();
        }
        joystickWidget = new JoystickWidget(joystick);
    }
    joystickWidget->show();
}

void MainWindow::showTestphase()                    //Beginn Code AL (03.01.12)
{
     if(!testphaseWidget)
    {
        testphaseWidget = new TestphaseWidget(this);
    }
    testphaseWidget->show();
    testphaseWidget->activateWindow();

//    else
//    {
//        testphaseWidget->raise();
//    }
}                                                   //Ende Code AL (03.01.12)

void MainWindow::showSkyeCamReconfig()                    //Beginn Code MA (20.01.12)
{
     if(!skyeCameraReconfigureWidget)
    {
        skyeCameraReconfigureWidget = new SkyeCameraReconfigure(this);
    }
    skyeCameraReconfigureWidget->show();
    skyeCameraReconfigureWidget->activateWindow();

//    else
//    {
//        testphaseWidget->raise();
//    }
}                                                   //Ende Code MA (20.01.12)

void MainWindow::showSettings()
{
    QGCSettingsWidget* settings = new QGCSettingsWidget(this);
    settings->show();
}

void MainWindow::addLink()
{
    SerialLink* link = new SerialLink();
    // TODO This should be only done in the dialog itself

    LinkManager::instance()->add(link);
    LinkManager::instance()->addProtocol(link, mavlink);

    // Go fishing for this link's configuration window
    QList<QAction*> actions = ui.menuNetwork->actions();

	const int32_t& linkIndex(LinkManager::instance()->getLinks().indexOf(link));
	const int32_t& linkID(LinkManager::instance()->getLinks()[linkIndex]->getId());

    foreach (QAction* act, actions)
    {
        if (act->data().toInt() == linkID)
        { // LinkManager::instance()->getLinks().indexOf(link)
            act->trigger();
            break;
        }
    }
}

void MainWindow::addLink(LinkInterface *link)
{
    // IMPORTANT! KEEP THESE TWO LINES
    // THEY MAKE SURE THE LINK IS PROPERLY REGISTERED
    // BEFORE LINKING THE UI AGAINST IT
    // Register (does nothing if already registered)
    LinkManager::instance()->add(link);
    LinkManager::instance()->addProtocol(link, mavlink);

    // Go fishing for this link's configuration window
    QList<QAction*> actions = ui.menuNetwork->actions();

    bool found(false);

	const int32_t& linkIndex(LinkManager::instance()->getLinks().indexOf(link));
	const int32_t& linkID(LinkManager::instance()->getLinks()[linkIndex]->getId());

    foreach (QAction* act, actions)
    {
        if (act->data().toInt() == linkID)
        { // LinkManager::instance()->getLinks().indexOf(link)
            found = true;
        }
    }

    //UDPLink* udp = dynamic_cast<UDPLink*>(link);

    if (!found)
    {  //  || udp
        CommConfigurationWindow* commWidget = new CommConfigurationWindow(link, mavlink, this);
        QAction* action = commWidget->getAction();
        ui.menuNetwork->addAction(action);

        // Error handling
        connect(link, SIGNAL(communicationError(QString,QString)), this, SLOT(showCriticalMessage(QString,QString)), Qt::QueuedConnection);
        // Special case for simulationlink
        MAVLinkSimulationLink* sim = dynamic_cast<MAVLinkSimulationLink*>(link);
        if (sim)
        {
            connect(ui.actionSimulate, SIGNAL(triggered(bool)), sim, SLOT(connectLink(bool)));
        }
    }
}

void MainWindow::setActiveUAS(UASInterface* uas)
{
    // Enable and rename menu
    ui.menuUnmanned_System->setTitle(uas->getUASName());
    if (!ui.menuUnmanned_System->isEnabled()) ui.menuUnmanned_System->setEnabled(true);

#ifdef MAVLINK_ENABLED_SKYE         // Begin Code MA (06.03.2012) [similar JoystickInput] -------------
    // Connect 3d-Mouse Input to (skye) Mavlink messages
    SkyeMAV* tmp = 0;
    // TODO: Disconnect old uas

    tmp = dynamic_cast<SkyeMAV*>(uas);
    if(tmp) {
        connect(this, SIGNAL(valueMouseChanged(double,double,double,double,double,double)), tmp, SLOT(setManualControlCommands6DoF(double,double,double,double,double,double)));
        connect(this, SIGNAL(valueKeyboardChanged(double,double,double,double,double,double)), tmp, SLOT(setManualControlCommands6DoF(double,double,double,double,double,double)));
    }
#endif // MAVLINK_ENABLED_SKYE      // Ende Code MA (27.02.2012) ---------------------------
}

void MainWindow::UASSpecsChanged(int uas)
{
    UASInterface* activeUAS = UASManager::instance()->getActiveUAS();
    if (activeUAS)
    {
        if (activeUAS->getUASID() == uas)
        {
            ui.menuUnmanned_System->setTitle(activeUAS->getUASName());
        }
    }
}

void MainWindow::UASCreated(UASInterface* uas)
{

    // Connect the UAS to the full user interface

    //if (uas != NULL)
    //{
        // The pilot, operator and engineer views were not available on startup, enable them now
        ui.actionPilotsView->setEnabled(true);
        ui.actionOperatorsView->setEnabled(true);
        ui.actionSkyeView->setEnabled(true);                                    //Beginn und Ende Code AL (01.03.12)-------------------------------------------------
        ui.actionEngineersView->setEnabled(true);
        // The UAS actions are not enabled without connection to system
        ui.actionLiftoff->setEnabled(true);
        ui.actionLand->setEnabled(true);
        ui.actionEmergency_Kill->setEnabled(true);
        ui.actionEmergency_Land->setEnabled(true);
        ui.actionShutdownMAV->setEnabled(true);

        QIcon icon;
        // Set matching icon
        switch (uas->getSystemType())
		{
        case MAV_TYPE_GENERIC:
            icon = QIcon(":/images/mavs/generic.svg");
            break;
        case MAV_TYPE_FIXED_WING:
            icon = QIcon(":/images/mavs/fixed-wing.svg");
            break;
        case MAV_TYPE_QUADROTOR:
            icon = QIcon(":/images/mavs/quadrotor.svg");
            break;
        case MAV_TYPE_COAXIAL:
            icon = QIcon(":/images/mavs/coaxial.svg");
            break;
        case MAV_TYPE_HELICOPTER:
            icon = QIcon(":/images/mavs/helicopter.svg");
            break;
        case MAV_TYPE_GCS:
            icon = QIcon(":/images/mavs/groundstation.svg");
            break;
        case MAV_TYPE_AIRSHIP:
            icon = QIcon(":/images/skye_images/LOGO_DEF.png");
            break;
        default:
            icon = QIcon(":/images/mavs/unknown.svg");
            break;
        }

        QAction* uasAction = new QAction(icon, tr("Select %1 for control").arg(uas->getUASName()), ui.menuConnected_Systems);
        connect(uas, SIGNAL(systemRemoved()), uasAction, SLOT(deleteLater()));
        connect(uasAction, SIGNAL(triggered()), uas, SLOT(setSelected()));
        connect(uas, SIGNAL(systemSpecsChanged(int)), this, SLOT(UASSpecsChanged(int)));

        ui.menuConnected_Systems->addAction(uasAction);

        // FIXME Should be not inside the mainwindow
        if (debugConsoleDockWidget)
        {
            DebugConsole *debugConsole = dynamic_cast<DebugConsole*>(debugConsoleDockWidget->widget());
            if (debugConsole)
            {
                connect(uas, SIGNAL(textMessageReceived(int,int,int,QString)),
                        debugConsole, SLOT(receiveTextMessage(int,int,int,QString)));
            }
        }

        // Health / System status indicator
        if (infoDockWidget)
        {
            UASInfoWidget *infoWidget = dynamic_cast<UASInfoWidget*>(infoDockWidget->widget());
            if (infoWidget)
            {
                infoWidget->addUAS(uas);
            }
        }

        // UAS List
        if (listDockWidget)
        {
            UASListWidget *listWidget = dynamic_cast<UASListWidget*>(listDockWidget->widget());
            if (listWidget)
            {
                listWidget->addUAS(uas);
            }
        }

        // Line chart
        if (!linechartWidget)
        {
            // Center widgets
            linechartWidget = new Linecharts(this);
            linechartWidget->addSource(mavlinkDecoder);
            addCentralWidget(linechartWidget, tr("Realtime Plot"));
        }

        // Load default custom widgets for this autopilot type
        loadCustomWidgetsFromDefaults(uas->getSystemTypeName(), uas->getAutopilotTypeName());


        if (uas->getAutopilotType() == MAV_AUTOPILOT_PIXHAWK)
        {
            // Dock widgets
            if (!detectionDockWidget)
            {
                detectionDockWidget = new QDockWidget(tr("Object Recognition"), this);
                detectionDockWidget->setWidget( new ObjectDetectionView("images/patterns", this) );
                detectionDockWidget->setObjectName("OBJECT_DETECTION_DOCK_WIDGET");
                addTool(detectionDockWidget, tr("Object Recognition"), Qt::RightDockWidgetArea);
            }

            if (!watchdogControlDockWidget)
            {
                watchdogControlDockWidget = new QDockWidget(tr("Process Control"), this);
                watchdogControlDockWidget->setWidget( new WatchdogControl(this) );
                watchdogControlDockWidget->setObjectName("WATCHDOG_CONTROL_DOCKWIDGET");
                addTool(watchdogControlDockWidget, tr("Process Control"), Qt::BottomDockWidgetArea);
            }
        }

        if (uas->getAutopilotType() == MAV_AUTOPILOT_SKYE)
        {
            // Dock widget
            if (!skyeBatteryInfoDockWidget)
            {
                skyeBatteryInfoDockWidget = new QDockWidget("Detailed Battery Info", this);
                skyeBatteryInfoDockWidget->setWidget( new UASSkyeBatteryInfoWidget);
                skyeBatteryInfoDockWidget->setObjectName("SKYE_BATTERY_INFO_DOCKWIDGET");
                addTool(skyeBatteryInfoDockWidget, tr("Battery Info"), Qt::RightDockWidgetArea);
            }
        }

        // Change the view only if this is the first UAS

        // If this is the first connected UAS, it is both created as well as
        // the currently active UAS
        if (UASManager::instance()->getUASList().size() == 1)
        {
            // Load last view if setting is present
            if (settings.contains("CURRENT_VIEW_WITH_UAS_CONNECTED"))
            {
                int view = settings.value("CURRENT_VIEW_WITH_UAS_CONNECTED").toInt();
                switch (view)
                {
                case VIEW_ENGINEER:
                    loadEngineerView();
                    break;
                case VIEW_MAVLINK:
                    loadMAVLinkView();
                    break;
                case VIEW_FIRMWAREUPDATE:
                    loadFirmwareUpdateView();
                    break;
                case VIEW_PILOT:
                    loadPilotView();
                    break;
                case VIEW_SKYE:         //Beginn Code AL (01.03.12)---------------------------------------
                    loadSkyeView();     //Ende Code AL ---------------------------------------------------
                case VIEW_UNCONNECTED:
                    loadUnconnectedView();
                    break;
                case VIEW_OPERATOR:
                default:
                    loadOperatorView();
                    break;
                }
            }
            else
            {
                if (uas->getAutopilotType() == MAV_AUTOPILOT_SKYE)      // Beginn Code MA (15.03.2012)
                {
                    loadSkyeView();
                }
                else
                {                               // Ende Code MA (15.03.2012)
                    loadOperatorView();
                }
            }
        }

    //}

    if (!ui.menuConnected_Systems->isEnabled()) ui.menuConnected_Systems->setEnabled(true);

    // Reload view state in case new widgets were added
    loadViewState();
}

/**
 * Stores the current view state
 */
void MainWindow::storeViewState()
{
    if (!aboutToCloseFlag)
    {
        // Save current state
        settings.setValue(getWindowStateKey(), saveState(QGC::applicationVersion()));
        settings.setValue(getWindowStateKey()+"CENTER_WIDGET", centerStack->currentIndex());
        // Although we want save the state of the window, we do not want to change the top-leve state (minimized, maximized, etc)
        // therefore this state is stored here and restored after applying the rest of the settings in the new
        // perspective.
        windowStateVal = this->windowState();
        settings.setValue(getWindowGeometryKey(), saveGeometry());
    }
}

void MainWindow::loadViewState()
{
    // Restore center stack state
    int index = settings.value(getWindowStateKey()+"CENTER_WIDGET", -1).toInt();
    // The offline plot view is usually the consequence of a logging run, always show the realtime view first
    if (centerStack->indexOf(dataplotWidget) == index)
    {
        // Rewrite to realtime plot
        index = centerStack->indexOf(linechartWidget);
    }

    if (index != -1)
    {
        centerStack->setCurrentIndex(index);
    }
    else
    {
        // Hide custom widgets
        if (detectionDockWidget) detectionDockWidget->hide();
        if (watchdogControlDockWidget) watchdogControlDockWidget->hide();

        // Load defaults
        switch (currentView)
        {
        case VIEW_ENGINEER:
            centerStack->setCurrentWidget(linechartWidget);
            controlDockWidget->hide();
            listDockWidget->hide();
            waypointsDockWidget->hide();
            infoDockWidget->hide();
            debugConsoleDockWidget->show();
            logPlayerDockWidget->show();
            mavlinkInspectorWidget->show();
            mavlinkSenderWidget->show();
            parametersDockWidget->show();
            hsiDockWidget->hide();
            headDown1DockWidget->hide();
            headDown2DockWidget->hide();
            rcViewDockWidget->hide();
            headUpDockWidget->hide();
            video1DockWidget->hide();
            video2DockWidget->hide();
            break;
        case VIEW_PILOT:
            centerStack->setCurrentWidget(hudWidget);
            controlDockWidget->hide();
            listDockWidget->hide();
            waypointsDockWidget->hide();
            infoDockWidget->hide();
            debugConsoleDockWidget->hide();
            logPlayerDockWidget->hide();
            mavlinkInspectorWidget->hide();
            parametersDockWidget->hide();
            hsiDockWidget->show();
            headDown1DockWidget->show();
            headDown2DockWidget->show();
            rcViewDockWidget->hide();
            headUpDockWidget->hide();
            video1DockWidget->hide();
            video2DockWidget->hide();
            break;
        case VIEW_MAVLINK:
            centerStack->setCurrentWidget(protocolWidget);
            controlDockWidget->hide();
            listDockWidget->hide();
            waypointsDockWidget->hide();
            infoDockWidget->hide();
            debugConsoleDockWidget->hide();
            logPlayerDockWidget->hide();
            mavlinkInspectorWidget->show();
            mavlinkSenderWidget->show();
            parametersDockWidget->hide();
            hsiDockWidget->hide();
            headDown1DockWidget->hide();
            headDown2DockWidget->hide();
            rcViewDockWidget->hide();
            headUpDockWidget->hide();
            video1DockWidget->hide();
            video2DockWidget->hide();
            break;
        case VIEW_FIRMWAREUPDATE:
            centerStack->setCurrentWidget(firmwareUpdateWidget);
            controlDockWidget->hide();
            listDockWidget->hide();
            waypointsDockWidget->hide();
            infoDockWidget->hide();
            debugConsoleDockWidget->hide();
            logPlayerDockWidget->hide();
            mavlinkInspectorWidget->hide();
            mavlinkSenderWidget->hide();
            parametersDockWidget->hide();
            hsiDockWidget->hide();
            headDown1DockWidget->hide();
            headDown2DockWidget->hide();
            rcViewDockWidget->hide();
            headUpDockWidget->hide();
            video1DockWidget->hide();
            video2DockWidget->hide();
            break;
        case VIEW_OPERATOR:
            centerStack->setCurrentWidget(mapWidget);
            controlDockWidget->hide();
            listDockWidget->show();
            waypointsDockWidget->show();
            infoDockWidget->hide();
            debugConsoleDockWidget->show();
            logPlayerDockWidget->show();
            parametersDockWidget->hide();
            hsiDockWidget->show();
            headDown1DockWidget->hide();
            headDown2DockWidget->hide();
            rcViewDockWidget->hide();
            headUpDockWidget->show();
            video1DockWidget->hide();
            video2DockWidget->hide();
            mavlinkInspectorWidget->hide();
            break;
        case VIEW_SKYE:                                 //Beginn Code AL (01.03.12) Hier werden spaeter unsere eigenen Widgets geladen----------
            centerStack->setCurrentWidget(mapWidget);
            controlDockWidget->hide();
            listDockWidget->show();
            waypointsDockWidget->show();
            infoDockWidget->hide();
            debugConsoleDockWidget->show();
            logPlayerDockWidget->show();
            parametersDockWidget->hide();
            hsiDockWidget->show();
            headDown1DockWidget->hide();
            headDown2DockWidget->hide();
            rcViewDockWidget->hide();
            headUpDockWidget->show();
            video1DockWidget->hide();
            video2DockWidget->hide();
            mavlinkInspectorWidget->show();
            break;                                      //Ende Code AL
        case VIEW_UNCONNECTED:
        case VIEW_FULL:
        default:
            centerStack->setCurrentWidget(mapWidget);
            controlDockWidget->hide();
            listDockWidget->show();
            waypointsDockWidget->hide();
            infoDockWidget->hide();
            debugConsoleDockWidget->show();
            logPlayerDockWidget->show();
            parametersDockWidget->hide();
            hsiDockWidget->hide();
            headDown1DockWidget->hide();
            headDown2DockWidget->hide();
            rcViewDockWidget->hide();
            headUpDockWidget->show();
            video1DockWidget->hide();
            video2DockWidget->hide();
            mavlinkInspectorWidget->show();
            break;
        }
    }

    // Restore the widget positions and size
    if (settings.contains(getWindowStateKey()))
    {
        restoreState(settings.value(getWindowStateKey()).toByteArray(), QGC::applicationVersion());
    }
}

void MainWindow::loadEngineerView()
{
    if (currentView != VIEW_ENGINEER)
    {
        storeViewState();
        currentView = VIEW_ENGINEER;
        ui.actionEngineersView->setChecked(true);
        loadViewState();
    }
}

void MainWindow::loadOperatorView()
{
    if (currentView != VIEW_OPERATOR)
    {
        storeViewState();
        currentView = VIEW_OPERATOR;
        ui.actionOperatorsView->setChecked(true);
        loadViewState();
    }
}

void MainWindow::loadSkyeView()                         //Beginn Code AL (01.03.12)--------------------------------
{
    if (currentView != VIEW_SKYE)
    {
        storeViewState();
        currentView = VIEW_SKYE;
        ui.actionSkyeView->setChecked(true);
        loadViewState();
    }
}                                                       //Ende Code AL---------------------------------------------

void MainWindow::loadUnconnectedView()
{
    if (currentView != VIEW_UNCONNECTED)
    {
        storeViewState();
        currentView = VIEW_UNCONNECTED;
        ui.actionUnconnectedView->setChecked(true);
        loadViewState();
    }
}

void MainWindow::loadPilotView()
{
    if (currentView != VIEW_PILOT)
    {
        storeViewState();
        currentView = VIEW_PILOT;
        ui.actionPilotsView->setChecked(true);
        loadViewState();
    }
}

void MainWindow::loadMAVLinkView()
{
    if (currentView != VIEW_MAVLINK)
    {
        storeViewState();
        currentView = VIEW_MAVLINK;
        ui.actionMavlinkView->setChecked(true);
        loadViewState();
    }
}

void MainWindow::loadFirmwareUpdateView()
{
    if (currentView != VIEW_FIRMWAREUPDATE)
    {
        storeViewState();
        currentView = VIEW_FIRMWAREUPDATE;
        ui.actionFirmwareUpdateView->setChecked(true);
        loadViewState();
    }
}

void MainWindow::loadDataView(QString fileName)
{
    // Plot is now selected, now load data from file
    if (dataplotWidget)
    {
        dataplotWidget->loadFile(fileName);
    }
    QStackedWidget *centerStack = dynamic_cast<QStackedWidget*>(centralWidget());
    if (centerStack)
    {
        centerStack->setCurrentWidget(dataplotWidget);
        dataplotWidget->loadFile(fileName);
    }
}


QList<QAction*> MainWindow::listLinkMenuActions(void)
{
    return ui.menuNetwork->actions();
}


void MainWindow::setInputMode(int inputMode)
{
#ifdef MAVLINK_ENABLED_SKYE
    switch (inputMode)
    {
    case 1:
            this->inputMode = UASSkyeControlWidget::QGC_INPUT_MODE_MOUSE;
            #ifdef MOUSE_ENABLED
            if ( !mouseInitialized )
            {
                start3dMouse();
            }
            #endif
            break;
    case 2:
            this->inputMode = UASSkyeControlWidget::QGC_INPUT_MODE_TOUCH;
            break;
    case 3:
            this->inputMode = UASSkyeControlWidget::QGC_INPUT_MODE_KEYBOARD;
            break;
    default:
            this->inputMode = UASSkyeControlWidget::QGC_INPUT_MODE_NONE;
            qDebug() << "No input device set!";
            break;
    }
    statusBar()->showMessage("Set new Input mode", 20000);
//    qDebug() << "New Input: " << inputMode;
#else
    qDebug() << "Changing input mode only available for SKYE";
#endif //MAVLINK_ENABLED_SKYE
}

#ifdef MOUSE_ENABLED            // Beginn Code MA (21.03.2012)
void MainWindow::start3dMouse()
{
    // man visudo --> then you can omit giving password (did not work..)
    qDebug() << "Starting 3DxWare Daemon for 3dConnexion 3dMouse";
    QString processProgramm = "gksudo";
    QStringList processArguments;
    processArguments << "/etc/3DxWare/daemon/3dxsrv -d usb";
   // QProcess process3dxDaemon;
    process3dxDaemon = new QProcess();
    process3dxDaemon->start(processProgramm, processArguments);
//    process3dxDaemon->waitForFinished();
//    {
//        qDebug() << "... continuing without 3DxWare. May not be initialized properly!";
//        qDebug() << "Try in terminal as user root:" << processArguments.last();
//    }

    Display *display = QX11Info::display();
    if(!display)
    {
        qDebug() << "Cannot open display!" << endl;
    }
    if ( !MagellanInit( display, winId() ) )
      {
           qDebug() << "No 3dXWare driver is running!";
           return;
      }
    else
    {
        qDebug() << "Initialized 3dMouse";
        mouseInitialized = true;
    }
}                               // Ende Code MA (21.03.2012)

                                // Beginn Code MA (06.03.2012) -----------
bool MainWindow::x11Event(XEvent *event)
{
//    qDebug("XEvent occured...");
    if (!mouseInitialized)
    {
        qDebug() << "Cancelled handling X11event for 3dMouse";
        return false;
    }
    qDebug() << "Following X11event";
#ifdef MAVLINK_ENABLED_SKYE
    if (inputMode == UASSkyeControlWidget::QGC_INPUT_MODE_MOUSE)
    {
#endif // MAVLINK_ENABLED_SKYE
    MagellanFloatEvent MagellanEvent;
    double maxMagellanValue = 350;              // Valid for Space Navigator for Notebooks

    Display *display = QX11Info::display();
    if(!display)
    {
        qDebug() << "Cannot open display!" << endl;
    }

   switch (event->type)
   {
    case ClientMessage:
          switch( MagellanTranslateEvent( display, event, &MagellanEvent, 1.0, 1.0 ) )
           {
            case MagellanInputMotionEvent :
                 MagellanRemoveMotionEvents( display );
                 qDebug("3D Mouse Motion Detected!");
                 for (int i = 0; i < 6; i++) {  // Saturation
//                     if (MagellanEvent.MagellanData[i] < (0-maxMagellanValue))
//                     {
//                         MagellanEvent.MagellanData[i] = - maxMagellanValue;
//                     }else if (MagellanEvent.MagellanData[i] > maxMagellanValue)
//                     {
//                         MagellanEvent.MagellanData[i] = maxMagellanValue;
//                     }
                     // Cancel value if motion is disabled
                     qDebug() << MagellanEvent.MagellanData[1];
                     if ((i<3 && !mouseTranslationEnable) || (i>=3 && !mouseRotationEnable))
                     {
                         MagellanEvent.MagellanData[i] = 0;
                     }
                     qDebug() << MagellanEvent.MagellanData[1];
                     MagellanEvent.MagellanData[i] = (abs(MagellanEvent.MagellanData[i]) < maxMagellanValue) ? MagellanEvent.MagellanData[i] : (maxMagellanValue*MagellanEvent.MagellanData[i]/abs(MagellanEvent.MagellanData[i]));
                 }
                 emit valueMouseChanged(MagellanEvent.MagellanData[ MagellanZ ] / maxMagellanValue,
                                        MagellanEvent.MagellanData[ MagellanX ] / maxMagellanValue,
                                        - MagellanEvent.MagellanData[ MagellanY ] / maxMagellanValue,
                                        MagellanEvent.MagellanData[ MagellanC ] / maxMagellanValue,
                                        MagellanEvent.MagellanData[ MagellanA ] / maxMagellanValue,
                                        - MagellanEvent.MagellanData[ MagellanB ] / maxMagellanValue);
            return false;
            break;

            case MagellanInputButtonPressEvent :
                qDebug() << "MagellanInputButtonPressEvent called with button " << MagellanEvent.MagellanButton;
                switch (MagellanEvent.MagellanButton)
                {
                case 1:
                {
                    mouseRotationEnable = !mouseRotationEnable;
                    emit mouseRotationEnabledChanged(mouseRotationEnable);
                    qDebug() << "Emitted Rotation " << (bool)mouseRotationEnable;
                    break;
                }
                case 2:
                {
                    mouseTranslationEnable = !mouseTranslationEnable;
                    emit mouseTranslationEnabledChanged(mouseTranslationEnable);
                    qDebug() << "Emitted translation " << (bool)mouseTranslationEnable;
                    break;
                }
                default:
                    break;
            }
    default:
    return false;
    break;
    }
   }
    #ifdef MAVLINK_ENABLED_SKYE
    }else
    {
        qDebug() << "Skipped 3dMouse input.. Input mode is " << inputMode;
    }

    #endif // MAVLINK_ENABLED_SKYE
    return false;   // Event will not be destroyed

}
#endif // MOUSE_ENABLED                                     // Ende Code MA (06.03.2012) ------

void MainWindow::keyPressEvent(QKeyEvent *event)            // Beginn Code MA (07.03.2012) ---------
{
    if (inputMode == UASSkyeControlWidget::QGC_INPUT_MODE_KEYBOARD)
    {
        qDebug() << "Key pressed and accepted!";
        handleKeyEvents(event, true);
    }
    else
    {
        // Let base class implementation handle the event
        event->ignore();
    }
}                                                           // Ende Code MA (07.03.2012) ------------

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (inputMode == UASSkyeControlWidget::QGC_INPUT_MODE_KEYBOARD)
    {
        qDebug() << "Key released and accepted!";
        handleKeyEvents(event, false);
    }
    else
    {
        // Let base class implementation handle the event
        event->ignore();
    }
}

void MainWindow::handleKeyEvents(QKeyEvent *event, bool keyPressed)
{
    //int keyDirection = keyPressed ? 1 : -1;

    switch (event->key())
    {
    // Translational motion
    case Qt::Key_S:
        // minus x
        if (keyPressed)
            keyXValue = (keyXValue > -0.9) ? (keyXValue - 0.25) : (-1);
        else
            keyXValue = 0;
        break;
    case Qt::Key_W:
        // plus x
        if (keyPressed)
            keyXValue = (keyXValue < 0.9) ? (keyXValue + 0.25) : 1;
        else
            keyXValue = 0;
        break;
    case Qt::Key_A:
        // minus y
        if (keyPressed)
            keyYValue = (keyYValue > -0.9) ? (keyYValue - 0.25) : (-1);
        else
            keyYValue = 0;
        break;
    case Qt::Key_D:
        // plus y
        if (keyPressed)
            keyYValue = (keyYValue < 0.9) ? (keyYValue + 0.25) : 1;
        else
            keyYValue = 0;
        break;
    case Qt::Key_R:
        // minus z
        if (keyPressed)
            keyZValue = (keyZValue > -0.9) ? (keyZValue - 0.25) : (-1);
        else
            keyZValue = 0;
        break;
    case Qt::Key_F:
        // plus z
        if (keyPressed)
            keyZValue = (keyZValue < 0.9) ? (keyZValue + 0.25) : 1;
        else
            keyZValue = 0;
        break;

    // Rotational motion
    case Qt::Key_Down:
        // minus pitch
        if (keyPressed)
            keyPitchValue = (keyPitchValue > -0.9) ? (keyPitchValue - 0.25) : (-1);
        else
            keyPitchValue = 0;
        break;
    case Qt::Key_Up:
        // plus pitch
        if (keyPressed)
        keyPitchValue = (keyPitchValue < 0.9) ? (keyPitchValue + 0.25) : 1;
    else
        keyPitchValue = 0;
        break;
    case Qt::Key_Left:
        // minus yaw
        if (keyPressed)
            keyYawValue = (keyYawValue > -0.9) ? (keyYawValue - 0.25) : (-1);
        else
            keyYawValue = 0;
        break;
    case Qt::Key_Right:
        // plus yaw
        if (keyPressed)
        keyYawValue = (keyYawValue < 0.9) ? (keyYawValue + 0.25) : 1;
    else
        keyYawValue = 0;
        break;

        // Special keys
    case Qt::Key_Space:
        // reset roll?
        break;
    case Qt::Key_Enter:
        // reset roll and pitch
        break;
    default:
        event->ignore();
        break;


    }

    emit valueKeyboardChanged(keyXValue, keyYValue, keyZValue, keyRollValue, keyPitchValue, keyYawValue);
}
