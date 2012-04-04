/*=====================================================================

PIXHAWK Micro Air Vehicle Flying Robotics Toolkit

(c) 2009, 2010 PIXHAWK PROJECT  <http://pixhawk.ethz.ch>

This file is part of the PIXHAWK project

    PIXHAWK is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PIXHAWK is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PIXHAWK. If not, see <http://www.gnu.org/licenses/>.

======================================================================*/

/**
 * @file
 *   @brief Definition of widget controlling one (skye) MAV. Newimplemetation of UASControlWidget.
 *
 *   @author Matthias Krebs <makrebs@student.ethz.ch>
 *
 */

#include <QString>
#include <QTimer>
#include <QLabel>
#include <QFileDialog>
#include <QDebug>
#include <QPalette>

#include "UASSkyeControlWidget.h"
#include "UASManager.h"
#include "SkyeMAV.h"
#include "QGC.h"

UASSkyeControlWidget::UASSkyeControlWidget(QWidget *parent) : QWidget(parent),
    uasId(0),
    uasMode(0),
    engineOn(false),
    inputMode(QGC_INPUT_MODE_NONE),
    mouseTranslationEnabled(true),
    mouseRotationEnabled(true)
{
#ifdef MAVLINK_ENABLED_SKYE
    ui.setupUi(this);

    // Uncheck and group buttons to enable exclusiv checkable
    ui.directControlButton->setChecked(uasMode & MAV_MODE_DIRECT_CONTROL_DISARMED);
    ui.assistedControlButton->setChecked(uasMode & MAV_MODE_ASSISTED_CONTROL_DISARMED);
    ui.halfAutomaticControlButton->setChecked(uasMode & MAV_MODE_HALF_AUTOMATIC_DISARMED);
    ui.fullAutomaticControlButton->setChecked(uasMode & MAV_MODE_FULL_AUTOMATIC_DISARMED);
    modeButtonGroup = new QButtonGroup;
    modeButtonGroup->addButton(ui.directControlButton);
    modeButtonGroup->addButton(ui.assistedControlButton);
    modeButtonGroup->addButton((ui.halfAutomaticControlButton));
    modeButtonGroup->addButton((ui.fullAutomaticControlButton));

    ui.mouseButton->setChecked(inputMode == QGC_INPUT_MODE_MOUSE);
    ui.touchButton->setChecked(inputMode == QGC_INPUT_MODE_TOUCH);
    ui.keyboardButton->setChecked(inputMode == QGC_INPUT_MODE_KEYBOARD);
    inputButtonGroup = new QButtonGroup;
    inputButtonGroup->addButton(ui.mouseButton);
    inputButtonGroup->addButton((ui.touchButton));
    inputButtonGroup->addButton((ui.keyboardButton));

    connect(UASManager::instance(), SIGNAL(activeUASSet(UASInterface*)), this, SLOT(setUAS(UASInterface*)));

    connect(ui.directControlButton, SIGNAL(toggled(bool)), this, SLOT(setDirectControlMode(bool)));
    connect(ui.assistedControlButton, SIGNAL(toggled(bool)), this, SLOT(setAssistedControlMode(bool)));
    connect(ui.halfAutomaticControlButton, SIGNAL(toggled(bool)), this, SLOT(setHalfAutomaticControlMode(bool)));
    connect(ui.fullAutomaticControlButton, SIGNAL(toggled(bool)), this, SLOT(setFullAutomaticControlMode(bool)));

    connect(ui.mouseButton, SIGNAL(toggled(bool)), this, SLOT(setInputMouse(bool)));
    connect(ui.touchButton, SIGNAL(toggled(bool)), this, SLOT(setInputTouch(bool)));
    connect(ui.keyboardButton, SIGNAL(toggled(bool)), this, SLOT(setInputKeyboard(bool)));

    //ui.gridLayout->setAlignment(Qt::AlignTop);

    updateStyleSheet();

#endif //MAVLINK_ENABLED_SKYE
}

