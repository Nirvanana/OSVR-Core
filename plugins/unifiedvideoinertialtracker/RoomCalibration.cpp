/** @file
    @brief Implementation

    @date 2016

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2016 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Internal Includes
#include "RoomCalibration.h"
#include "ForEachTracked.h"
#include "TrackedBodyIMU.h"
#include "Assumptions.h"

// Library/third-party includes
// - none

// Standard includes
#include <stdexcept>
#include <iostream>

namespace osvr {
namespace vbtracker {
    namespace filters = util::filters;

    using osvr::util::time::duration;

    /// These are the levels that the velocity must remain below for a given
    /// number of consecutive frames before we accept a correlation between the
    /// IMU and video as truth.
    static const auto LINEAR_VELOCITY_CUTOFF = 0.2;
    static const auto ANGULAR_VELOCITY_CUTOFF = 1.e-4;

    /// The number of low-velocity frames required
    static const std::size_t REQUIRED_SAMPLES = 10;

    /// The distance from the camera that we want to encourage users to move
    /// within for best initial startup. Provides the best view of beacons for
    /// initial start of autocalibration.
    static const auto NEAR_MESSAGE_CUTOFF = 0.3;

    RoomCalibration::RoomCalibration()
        : last(util::time::getNow()),
          positionFilter(filters::one_euro::Params{}),
          orientationFilter(filters::one_euro::Params{}) {}

    bool RoomCalibration::wantVideoData(BodyTargetId const &target) const {
        // This was once all in one conditional expression but it was almost
        // impossible for humans to parse, thus I leave it to computers to parse
        // this and optimize it.

        /// we only want video data once we have IMU
        if (!haveIMUData()) {
            return false;
        }
        if (!haveVideoData()) {
            /// If it's our first video data, the body ID should match with the
            /// IMU.
            return target.first == m_imuBody;
        }

        /// If it's not our first video data, it should be from the same target
        /// as our first video data.
        return m_videoTarget == target;
    }

    void RoomCalibration::processVideoData(
        BodyTargetId const &target, util::time::TimeValue const &timestamp,
        Eigen::Vector3d const &xlate, Eigen::Quaterniond const &quat) {
        if (!wantVideoData(target)) {
            // early out if this is the wrong target, or we don't have IMU data
            // yet.
            return;
        }
        bool firstData = !haveVideoData();
        m_videoTarget = target;
        auto dt = duration(timestamp, last);
        last = timestamp;
        if (dt <= 0) {
            dt = 1; // in case of weirdness, avoid divide by zero.
        }

        Eigen::Isometry3d targetPose;
        targetPose.fromPositionOrientationScale(xlate, quat,
                                                Eigen::Vector3d::Ones());

        // Pose of tracked device (in camera space) is cTd
        // orientation is rTd or iTd: tracked device in IMU space (aka room
        // space, modulo yaw)
        // rTc is camera in room space (what we want to find), so we can take
        // camera-reported cTd, perform rTc * cTd, and end up with rTd with
        // position...
        Eigen::Isometry3d rTc = m_imuOrientation * targetPose.inverse();

        // Feed this into the filters...
        positionFilter.filter(dt, rTc.translation());
        orientationFilter.filter(dt, Eigen::Quaterniond(rTc.rotation()));

        // Look at the velocity to see if the user was holding still enough.
        auto linearVel = positionFilter.getDerivativeMagnitude();
        auto angVel = orientationFilter.getDerivativeMagnitude();

        // std::cout << "linear " << linearVel << " ang " << angVel << "\n";
        if (linearVel < LINEAR_VELOCITY_CUTOFF &&
            angVel < ANGULAR_VELOCITY_CUTOFF) {
            // OK, velocity within bounds
            if (reports == 0) {
                msg() << "Hold still, performing room calibration";
            }
            msgStream() << "." << std::flush;
            ++reports;
            if (finished()) {
                /// put an end to the dots
                msgStream() << "\n" << std::endl;
            }
        } else {
            handleExcessVelocity(xlate.z());
        }
    }
    void RoomCalibration::handleExcessVelocity(double zTranslation) {
        // reset the count if movement too fast.
        if (reports > 0) {
            /// put an end to the dots
            msgStream() << std::endl;
        }
        reports = 0;
        switch (m_instructionState) {
        case InstructionState::Uninstructed:
            if (zTranslation > NEAR_MESSAGE_CUTOFF) {
                instructions()
                    << "NOTE: For best results, during tracker/server "
                       "startup, hold your head/HMD still closer than "
                    << NEAR_MESSAGE_CUTOFF
                    << " meters from the tracking camera for a few "
                       "seconds, then rotate slowly in all directions.";
                endInstructions();
                m_instructionState = InstructionState::ToldToMoveCloser;
            }
            break;
        case InstructionState::ToldToMoveCloser:
            if (zTranslation < (0.9 * NEAR_MESSAGE_CUTOFF)) {
                instructions()
                    << "That distance looks good, hold it right there.";
                endInstructions();
                m_instructionState = InstructionState::ToldDistanceIsGood;
            }
            break;
        case InstructionState::ToldDistanceIsGood:
            // nothing to do now!
            break;
        }
    }
    void RoomCalibration::processIMUData(BodyId const &target,
                                         util::time::TimeValue const &timestamp,
                                         Eigen::Quaterniond const &quat) {}

    bool RoomCalibration::finished() const {
        return reports >= REQUIRED_SAMPLES;
    }
    Eigen::Isometry3d RoomCalibration::getRoomToCamera() const {
        Eigen::Isometry3d ret;
        ret.fromPositionOrientationScale(positionFilter.getState(),
                                         orientationFilter.getState(),
                                         Eigen::Vector3d::Ones());
        return ret;
    }
    std::ostream &RoomCalibration::msgStream() const { return std::cout; }
    std::ostream &RoomCalibration::msg() const {
        return msgStream() << "[Unified Tracker: Room Calibration] ";
    }

    std::ostream &RoomCalibration::instructions() const {
        msgStream() << "\n\n";
        return msg();
    }
    void RoomCalibration::endInstructions() const {
        msgStream() << "\n\n" << std::endl;
    }
    bool isRoomCalibrationComplete(TrackingSystem const &sys) {
        // Check for camera pose
        if (!sys.haveCameraPose()) {
            return false;
        }

        // Check all IMUs.
        auto numIMUs = std::size_t{0};
        bool complete = true;
        forEachIMU(sys, [&](TrackedBodyIMU const &imu) {
            complete = complete && imu.calibrationYawKnown();
            ++numIMUs;
        });

#ifdef OSVR_UVBI_ASSUME_SINGLE_IMU
        if (numIMUs > 1) {
            throw std::logic_error("More than one IMU system wide, but the "
                                   "single IMU assumption define is still in "
                                   "place!");
        }
#endif
        return complete;
    }
} // namespace vbtracker
} // namespace osvr
