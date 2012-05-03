#ifndef SKYEMAV_H
#define SKYEMAV_H

#include <QPolygonF>
#include <QTimer>
#include "UAS.h"
#include "BluefoxReconfigure.h"

class SkyeMAV : public UAS
{
    Q_OBJECT
    Q_INTERFACES(UASInterface)
public:
    SkyeMAV(MAVLinkProtocol* mavlink, int id);
    ~SkyeMAV();


    /** @brief Get the airframe */
    int getAirframe() const
    {
        return airframe;
    }
    /** @brief Provide encapsulated mavlink image */
    QImage getImage();
    /** @brief Get mode */
    uint8_t getMode();
    /** @brief */
    int getCurrentTrajectoryStamp() { return currentTrajectoryStamp; }

public slots:
    /** @brief Set the specific airframe type */
    void setAirframe(int airframe)
    {
        this->airframe = airframe;
        emit systemSpecsChanged(uasId);
    }
    /** @brief Receive a MAVLink message from this MAV */
    void receiveMessage(LinkInterface* link, mavlink_message_t message);
    /** @brief Send the 6 DOF command (from 3d Mouse) to MAV */
    void setManualControlCommands6DoF(double x , double y , double z , double a , double b, double c);
    /** @brief Send the 8 DOF command (from Testphase Widget) to MAV */
    void setTestphaseCommandsByWidget(int Thrust1 , int Thrust2 , int Thrust3 , int Thrust4 , int Orientation1 , int Orientation2, int Orientation3, int Orientation4 ); //AL (06.03.12)
    /** @brief Set multiplication factor for manual control */
    void setSensitivityFactorTrans(float val) {sensitivityFactorTrans = val;}
    /** @brief Set multiplication factor for manual control */
    void setSensitivityFactorRot(float val) {sensitivityFactorRot = val;}

    /** @brief Send command to take single image with camera and send it to groundstation */
    void takeImageShot(MAV_CAM_ID cam);
    /** Send Boolean to MAV, if motors should be homed*/
    void sendHomingCommand();
    /** Send configurations for bluefox camera*/
    void sendBluefoxReconfigureCommand(mavlink_skye_cam_reconfigure_bluefox_settings_t* bluefox);
    /** Request for current configurations for bluefox camera*/
    void requestBluefoxSettings();

    void followTrajectory();

signals:
    /** @brief Emit new detailed accu info for one accu pack
     *  @param packID see ENUM MAV_SKYE_BATTERY_PACK_ID
     *          voltCell1 Voltage of cell 1 in volt
     *          voltCell2 Voltage of cell 2 in volt
     *          voltCell3 Voltage of cell 3 in volt
     *          voltCell4 Voltage of cell 4 in volt
     *          current Recent current of accu pack in ampere
     *          percent Estimated remaining battery, -1 when not estimated
    **/
    void batteryPackChanged(mavlink_skye_battery_status_t* battery);
    void reportUDPLinkFailed(QString errorStr);

    /** @brief  Emit current camera reconfiguration settings **/
    void bluefoxSettingsChanged(mavlink_skye_cam_reconfigure_bluefox_settings_t* bluefox);

protected:
    /** @brief Send a Direct Control Command to MAV: */
    void sendDirectControlCommands(double xThrust, double yThrust, double zThrust, double xMoment, double yMoment, double zMoment);
    /** @brief Send a Assisted Control Command to MAV: */
    void sendAssistedControlCommands(double xVel, double yVel, double zVel, double xRot, double yRot, double zRot);
    /** @brief Send a Testphase Control Command to MAV: */
    void sendTestphaseControlCommands(int Thrust1 , int Thrust2 , int Thrust3 , int Thrust4 , int Orientation1 , int Orientation2, int Orientation3, int Orientation4 );

    int airframe;                   ///< The airframe type

    int manual1Thrust;          ///< Thrust for Motor 1 set by human pilot
    int manual2Thrust;          ///< Thrust for Motor 2 set by human pilot
    int manual3Thrust;          ///< Thrust for Motor 3 set by human pilot
    int manual4Thrust;          ///< Thrust for Motor 4 set by human pilot
    int manual1Orientation;     ///< Orientation for Motor 1 set by human pilot
    int manual2Orientation;     ///< Orientation for Motor 2 set by human pilot
    int manual3Orientation;     ///< Orientation for Motor 3 set by human pilot
    int manual4Orientation;     ///< Orientation for Motor 4 set by human pilot

    double manualXThrust;      ///< X Thrust set by human pilot
    double manualYThrust;      ///< Y Thrust set by human pilot
    double manualZThrust;      ///< Z Thrust set by human pilot
    double manualXMoment;      ///< Moment around X axis set by human pilot
    double manualYMoment;      ///< Moment around Y axis set by human pilot
    double manualZMoment;      ///< Moment around Z axis set by human pilot

    double manualXVel;          ///< X velocity set by human pilot
    double manualYVel;          ///< Y velocity set by human pilot
    double manualZVel;          ///< Z velocity set by human pilot
    double manualXRot;          ///< Roll (x axis) set by human pilot
    double manualYRot;          ///< Pitch (y axis) set by human pilot
    double manualZRot;          ///< Yaw (z axis) set by human pilot

    float sensitivityFactorTrans;    ///< Translational factor for manual control [remark: abs(deviceInput) <= 1 ]
    float sensitivityFactorRot;      ///< Rotational factor for manual control [remark: abs(deviceInput) <= 1 ]

    int currentTrajectoryStamp;
    QTimer trajectoryTimer;
};

#endif // SKYEMAV_H