void UASSkyeControlWidget::setUAS(UASInterface* uas)
{
#ifdef MAVLINK_ENABLED_SKYE
    if (this->uasId!= 0)
    {
        UASInterface* oldUAS = UASManager::instance()->getUASForId(this->uasId);
        ui.lastActionLabel->setText("Disconnected from " + oldUAS->getUASName());
        this->uasId = 0;
        SkyeMAV* mav = dynamic_cast<SkyeMAV*>(oldUAS);
        if (mav)
        {
            disconnect(this, SIGNAL(changedMode(int)), mav, SLOT(setMode(int)));
            disconnect(mav, SIGNAL(modeChanged(int,int)), this, SLOT(updateMode(int,int)));
            disconnect(mav, SIGNAL(statusChanged(int)), this, SLOT(updateState(int)));

            disconnect(ui.bluefoxLeftButton, SIGNAL(clicked()), this, SLOT(triggerLeftBluefoxImageShot()));
            disconnect(ui.bluefoxRightButton, SIGNAL(clicked()), this, SLOT(triggerRightBluefoxImageShot()));
            disconnect(ui.prosilicaButton, SIGNAL(clicked()), this, SLOT(triggerProsilicaImageShot()));
        }
    }


    SkyeMAV* mav = dynamic_cast<SkyeMAV*>(uas);
    if (mav)
    {
        ui.controlStatusLabel->setText(tr("Connected to ") + mav->getUASName());
        this->uasId = mav->getUASID();

        updateMode(mav->getUASID(), mav->getUASMode());
        updateState(mav->getUASState());

        // Connect user interface controls
        connect(ui.controlButton, SIGNAL(clicked()), this, SLOT(cycleContextButton()));
        connect(mav, SIGNAL(modeChanged(int,int)), this, SLOT(updateMode(int,int)));
        connect(mav, SIGNAL(statusChanged(int)), this, SLOT(updateState(int)));

        connect(ui.bluefoxLeftButton, SIGNAL(clicked()), this, SLOT(triggerLeftBluefoxImageShot()));
        connect(ui.bluefoxRightButton, SIGNAL(clicked()), this, SLOT(triggerRightBluefoxImageShot()));
        connect(ui.prosilicaButton, SIGNAL(clicked()), this, SLOT(triggerProsilicaImageShot()));
    }


#endif // MAVLINK_ENABLED_SKYE
}

UASSkyeControlWidget::~UASSkyeControlWidget()
{
#ifdef MAVLINK_ENABLED_SKYE
#endif // MAVLINK_ENABLED_SKYE
}

void UASSkyeControlWidget::updateStatemachine()
{
#ifdef MAVLINK_ENABLED_SKYE

    if (engineOn)
    {
        ui.controlButton->setText(tr("DISARM SYSTEM"));
    }
    else
    {
        ui.controlButton->setText(tr("ARM SYSTEM"));
    }
#endif // MAVLINK_ENABLED_SKYE
}

///**
// * Set the background color based on the MAV color. If the MAV is selected as the
// * currently actively controlled system, the frame color is highlighted
// */
//void UASSkyeControlWidget::setBackgroundColor(QColor color)
//{
//#ifdef MAVLINK_ENABLED_SKYE
//    // UAS color
//    QColor uasColor = color;
//    QString colorstyle;
//    QString borderColor = "#4A4A4F";
//    borderColor = "#FA4A4F";
//    uasColor = uasColor.darker(900);
//    colorstyle = colorstyle.sprintf("QLabel { border-radius: 3px; padding: 0px; margin: 0px; background-color: #%02X%02X%02X; border: 0px solid %s; }",
//                                    uasColor.red(), uasColor.green(), uasColor.blue(), borderColor.toStdString().c_str());
//    setStyleSheet(colorstyle);
//    QPalette palette = this->palette();
//    palette.setBrush(QPalette::Window, QBrush(uasColor));
//    setPalette(palette);
//    setAutoFillBackground(true);
//#endif // MAVLINK_ENABLED_SKYE
//}

