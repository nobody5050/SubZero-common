#pragma once

#include <frc/controller/HolonomicDriveController.h>
#include <frc/geometry/Pose2d.h>
#include <frc/geometry/Rotation2d.h>
#include <frc/kinematics/ChassisSpeeds.h>

namespace subzero {

/**
 * @brief Interface for classes that move towards a target while maintaining
 * driver input
 *
 */
class ITurnToTarget {
public:
  virtual frc::ChassisSpeeds GetSpeedCorrection() = 0;
  virtual frc::ChassisSpeeds BlendWithInput(const frc::ChassisSpeeds &other,
                                            double correctionFactor) = 0;
  virtual bool AtGoal() = 0;
  virtual void Update() = 0;
};
} // namespace subzero