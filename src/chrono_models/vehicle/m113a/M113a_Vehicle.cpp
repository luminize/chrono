// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All right reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban
// =============================================================================
//
// M113 vehicle model.
//
// =============================================================================

#include "chrono/assets/ChSphereShape.h"
#include "chrono/assets/ChTriangleMeshShape.h"
#include "chrono/utils/ChUtilsInputOutput.h"

#include "chrono_vehicle/ChVehicleModelData.h"

#include "chrono_models/vehicle/m113a/M113a_Chassis.h"
#include "chrono_models/vehicle/m113a/M113a_DrivelineBDS.h"
#include "chrono_models/vehicle/m113a/M113a_SimpleDriveline.h"
#include "chrono_models/vehicle/m113a/M113a_TrackAssemblySinglePin.h"
#include "chrono_models/vehicle/m113a/M113a_Vehicle.h"

namespace chrono {
namespace vehicle {
namespace m113 {

// -----------------------------------------------------------------------------
// Constructors
// -----------------------------------------------------------------------------
M113a_Vehicle::M113a_Vehicle(bool fixed, ChMaterialSurfaceBase::ContactMethod contactMethod)
    : ChTrackedVehicle("M113 Vehicle", contactMethod) {
    Create(fixed);
}

M113a_Vehicle::M113a_Vehicle(bool fixed, ChSystem* system)
    : ChTrackedVehicle("M113 Vehicle", system) {
    Create(fixed);
}

void M113a_Vehicle::Create(bool fixed) {
    // Create the chassis subsystem
    m_chassis = std::make_shared<M113a_Chassis>("Chassis", fixed);

    // Create the track assembly subsystems
    m_tracks[0] = std::make_shared<M113a_TrackAssemblySinglePin>(LEFT);
    m_tracks[1] = std::make_shared<M113a_TrackAssemblySinglePin>(RIGHT);

    // Create the driveline
    m_driveline = std::make_shared<M113a_SimpleDriveline>();
    ////m_driveline = std::make_shared<M113a_DrivelineBDS>();

    GetLog() << "M113 vehicle mass = " << GetVehicleMass() << " kg.\n";
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
void M113a_Vehicle::Initialize(const ChCoordsys<>& chassisPos, double chassisFwdVel) {
    m_chassis->Initialize(m_system, chassisPos, chassisFwdVel);

    // Initialize the left and right track assemblies.
    double track_offset = 1.0795;
    m_tracks[0]->Initialize(m_chassis->GetBody(), ChVector<>(0, track_offset, 0));
    m_tracks[1]->Initialize(m_chassis->GetBody(), ChVector<>(0, -track_offset, 0));

    // Initialize the driveline subsystem
    m_driveline->Initialize(m_chassis->GetBody(), m_tracks[0], m_tracks[1]);
}

}  // end namespace m113
}  // end namespace vehicle
}  // end namespace chrono