void UASSkyeControlWidget::updateMode(int uas,int baseMode)
{
#ifdef MAVLINK_ENABLED_SKYE
    if ((uasId == uas) && ((int)uasMode != baseMode))
    {
        uasMode = (unsigned int)baseMode;
        switch (uasMode)
        {
        case MAV_MODE_PREFLIGHT:
        {
            uncheckAllModeButtons();
        }break;
        case MAV_MODE_DIRECT_CONTROL_DISARMED:
        case MAV_MODE_DIRECT_CONTROL_ARMED:
        {
            ui.directControlButton->setChecked(true);
        }break;
        case MAV_MODE_ASSISTED_CONTROL_DISARMED:
        case MAV_MODE_ASSISTED_CONTROL_ARMED:
        {
            ui.assistedControlButton->setChecked(true);
        }break;
        case MAV_MODE_HALF_AUTOMATIC_DISARMED:
        case MAV_MODE_HALF_AUTOMATIC_ARMED:
        {
            ui.halfAutomaticControlButton->setChecked(true);
        }break;
        case MAV_MODE_FULL_AUTOMATIC_DISARMED:
        case MAV_MODE_FULL_AUTOMATIC_ARMED:
        {
            ui.fullAutomaticControlButton->setChecked(true);
        }break;
        default:
        {
            uncheckAllModeButtons();
        }break;
        }


    }
#endif // MAVLINK_ENABLED_SKYE
}

void UASSkyeControlWidget::updateState(int state)
{
#ifdef MAVLINK_ENABLED_SKYE
    switch (state)
    {
    case (int)MAV_STATE_ACTIVE:
        engineOn = true;
        ui.controlButton->setText(tr("DISARM SYSTEM"));
        ui.controlButton->setStyleSheet("* {  background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #DD0044, stop: 1 #AA0022); border-color: yellow; color: yellow }");
        ui.directControlButton->setDisabled(true);
        ui.assistedControlButton->setDisabled(true);
        ui.halfAutomaticControlButton->setDisabled(true);
        ui.fullAutomaticControlButton->setDisabled(true);
        break;
    case (int)MAV_STATE_STANDBY:
        engineOn = false;
        ui.controlButton->setText(tr("ARM SYSTEM"));
        ui.controlButton->setStyleSheet("* { background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #00DD44, stop: 1 #11AA22); }");
        ui.directControlButton->setEnabled(true);
        ui.assistedControlButton->setEnabled(true);
        ui.halfAutomaticControlButton->setEnabled(true);
        ui.fullAutomaticControlButton->setEnabled(true);
        break;
    }
#endif // MAVLINK_ENABLED_SKYE
}

/**
 * Called by the button
 */
void UASSkyeControlWidget::setMode(int mode)
{
    // Adapt context button mode
    uasMode = mode;
    emit changedMode(mode);
}

void UASSkyeControlWidget::setDirectControlMode(bool checked)
{
    if (checked)
    {
#ifdef  MAVLINK_ENABLED_SKYE
        SkyeMAV* mav = dynamic_cast<SkyeMAV*>(UASManager::instance()->getUASForId(this->uasId));
        if (mav){
            UASInterface* mav = UASManager::instance()->getUASForId(this->uasId);
            if (mav->isArmed())
                setMode(MAV_MODE_DIRECT_CONTROL_ARMED);
            else
                setMode(MAV_MODE_DIRECT_CONTROL_DISARMED);
            transmitMode();
        }
        else
        {
            ui.lastActionLabel->setText("Active UAS is no SKYE!");
        }
#endif  // MAVLINK_ENABLED_SKYE
    }
}

void UASSkyeControlWidget::setAssistedControlMode(bool checked)
{
    if (checked)
    {
#ifdef MAVLINK_ENABLED_SKYE
        SkyeMAV* mav = dynamic_cast<SkyeMAV*>(UASManager::instance()->getUASForId(this->uasId));
        if (mav){
            UASInterface* mav = UASManager::instance()->getUASForId(this->uasId);
            if (mav->isArmed())
                setMode(MAV_MODE_ASSISTED_CONTROL_ARMED);
            else
                setMode(MAV_MODE_ASSISTED_CONTROL_DISARMED);
            transmitMode();
        }
        else
        {
            ui.lastActionLabel->setText("UAS is no SKYE!");
        }
#endif // MAVLINK_ENABLED_SKYE
    }
}

