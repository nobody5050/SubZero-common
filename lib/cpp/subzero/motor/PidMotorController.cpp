#pragma once

#include "subzero/motor/PidMotorController.h"

using namespace subzero;

template <typename TMotor, typename TController, typename TRelativeEncoder,
          typename TAbsoluteEncoder>
PidMotorController<TMotor, TController, TRelativeEncoder, TAbsoluteEncoder>::
    PidMotorController(std::string name, TMotor &motor,
                       TRelativeEncoder &encoder, TController &controller,
                       PidSettings pidSettings, TAbsoluteEncoder *absEncoder,
                       units::revolutions_per_minute_t maxRpm)
    : IPidMotorController(name), m_motor{motor}, m_controller{controller},
      m_encoder{encoder}, m_absEncoder{absEncoder}, m_settings{pidSettings},
      m_pidController{
          frc::PIDController{pidSettings.p, pidSettings.i, pidSettings.d}},
      m_maxRpm{maxRpm} {
  // Doing it here so the PID controllers themselves get updated
  UpdatePidSettings(pidSettings);
}

template <typename TMotor, typename TController, typename TRelativeEncoder,
          typename TAbsoluteEncoder>
void PidMotorController<TMotor, TController, TRelativeEncoder,
                        TAbsoluteEncoder>::Set(units::volt_t volts) {
  frc::SmartDashboard::PutNumber(m_name + " Commanded volts", volts.value());
  m_motor.SetVoltage(volts);
}

template <typename TMotor, typename TController, typename TRelativeEncoder,
          typename TAbsoluteEncoder>
void PidMotorController<TMotor, TController, TRelativeEncoder,
                        TAbsoluteEncoder>::Set(double percentage) {
  m_motor.Set(percentage);
}

template <typename TMotor, typename TController, typename TRelativeEncoder,
          typename TAbsoluteEncoder>
void PidMotorController<TMotor, TController, TRelativeEncoder,
                        TAbsoluteEncoder>::SetPidTolerance(double tolerance) {
  m_pidController.SetTolerance(tolerance);
}

template <typename TMotor, typename TController, typename TRelativeEncoder,
          typename TAbsoluteEncoder>
void PidMotorController<TMotor, TController, TRelativeEncoder,
                        TAbsoluteEncoder>::Update() {
  if (m_absolutePositionEnabled) {
    // ConsoleWriter.logVerbose(
    //     m_name,
    //     "relative position %0.3f, absolute position %0.3f, absolute target"
    //     "%0.3f",
    //     GetEncoderPosition(), GetAbsoluteEncoderPosition(),
    //     m_absoluteTarget);
    auto effort =
        m_pidController.Calculate(GetEncoderPosition(), m_absoluteTarget);
    double totalEffort = effort;
    Set(units::volt_t(totalEffort));

    if (m_pidController.AtSetpoint()) {
      m_pidController.Reset();
      m_absolutePositionEnabled = false;
      Stop();
    }
  }
}

template <typename TMotor, typename TController, typename TRelativeEncoder,
          typename TAbsoluteEncoder>
void PidMotorController<
    TMotor, TController, TRelativeEncoder,
    TAbsoluteEncoder>::RunWithVelocity(units::revolutions_per_minute_t rpm) {
  m_absolutePositionEnabled = false;
  frc::SmartDashboard::PutNumber(m_name + "commanded rpm", rpm.value());
  m_controller.SetReference(rpm.value(),
                            rev::spark::SparkLowLevel::ControlType::kVelocity);
}

template <typename TMotor, typename TController, typename TRelativeEncoder,
          typename TAbsoluteEncoder>
void PidMotorController<TMotor, TController, TRelativeEncoder,
                        TAbsoluteEncoder>::RunWithVelocity(double percentage) {
  if (abs(percentage) > 1.0) {
    ConsoleWriter.logError("PidMotorController",
                           "Incorrect percentages for motor %s: Value=%.4f ",
                           m_name.c_str(), percentage);
    return;
  }
  auto rpm = units::revolutions_per_minute_t(m_maxRpm) * percentage;
  RunWithVelocity(rpm);
}

template <typename TMotor, typename TController, typename TRelativeEncoder,
          typename TAbsoluteEncoder>
void PidMotorController<TMotor, TController, TRelativeEncoder,
                        TAbsoluteEncoder>::RunToPosition(double position) {
  ConsoleWriter.logVerbose(m_name + " Target position", position);
  Stop();
  m_pidController.Reset();
  m_absolutePositionEnabled = true;
  m_absoluteTarget = position;
}

template <typename TMotor, typename TController, typename TRelativeEncoder,
          typename TAbsoluteEncoder>
std::optional<double>
PidMotorController<TMotor, TController, TRelativeEncoder,
                   TAbsoluteEncoder>::GetAbsoluteEncoderPosition() {
  if (m_absEncoder) {
    return m_absEncoder->GetPosition();
  }

  return std::nullopt;
}

template <typename TMotor, typename TController, typename TRelativeEncoder,
          typename TAbsoluteEncoder>
void PidMotorController<TMotor, TController, TRelativeEncoder,
                        TAbsoluteEncoder>::Stop() {
  m_absolutePositionEnabled = false;
  m_motor.Set(0);
}

template <typename TMotor, typename TController, typename TRelativeEncoder,
          typename TAbsoluteEncoder>
void PidMotorController<TMotor, TController, TRelativeEncoder,
                        TAbsoluteEncoder>::UpdatePidSettings(PidSettings
                                                                 settings) {
  if (settings.p != m_settings.p) {
    ConsoleWriter.logInfo("PidMotorController", "Setting P to %.6f for %s",
                          settings.p, m_name.c_str());
    m_controller.SetP(settings.p);
  }

  if (settings.i != m_settings.i) {
    ConsoleWriter.logInfo("PidMotorController", "Setting I to %.6f for %s",
                          settings.i, m_name.c_str());
    m_controller.SetI(settings.i);
  }

  if (settings.d != m_settings.d) {
    ConsoleWriter.logInfo("PidMotorController", "Setting D to %.6f for %s",
                          settings.d, m_name.c_str());
    m_controller.SetD(settings.d);
  }

  if (settings.iZone != m_settings.iZone) {
    ConsoleWriter.logInfo("PidMotorController", "Setting IZone to %.6f for %s",
                          settings.iZone, m_name.c_str());
    m_controller.SetIZone(settings.iZone);
  }

  if (settings.ff != m_settings.ff) {
    ConsoleWriter.logInfo("PidMotorController", "Setting FF to %.6f for %s",
                          settings.ff, m_name.c_str());
    m_controller.SetFF(settings.ff);
  }

  m_settings = settings;
}