void UASSkyeControlWidget::setHalfAutomaticControlMode(bool checked)
{
    if (checked)
    {
#ifdef MAVLINK_ENABLED_SKYE
        SkyeMAV* mav = dynamic_cast<SkyeMAV*>(UASManager::instance()->getUASForId(this->uasId));
        if (mav){
            UASInterface* mav = UASManager::instance()->getUASForId(this->uasId);
            if (mav->isArmed())
                setMode(MAV_MODE_HALF_AUTOMATIC_ARMED);
            else
                setMode(MAV_MODE_HALF_AUTOMATIC_DISARMED);
            transmitMode();
        }
        else
        {
            ui.lastActionLabel->setText("UAS is no SKYE!");
        }
#endif // MAVLINK_ENABLED_SKYE
    }
}

void UASSkyeControlWidget::setFullAutomaticControlMode(bool checked)
{
    if (checked)
    {
#ifdef MAVLINK_ENABLED_SKYE
        SkyeMAV* mav = dynamic_cast<SkyeMAV*>(UASManager::instance()->getUASForId(this->uasId));
        if (mav){
            UASInterface* mav = UASManager::instance()->getUASForId(this->uasId);
            if (mav->isArmed())
                setMode(MAV_MODE_FULL_AUTOMATIC_ARMED);
            else
                setMode(MAV_MODE_FULL_AUTOMATIC_DISARMED);
            transmitMode();
        }
        else
        {
            ui.lastActionLabel->setText("UAS is no SKYE!");
        }
#endif // MAVLINK_ENABLED_SKYE
    }
}

void UASSkyeControlWidget::setInputMouse(bool checked)
{
#ifdef MAVLINK_ENABLED_SKYE
    if (checked)
    {
        inputMode = QGC_INPUT_MODE_MOUSE;
        emit changedInput(inputMode);
        ui.lastActionLabel->setText(tr("3dMouse activated!"));
    }
    updateStyleSheet();
#endif // MAVLINK_ENABLED_SKYE
}

void UASSkyeControlWidget::setInputTouch(bool checked)
{
#ifdef MAVLINK_ENABLED_SKYE
    if (checked)
    {
        inputMode = QGC_INPUT_MODE_TOUCH;
        emit changedInput(inputMode);
        ui.lastActionLabel->setText(tr("Touchscreen activated!"));
    }
#endif // MAVLINK_ENABLED_SKYE
}

void UASSkyeControlWidget::setInputKeyboard(bool checked)
{
#ifdef MAVLINK_ENABLED_SKYE
    if (checked)
    {
        inputMode = QGC_INPUT_MODE_KEYBOARD;
        emit changedInput(inputMode);
        ui.lastActionLabel->setText(tr("Keyboard activated!"));
    }
#endif // MAVLINK_ENABLED_SKYE
}

void UASSkyeControlWidget::transmitMode()
{
#ifdef MAVLINK_ENABLED_SKYE
    UASInterface* mav = UASManager::instance()->getUASForId(this->uasId);
    if (mav)
    {
        mav->setMode(uasMode);
        QString mode = UAS::getShortModeTextFor(uasMode);

        ui.lastActionLabel->setText(QString("Sent mode %1 to %2").arg(mode).arg(mav->getUASName()));
    }
    else
    {
        ui.lastActionLabel->setText("No UAS activated!");
    }
#endif // MAVLINK_ENABLED_SKYE
}

void UASSkyeControlWidget::cycleContextButton()
{
#ifdef MAVLINK_ENABLED_SKYE
    UAS* mav = dynamic_cast<UAS*>(UASManager::instance()->getUASForId(this->uasId));
    if (mav)
    {
        if (!engineOn)
        {
            if (uasMode)
            {
                mav->armSystem();
                ui.lastActionLabel->setText(QString("Enabled motors on %1").arg(mav->getUASName()));
            } else {
                ui.lastActionLabel->setText("Set mode before!");
            }

        } else {
            mav->disarmSystem();
            ui.lastActionLabel->setText(QString("Disabled motors on %1").arg(mav->getUASName()));
        }
        // Update state now and in several intervals when MAV might have changed state
        updateStatemachine();

        QTimer::singleShot(50, this, SLOT(updateStatemachine()));
        QTimer::singleShot(200, this, SLOT(updateStatemachine()));

    }
#endif // MAVLINK_ENABLED_SKYE
}

void UASSkyeControlWidget::updateStyleSheet()
{
    QString style = "";
    style.append("QPushButton { height: 40; }");
    if (ui.mouseButton->isChecked())
    {
//        qDebug() << "3dMouse TRANSLATION is: " << mouseTranslationEnabled << ", ROTATION is: " << mouseRotationEnabled;
        if (mouseTranslationEnabled && mouseRotationEnabled)
        {
            style.append("QPushButton#mouseButton {image: url(:images/skye_images/input/3dx_spacenavigator_200x198_trans_rot.png);}");
        }
        if (mouseTranslationEnabled && !mouseRotationEnabled)
        {
            style.append("QPushButton#mouseButton {image: url(:images/skye_images/input/3dx_spacenavigator_200x198_trans.png);}");
        }
        if (!mouseTranslationEnabled && mouseRotationEnabled)
        {
            style.append("QPushButton#mouseButton {image: url(:images/skye_images/input/3dx_spacenavigator_200x198_rot.png);}");
        }
        if (!mouseTranslationEnabled && !mouseRotationEnabled)
        {
            style.append("QPushButton#mouseButton {image: url(:images/skye_images/input/3dx_spacenavigator_200x198.png); background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #AA0000, stop: 1 #FF0000);}");
        }
    }else{
        style.append("QPushButton#mouseButton {image: url(:images/skye_images/input/3dx_spacenavigator_200x198.png);}");
    }

    style.append("QPushButton#touchButton {image: url(:images/skye_images/input/FingerPointing.png);}");
    style.append("QPushButton#keyboardButton {image: url(:images/skye_images/input/keyboard-icon_64.png); }");
    style.append("QPushButton:disabled {background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #BBBBBB, stop: 1 #444444); color: #333333 }");
    this->setStyleSheet(style);
}

void UASSkyeControlWidget::changeMouseTranslationEnabled(bool transEnabled)
{
    if (mouseTranslationEnabled != transEnabled)
    {
        mouseTranslationEnabled = transEnabled;
    }
    updateStyleSheet();
}

void UASSkyeControlWidget::changeMouseRotationEnabled(bool rotEnabled)
{
    if (mouseRotationEnabled != rotEnabled)
    {
        mouseRotationEnabled = rotEnabled;
    }
    updateStyleSheet();
}

void UASSkyeControlWidget::triggerLeftBluefoxImageShot()
{
    emit triggeredImageShot(MAV_CAM_ID_BLUEFOX_LEFT);
    ui.lastActionLabel->setText("Left Bluefox image shot requested");
}

void UASSkyeControlWidget::triggerRightBluefoxImageShot()
{
    emit triggeredImageShot(MAV_CAM_ID_BLUEFOX_RIGHT);
    ui.lastActionLabel->setText("Right Bluefox image shot requested");
}

void UASSkyeControlWidget::triggerProsilicaImageShot()
{
    emit triggeredImageShot(MAV_CAM_ID_PROSILICA);
    ui.lastActionLabel->setText("Prosilica image shot requested");
}

void UASSkyeControlWidget::uncheckAllModeButtons()
{
    modeButtonGroup->setExclusive(false);
    QAbstractButton* checkedButton = modeButtonGroup->checkedButton();
    if (checkedButton)
    {
        checkedButton->toggle();
        ui.lastActionLabel->setText("No mode set!");
    }
    modeButtonGroup->setExclusive(true);
}