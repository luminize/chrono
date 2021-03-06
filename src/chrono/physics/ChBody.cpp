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
// Authors: Alessandro Tasora, Radu Serban
// =============================================================================

#include <stdlib.h>
#include <algorithm>

#include "chrono/core/ChTransform.h"
#include "chrono/physics/ChBody.h"
#include "chrono/physics/ChForce.h"
#include "chrono/physics/ChGlobal.h"
#include "chrono/physics/ChMarker.h"
#include "chrono/physics/ChSystem.h"

#include "chrono/collision/ChCModelBullet.h"
#include "chrono/core/ChLinearAlgebra.h"

namespace chrono {

using namespace collision;
using namespace geometry;

// Register into the object factory, to enable run-time dynamic creation and persistence
CH_FACTORY_REGISTER(ChBody)

ChBody::ChBody(ChMaterialSurfaceBase::ContactMethod contact_method) {
    marklist.clear();
    forcelist.clear();

    BFlagsSetAllOFF();  // no flags

    Xforce = VNULL;
    Xtorque = VNULL;

    Force_acc = VNULL;
    Torque_acc = VNULL;
    Scr_force = VNULL;
    Scr_torque = VNULL;

    collision_model = InstanceCollisionModel();

    switch (contact_method) {
        case ChMaterialSurfaceBase::DVI:
            matsurface = std::make_shared<ChMaterialSurface>();
            break;
        case ChMaterialSurfaceBase::DEM:
            matsurface = std::make_shared<ChMaterialSurfaceDEM>();
            break;
    }

    density = 1000.0f;

    last_coll_pos = CSYSNORM;


    max_speed = 0.5f;
    max_wvel = 2.0f * float(CH_C_PI);

    sleep_time = 0.6f;
    sleep_starttime = 0;
    sleep_minspeed = 0.1f;
    sleep_minwvel = 0.04f;
    SetUseSleeping(true);

    variables.SetUserData((void*)this);

    body_id = 0;
}

ChBody::ChBody(ChCollisionModel* new_collision_model, ChMaterialSurfaceBase::ContactMethod contact_method) {
    marklist.clear();
    forcelist.clear();

    BFlagsSetAllOFF();  // no flags

    Xforce = VNULL;
    Xtorque = VNULL;

    Force_acc = VNULL;
    Torque_acc = VNULL;
    Scr_force = VNULL;
    Scr_torque = VNULL;

    collision_model = new_collision_model;
    collision_model->SetContactable(this);

    switch (contact_method) {
        case ChMaterialSurfaceBase::DVI:
            matsurface = std::make_shared<ChMaterialSurface>();
            break;
        case ChMaterialSurfaceBase::DEM:
            matsurface = std::make_shared<ChMaterialSurfaceDEM>();
            break;
    }

    density = 1000.0f;

    last_coll_pos = CSYSNORM;

    max_speed = 0.5f;
    max_wvel = 2.0f * float(CH_C_PI);

    sleep_time = 0.6f;
    sleep_starttime = 0;
    sleep_minspeed = 0.1f;
    sleep_minwvel = 0.04f;
    SetUseSleeping(true);

    variables.SetUserData((void*)this);

    body_id = 0;
}

ChBody::ChBody(const ChBody& other) : ChPhysicsItem(other), ChBodyFrame(other) {
    bflag = other.bflag;

    variables = other.variables;
    variables.SetUserData((void*)this);

    gyro = other.Get_gyro();

    RemoveAllForces();   // also copy-duplicate the forces? Let the user handle this..
    RemoveAllMarkers();  // also copy-duplicate the markers? Let the user handle this..

    ChTime = other.ChTime;

    // also copy-duplicate the collision model? Let the user handle this..
    collision_model = InstanceCollisionModel();

    matsurface = other.matsurface;  // also copy-duplicate the material? Let the user handle this..

    density = other.density;

    Scr_force = other.Scr_force;
    Scr_torque = other.Scr_torque;

    last_coll_pos = other.last_coll_pos;

    max_speed = other.max_speed;
    max_wvel = other.max_wvel;

    sleep_time = other.sleep_time;
    sleep_starttime = other.sleep_starttime;
    sleep_minspeed = other.sleep_minspeed;
    sleep_minwvel = other.sleep_minwvel;
}

ChBody::~ChBody() {
    RemoveAllForces();
    RemoveAllMarkers();

    if (collision_model)
        delete collision_model;
}

ChCollisionModel* ChBody::InstanceCollisionModel() {
    ChCollisionModel* collision_model_t = (ChModelBullet*)new ChModelBullet();
    collision_model_t->SetContactable(this);
    return collision_model_t;
}

//// STATE BOOKKEEPING FUNCTIONS

void ChBody::IntStateGather(const unsigned int off_x,  // offset in x state vector
                            ChState& x,                // state vector, position part
                            const unsigned int off_v,  // offset in v state vector
                            ChStateDelta& v,           // state vector, speed part
                            double& T                  // time
                            ) {
    x.PasteCoordsys(this->coord, off_x, 0);
    v.PasteVector(this->coord_dt.pos, off_v, 0);
    v.PasteVector(this->GetWvel_loc(), off_v + 3, 0);
    T = this->GetChTime();
}

void ChBody::IntStateScatter(const unsigned int off_x,  // offset in x state vector
                             const ChState& x,          // state vector, position part
                             const unsigned int off_v,  // offset in v state vector
                             const ChStateDelta& v,     // state vector, speed part
                             const double T             // time
                             ) {
    this->SetCoord(x.ClipCoordsys(off_x, 0));
    this->SetPos_dt(v.ClipVector(off_v, 0));
    this->SetWvel_loc(v.ClipVector(off_v + 3, 0));
    this->SetChTime(T);
    this->Update();
}

void ChBody::IntStateGatherAcceleration(const unsigned int off_a, ChStateDelta& a) {
    a.PasteVector(this->coord_dtdt.pos, off_a, 0);
    a.PasteVector(this->GetWacc_loc(), off_a + 3, 0);
}

void ChBody::IntStateScatterAcceleration(const unsigned int off_a, const ChStateDelta& a) {
    this->SetPos_dtdt(a.ClipVector(off_a, 0));
    this->SetWacc_loc(a.ClipVector(off_a + 3, 0));
}

void ChBody::IntStateIncrement(const unsigned int off_x,  // offset in x state vector
                               ChState& x_new,            // state vector, position part, incremented result
                               const ChState& x,          // state vector, initial position part
                               const unsigned int off_v,  // offset in v state vector
                               const ChStateDelta& Dv     // state vector, increment
                               ) {
    // ADVANCE POSITION:
    x_new(off_x) = x(off_x) + Dv(off_v);
    x_new(off_x + 1) = x(off_x + 1) + Dv(off_v + 1);
    x_new(off_x + 2) = x(off_x + 2) + Dv(off_v + 2);

    // ADVANCE ROTATION: rot' = delta*rot  (use quaternion for delta rotation)
    ChQuaternion<> mdeltarot;
    ChQuaternion<> moldrot = x.ClipQuaternion(off_x + 3, 0);
    ChVector<> newwel_abs = Amatrix * Dv.ClipVector(off_v + 3, 0);
    double mangle = newwel_abs.Length();
    newwel_abs.Normalize();
    mdeltarot.Q_from_AngAxis(mangle, newwel_abs);
    ChQuaternion<> mnewrot = mdeltarot * moldrot;  // quaternion product
    x_new.PasteQuaternion(mnewrot, off_x + 3, 0);
}

void ChBody::IntLoadResidual_F(const unsigned int off,  // offset in R residual
                               ChVectorDynamic<>& R,    // result: the R residual, R += c*F
                               const double c           // a scaling factor
                               ) {
    // add applied forces to 'fb' vector
    R.PasteSumVector(Xforce * c, off, 0);

    // add applied torques to 'fb' vector, including gyroscopic torque
    if (this->GetNoGyroTorque())
        R.PasteSumVector((Xtorque)*c, off + 3, 0);
    else
        R.PasteSumVector((Xtorque - gyro) * c, off + 3, 0);
}

void ChBody::IntLoadResidual_Mv(const unsigned int off,      // offset in R residual
                                ChVectorDynamic<>& R,        // result: the R residual, R += c*M*v
                                const ChVectorDynamic<>& w,  // the w vector
                                const double c               // a scaling factor
                                ) {
    R(off + 0) += c * GetMass() * w(off + 0);
    R(off + 1) += c * GetMass() * w(off + 1);
    R(off + 2) += c * GetMass() * w(off + 2);
    ChVector<> Iw = GetInertia() * w.ClipVector(off + 3, 0);
    Iw *= c;
    R.PasteSumVector(Iw, off + 3, 0);
}

void ChBody::IntToDescriptor(const unsigned int off_v,  // offset in v, R
                             const ChStateDelta& v,
                             const ChVectorDynamic<>& R,
                             const unsigned int off_L,  // offset in L, Qc
                             const ChVectorDynamic<>& L,
                             const ChVectorDynamic<>& Qc) {
    this->variables.Get_qb().PasteClippedMatrix(v, off_v, 0, 6, 1, 0, 0);  // for solver warm starting only
    this->variables.Get_fb().PasteClippedMatrix(R, off_v, 0, 6, 1, 0, 0);  // solver known term
}

void ChBody::IntFromDescriptor(const unsigned int off_v,  // offset in v
                               ChStateDelta& v,
                               const unsigned int off_L,  // offset in L
                               ChVectorDynamic<>& L) {
    v.PasteMatrix(this->variables.Get_qb(), off_v, 0);
}

////

void ChBody::InjectVariables(ChSystemDescriptor& mdescriptor) {
    this->variables.SetDisabled(!this->IsActive());

    mdescriptor.InsertVariables(&this->variables);
}

void ChBody::VariablesFbReset() {
    this->variables.Get_fb().FillElem(0.0);
}

void ChBody::VariablesFbLoadForces(double factor) {
    // add applied forces to 'fb' vector
    this->variables.Get_fb().PasteSumVector(Xforce * factor, 0, 0);

    // add applied torques to 'fb' vector, including gyroscopic torque
    if (this->GetNoGyroTorque())
        this->variables.Get_fb().PasteSumVector((Xtorque)*factor, 3, 0);
    else
        this->variables.Get_fb().PasteSumVector((Xtorque - gyro) * factor, 3, 0);
}

void ChBody::VariablesFbIncrementMq() {
    this->variables.Compute_inc_Mb_v(this->variables.Get_fb(), this->variables.Get_qb());
}

void ChBody::VariablesQbLoadSpeed() {
    // set current speed in 'qb', it can be used by the solver when working in incremental mode
    this->variables.Get_qb().PasteVector(GetCoord_dt().pos, 0, 0);
    this->variables.Get_qb().PasteVector(GetWvel_loc(), 3, 0);
}

void ChBody::VariablesQbSetSpeed(double step) {
    ChCoordsys<> old_coord_dt = this->GetCoord_dt();

    // from 'qb' vector, sets body speed, and updates auxiliary data
    this->SetPos_dt(this->variables.Get_qb().ClipVector(0, 0));
    this->SetWvel_loc(this->variables.Get_qb().ClipVector(3, 0));

    // apply limits (if in speed clamping mode) to speeds.
    ClampSpeed();

    // compute auxiliary gyroscopic forces
    ComputeGyro();

    // Compute accel. by BDF (approximate by differentiation);
    if (step) {
        this->SetPos_dtdt((this->GetCoord_dt().pos - old_coord_dt.pos) / step);
        this->SetRot_dtdt((this->GetCoord_dt().rot - old_coord_dt.rot) / step);
    }
}

void ChBody::VariablesQbIncrementPosition(double dt_step) {
    if (!this->IsActive())
        return;

    // Updates position with incremental action of speed contained in the
    // 'qb' vector:  pos' = pos + dt * speed   , like in an Eulero step.

    ChVector<> newspeed = variables.Get_qb().ClipVector(0, 0);
    ChVector<> newwel = variables.Get_qb().ClipVector(3, 0);

    // ADVANCE POSITION: pos' = pos + dt * vel
    this->SetPos(this->GetPos() + newspeed * dt_step);

    // ADVANCE ROTATION: rot' = [dt*wwel]%rot  (use quaternion for delta rotation)
    ChQuaternion<> mdeltarot;
    ChQuaternion<> moldrot = this->GetRot();
    ChVector<> newwel_abs = Amatrix * newwel;
    double mangle = newwel_abs.Length() * dt_step;
    newwel_abs.Normalize();
    mdeltarot.Q_from_AngAxis(mangle, newwel_abs);
    ChQuaternion<> mnewrot = mdeltarot % moldrot;
    this->SetRot(mnewrot);
}

void ChBody::SetNoSpeedNoAcceleration() {
    this->SetPos_dt(VNULL);
    this->SetWvel_loc(VNULL);
    this->SetPos_dtdt(VNULL);
    this->SetRot_dtdt(QNULL);
}

////
void ChBody::ClampSpeed() {
    if (this->GetLimitSpeed()) {
        double w = 2.0 * this->coord_dt.rot.Length();
        if (w > max_wvel)
            coord_dt.rot *= max_wvel / w;

        double v = this->coord_dt.pos.Length();
        if (v > max_speed)
            coord_dt.pos *= max_speed / v;
    }
}

//// Utilities for coordinate transformations

ChVector<> ChBody::Point_World2Body(const ChVector<>& mpoint) {
    return ChFrame<double>::TransformParentToLocal(mpoint);
}

ChVector<> ChBody::Point_Body2World(const ChVector<>& mpoint) {
    return ChFrame<double>::TransformLocalToParent(mpoint);
}

ChVector<> ChBody::Dir_World2Body(const ChVector<>& mpoint) {
    return Amatrix.MatrT_x_Vect(mpoint);
}

ChVector<> ChBody::Dir_Body2World(const ChVector<>& mpoint) {
    return Amatrix.Matr_x_Vect(mpoint);
}

ChVector<> ChBody::RelPoint_AbsSpeed(const ChVector<>& mrelpoint) {
    return PointSpeedLocalToParent(mrelpoint);
}

ChVector<> ChBody::RelPoint_AbsAcc(const ChVector<>& mrelpoint) {
    return PointAccelerationLocalToParent(mrelpoint);
}

// The inertia tensor functions

void ChBody::SetInertia(const ChMatrix33<>& newXInertia) {
    variables.SetBodyInertia(newXInertia);
}

void ChBody::SetInertiaXX(const ChVector<>& iner) {
    variables.GetBodyInertia().SetElement(0, 0, iner.x);
    variables.GetBodyInertia().SetElement(1, 1, iner.y);
    variables.GetBodyInertia().SetElement(2, 2, iner.z);
    variables.GetBodyInertia().FastInvert(variables.GetBodyInvInertia());
}
void ChBody::SetInertiaXY(const ChVector<>& iner) {
    variables.GetBodyInertia().SetElement(0, 1, iner.x);
    variables.GetBodyInertia().SetElement(0, 2, iner.y);
    variables.GetBodyInertia().SetElement(1, 2, iner.z);
    variables.GetBodyInertia().SetElement(1, 0, iner.x);
    variables.GetBodyInertia().SetElement(2, 0, iner.y);
    variables.GetBodyInertia().SetElement(2, 1, iner.z);
    variables.GetBodyInertia().FastInvert(variables.GetBodyInvInertia());
}

ChVector<> ChBody::GetInertiaXX() {
    ChVector<> iner;
    iner.x = variables.GetBodyInertia().GetElement(0, 0);
    iner.y = variables.GetBodyInertia().GetElement(1, 1);
    iner.z = variables.GetBodyInertia().GetElement(2, 2);
    return iner;
}

ChVector<> ChBody::GetInertiaXY() {
    ChVector<> iner;
    iner.x = variables.GetBodyInertia().GetElement(0, 1);
    iner.y = variables.GetBodyInertia().GetElement(0, 2);
    iner.z = variables.GetBodyInertia().GetElement(1, 2);
    return iner;
}

void ChBody::ComputeQInertia(ChMatrixNM<double, 4, 4>* mQInertia) {
    ChMatrixNM<double, 3, 4> res;
    ChMatrixNM<double, 3, 4> Gl;
    ChMatrixNM<double, 4, 3> GlT;

    SetMatrix_Gl(Gl, coord.rot);
    GlT.CopyFromMatrixT(Gl);

    res.MatrMultiply(this->GetInertia(), Gl);
    mQInertia->MatrMultiply(GlT, res);  // [Iq]=[G'][Ix][G]
}

//////

void ChBody::Add_as_lagrangian_force(const ChVector<>& force,
                                     const ChVector<>& appl_point,
                                     int local,
                                     ChMatrixNM<double, 7, 1>* mQf) {
    ChVector<> mabsforce;
    ChVector<> mabstorque;
    To_abs_forcetorque(force, appl_point, local, mabsforce, mabstorque);
    mQf->PasteSumVector(mabsforce, 0, 0);
    mQf->PasteSumQuaternion(ChFrame<>::GlT_x_Vect(coord.rot, Dir_World2Body(mabstorque)), 3, 0);
}

void ChBody::Add_as_lagrangian_torque(const ChVector<>& torque, int local, ChMatrixNM<double, 7, 1>* mQf) {
    ChVector<> mabstorque;
    To_abs_torque(torque, local, mabstorque);
    mQf->PasteSumQuaternion(ChFrame<>::GlT_x_Vect(coord.rot, Dir_World2Body(mabstorque)), 3, 0);
}

//////

void ChBody::Accumulate_force(const ChVector<>& force, const ChVector<>& appl_point, int local) {
    ChVector<> mabsforce;
    ChVector<> mabstorque;
    To_abs_forcetorque(force, appl_point, local, mabsforce, mabstorque);

    Force_acc += mabsforce;
    Torque_acc += mabstorque;
}

void ChBody::Accumulate_torque(const ChVector<>& torque, int local) {
    ChVector<> mabstorque;
    To_abs_torque(torque, local, mabstorque);
    Torque_acc += mabstorque;
}

void ChBody::Accumulate_script_force(const ChVector<>& force, const ChVector<>& appl_point, int local) {
    ChVector<> mabsforce;
    ChVector<> mabstorque;
    To_abs_forcetorque(force, appl_point, local, mabsforce, mabstorque);

    Scr_force += mabsforce;
    Scr_torque += mabstorque;
}

void ChBody::Accumulate_script_torque(const ChVector<>& torque, int local) {
    ChVector<> mabstorque;
    To_abs_torque(torque, local, mabstorque);

    Scr_torque += mabstorque;
}

////////

void ChBody::ComputeGyro() {
    ChVector<> Wvel = this->GetWvel_loc();
    gyro = Vcross(Wvel, (variables.GetBodyInertia().Matr_x_Vect(Wvel)));
}

bool ChBody::TrySleeping() {
    BFlagSet(BF_COULDSLEEP, false);

    if (this->GetUseSleeping()) {
        if (!this->IsActive())
            return false;

        // if not yet sleeping:
        if ((this->coord_dt.pos.LengthInf() < this->sleep_minspeed) &&
            (2.0 * this->coord_dt.rot.LengthInf() < this->sleep_minwvel)) {
            if ((this->GetChTime() - this->sleep_starttime) > this->sleep_time) {
                BFlagSet(BF_COULDSLEEP, true);  // mark as sleep candidate
                return true;                    // could go to sleep!
            }
        } else {
            this->sleep_starttime = float(this->GetChTime());
        }
    }
    return false;
}

void ChBody::AddMarker(std::shared_ptr<ChMarker> amarker) {
    // don't allow double insertion of same object
    assert(std::find<std::vector<std::shared_ptr<ChMarker>>::iterator>(marklist.begin(), marklist.end(), amarker) ==
           marklist.end());

    amarker->SetBody(this);
    marklist.push_back(amarker);
}

void ChBody::AddForce(std::shared_ptr<ChForce> aforce) {
    // don't allow double insertion of same object
    assert(std::find<std::vector<std::shared_ptr<ChForce>>::iterator>(forcelist.begin(), forcelist.end(), aforce) ==
           forcelist.end());

    aforce->SetBody(this);
    forcelist.push_back(aforce);
}

void ChBody::RemoveForce(std::shared_ptr<ChForce> mforce) {
    // trying to remove objects not previously added?
    assert(std::find<std::vector<std::shared_ptr<ChForce>>::iterator>(forcelist.begin(), forcelist.end(), mforce) !=
           forcelist.end());

    // warning! linear time search
    forcelist.erase(
        std::find<std::vector<std::shared_ptr<ChForce>>::iterator>(forcelist.begin(), forcelist.end(), mforce));

    mforce->SetBody(0);
}

void ChBody::RemoveMarker(std::shared_ptr<ChMarker> mmarker) {
    // trying to remove objects not previously added?
    assert(std::find<std::vector<std::shared_ptr<ChMarker>>::iterator>(marklist.begin(), marklist.end(), mmarker) !=
           marklist.end());

    // warning! linear time search
    marklist.erase(
        std::find<std::vector<std::shared_ptr<ChMarker>>::iterator>(marklist.begin(), marklist.end(), mmarker));

    mmarker->SetBody(0);
}

void ChBody::RemoveAllForces() {
    for (auto& force : forcelist) {
        force->SetBody(NULL);
    }
    forcelist.clear();
}

void ChBody::RemoveAllMarkers() {
    for (auto& marker : marklist) {
        marker->SetBody(NULL);
    }
    marklist.clear();
}

std::shared_ptr<ChMarker> ChBody::SearchMarker(const char* m_name) {
    return ChContainerSearchFromName<std::shared_ptr<ChMarker>, std::vector<std::shared_ptr<ChMarker>>::iterator>(
        m_name, marklist.begin(), marklist.end());
}

std::shared_ptr<ChForce> ChBody::SearchForce(const char* m_name) {
    return ChContainerSearchFromName<std::shared_ptr<ChForce>, std::vector<std::shared_ptr<ChForce>>::iterator>(
        m_name, forcelist.begin(), forcelist.end());
}

// These are the members used to UPDATE
// the body coordinates during the animation
// Also the coordinates of forces and markers
// linked to the body will be updated.

void ChBody::UpdateMarkers(double mytime) {
    for (auto& marker : marklist) {
        marker->Update(mytime);
    }
}

void ChBody::UpdateForces(double mytime) {
    // COMPUTE LAGRANGIAN FORCES APPLIED TO BODY

    // 1a- force caused by accumulation of forces in body's accumulator Force_acc
    Xforce = Force_acc;

    // 1b- force caused by accumulation of torques in body's accumulator Force_acc
    if (Vnotnull(Torque_acc)) {
        Xtorque = Dir_World2Body(Torque_acc);
    } else {
        Xtorque = VNULL;
    }

    // 2 - accumulation of other applied forces
    ChVector<> mforce;
    ChVector<> mtorque;

    for (auto& force : forcelist) {
        // update positions, f=f(t,q)
        force->Update(mytime);

        force->GetBodyForceTorque(&mforce, &mtorque);
        Xforce += mforce;
        Xtorque += mtorque;
    }

    // 3 - accumulation of script forces
    Xforce += Scr_force;

    if (Vnotnull(Scr_torque)) {
        Xtorque += Dir_World2Body(Scr_torque);
    }

    if (GetSystem()) {
        Xforce += GetSystem()->Get_G_acc() * this->GetMass();
    }
}

void ChBody::UpdateTime(double mytime) {
    ChTime = mytime;
}


// UpdateALL updates the state and time
// of the object AND the dependant (linked)
// markers and forces.

void ChBody::Update(bool update_assets) {
    // TrySleeping();         // See if the body can fall asleep; if so, put it to sleeping
    ClampSpeed();   // Apply limits (if in speed clamping mode) to speeds.
    ComputeGyro();  // Set the gyroscopic momentum.

    // Also update the children "markers" and
    // "forces" depending on the body current state.
    UpdateMarkers(ChTime);

    UpdateForces(ChTime);

    // This will update assets
    ChPhysicsItem::Update(ChTime, update_assets);
}

// As before, but keeps the current state.
// Mostly used for world reference body.
void ChBody::Update(double mytime, bool update_assets) {
    // For the body:
    UpdateTime(mytime);

    Update(update_assets);
}

void ChBody::SetBodyFixed(bool mev) {
    variables.SetDisabled(mev);
    if (mev == BFlagGet(BF_FIXED))
        return;
    BFlagSet(BF_FIXED, mev);
    // RecomputeCollisionModel(); // because one may use different model types for static or dynamic coll.shapes
}

// collision stuff
void ChBody::SetCollide(bool mcoll) {
    if (mcoll == BFlagGet(BF_COLLIDE))
        return;

    if (mcoll) {
        SyncCollisionModels();
        BFlagSetON(BF_COLLIDE);
        if (GetSystem())
            GetSystem()->GetCollisionSystem()->Add(this->GetCollisionModel());
    } else {
        BFlagSetOFF(BF_COLLIDE);
        if (GetSystem())
            GetSystem()->GetCollisionSystem()->Remove(this->GetCollisionModel());
    }
}

void ChBody::ChangeCollisionModel(ChCollisionModel* new_collision_model) {
    if (collision_model) {
        if (system)
            system->GetCollisionSystem()->Remove(collision_model);

        delete collision_model;
    }

    collision_model = new_collision_model;
    collision_model->SetContactable(this);
}

int ChBody::RecomputeCollisionModel() {
    if (!GetCollide())
        return FALSE;  // do nothing unless collision enabled

    collision_model->ClearModel();  // ++++ start geometry definition

    // ... external geometry fetch shapes?

    collision_model->BuildModel();  // ++++ complete geometry definition

    return TRUE;
}

void ChBody::SyncCollisionModels() {
    if (this->GetCollide())
        this->GetCollisionModel()->SyncPosition();
}

void ChBody::AddCollisionModelsToSystem() {
    assert(this->GetSystem());
    SyncCollisionModels();
    if (this->GetCollide())
        this->GetSystem()->GetCollisionSystem()->Add(this->GetCollisionModel());
}

void ChBody::RemoveCollisionModelsFromSystem() {
    assert(this->GetSystem());
    if (this->GetCollide())
        this->GetSystem()->GetCollisionSystem()->Remove(this->GetCollisionModel());
}

void ChBody::GetTotalAABB(ChVector<>& bbmin, ChVector<>& bbmax) {
    if (this->GetCollisionModel())
        this->GetCollisionModel()->GetAABB(bbmin, bbmax);
    else
        ChPhysicsItem::GetTotalAABB(bbmin, bbmax);  // default: infinite aabb
}

ChVector<> ChBody::GetContactPointSpeed(const ChVector<>& abs_point) {
    ChVector<> m_p1_loc = this->Point_World2Body(abs_point);
    return this->PointSpeedLocalToParent(m_p1_loc);
}

void ChBody::ContactForceLoadResidual_F(const ChVector<>& F, const ChVector<>& abs_point, ChVectorDynamic<>& R) {
    ChVector<> m_p1_loc = this->Point_World2Body(abs_point);
    ChVector<> force1_loc = this->Dir_World2Body(F);
    ChVector<> torque1_loc = Vcross(m_p1_loc, force1_loc);
    R.PasteSumVector(F, this->GetOffset_w() + 0, 0);
    R.PasteSumVector(torque1_loc, this->GetOffset_w() + 3, 0);
}

void ChBody::ComputeJacobianForContactPart(const ChVector<>& abs_point,
                                           ChMatrix33<>& contact_plane,
                                           ChVariableTupleCarrier_1vars<6>::type_constraint_tuple& jacobian_tuple_N,
                                           ChVariableTupleCarrier_1vars<6>::type_constraint_tuple& jacobian_tuple_U,
                                           ChVariableTupleCarrier_1vars<6>::type_constraint_tuple& jacobian_tuple_V,
                                           bool second) {
    ChVector<> m_p1_loc = this->Point_World2Body(abs_point);
    ChMatrix33<> Jx1, Jr1;
    ChMatrix33<> Ps1, Jtemp;
    Ps1.Set_X_matrix(m_p1_loc);

    Jx1.CopyFromMatrixT(contact_plane);
    if (!second)
        Jx1.MatrNeg();

    Jtemp.MatrMultiply(this->GetA(), Ps1);
    Jr1.MatrTMultiply(contact_plane, Jtemp);
    if (second)
        Jr1.MatrNeg();

    jacobian_tuple_N.Get_Cq()->PasteClippedMatrix(Jx1, 0, 0, 1, 3, 0, 0);
    jacobian_tuple_U.Get_Cq()->PasteClippedMatrix(Jx1, 1, 0, 1, 3, 0, 0);
    jacobian_tuple_V.Get_Cq()->PasteClippedMatrix(Jx1, 2, 0, 1, 3, 0, 0);
    jacobian_tuple_N.Get_Cq()->PasteClippedMatrix(Jr1, 0, 0, 1, 3, 0, 3);
    jacobian_tuple_U.Get_Cq()->PasteClippedMatrix(Jr1, 1, 0, 1, 3, 0, 3);
    jacobian_tuple_V.Get_Cq()->PasteClippedMatrix(Jr1, 2, 0, 1, 3, 0, 3);
}

void ChBody::ComputeJacobianForRollingContactPart(
    const ChVector<>& abs_point,
    ChMatrix33<>& contact_plane,
    ChVariableTupleCarrier_1vars<6>::type_constraint_tuple& jacobian_tuple_N,
    ChVariableTupleCarrier_1vars<6>::type_constraint_tuple& jacobian_tuple_U,
    ChVariableTupleCarrier_1vars<6>::type_constraint_tuple& jacobian_tuple_V,
    bool second) {
    ChMatrix33<> Jx1, Jr1;

    Jr1.MatrTMultiply(contact_plane, this->GetA());
    if (!second)
        Jr1.MatrNeg();

    jacobian_tuple_N.Get_Cq()->PasteClippedMatrix(Jx1, 0, 0, 1, 3, 0, 0);
    jacobian_tuple_U.Get_Cq()->PasteClippedMatrix(Jx1, 1, 0, 1, 3, 0, 0);
    jacobian_tuple_V.Get_Cq()->PasteClippedMatrix(Jx1, 2, 0, 1, 3, 0, 0);
    jacobian_tuple_N.Get_Cq()->PasteClippedMatrix(Jr1, 0, 0, 1, 3, 0, 3);
    jacobian_tuple_U.Get_Cq()->PasteClippedMatrix(Jr1, 1, 0, 1, 3, 0, 3);
    jacobian_tuple_V.Get_Cq()->PasteClippedMatrix(Jr1, 2, 0, 1, 3, 0, 3);
}

ChVector<> ChBody::GetContactForce() {
    return GetSystem()->GetContactContainer()->GetContactableForce(this);
}

ChVector<> ChBody::GetContactTorque() {
    return GetSystem()->GetContactContainer()->GetContactableTorque(this);
}

//////// FILE I/O

void ChBody::ArchiveOUT(ChArchiveOut& marchive) {
    // version number
    marchive.VersionWrite(1);

    // serialize parent class
    ChPhysicsItem::ArchiveOUT(marchive);
    // serialize parent class
    ChBodyFrame::ArchiveOUT(marchive);

    // serialize all member data:

    marchive << CHNVP(bflag);
    bool mflag;  // more readable flag output in case of ASCII in/out
    mflag = BFlagGet(BF_FIXED);
    marchive << CHNVP(mflag, "is_fixed");
    mflag = BFlagGet(BF_COLLIDE);
    marchive << CHNVP(mflag, "collide");
    mflag = BFlagGet(BF_LIMITSPEED);
    marchive << CHNVP(mflag, "limit_speed");
    mflag = BFlagGet(BF_NOGYROTORQUE);
    marchive << CHNVP(mflag, "no_gyro_torque");
    mflag = BFlagGet(BF_USESLEEPING);
    marchive << CHNVP(mflag, "use_sleeping");
    mflag = BFlagGet(BF_SLEEPING);
    marchive << CHNVP(mflag, "is_sleeping");

    // marchive << CHNVP(marklist);
    // do rather a custom array save:
    marchive.out_array_pre("markers", marklist.size(), "ChMarker");
    for (int i = 0; i < marklist.size(); i++) {
        marchive << CHNVP(marklist[i], "");
        marchive.out_array_between(marklist.size(), "markers");
    }
    marchive.out_array_end(marklist.size(), "markers");

    // marchive << CHNVP(forcelist);
    // do rather a custom array save:
    marchive.out_array_pre("forces", forcelist.size(), "ChForce");
    for (int i = 0; i < forcelist.size(); i++) {
        marchive << CHNVP(forcelist[i], "");
        marchive.out_array_between(forcelist.size(), "forces");
    }
    marchive.out_array_end(forcelist.size(), "forces");

    marchive << CHNVP(body_id);
    marchive << CHNVP(collision_model);
    marchive << CHNVP(gyro);
    marchive << CHNVP(Xforce);
    marchive << CHNVP(Xtorque);
    // marchive << CHNVP(Force_acc); // not useful in serialization
    // marchive << CHNVP(Torque_acc);// not useful in serialization
    // marchive << CHNVP(Scr_force); // not useful in serialization
    // marchive << CHNVP(Scr_torque);// not useful in serialization
    marchive << CHNVP(matsurface);
    // marchive << CHNVP(last_coll_pos);// not useful in serialization
    marchive << CHNVP(density);
    marchive << CHNVP(variables);
    marchive << CHNVP(max_speed);
    marchive << CHNVP(max_wvel);
    marchive << CHNVP(sleep_time);
    marchive << CHNVP(sleep_minspeed);
    marchive << CHNVP(sleep_minwvel);
    marchive << CHNVP(sleep_starttime);
}

/// Method to allow de serialization of transient data from archives.
void ChBody::ArchiveIN(ChArchiveIn& marchive) {
    // version number
    int version = marchive.VersionRead();

    // deserialize parent class
    ChPhysicsItem::ArchiveIN(marchive);
    // deserialize parent class
    ChBodyFrame::ArchiveIN(marchive);

    // stream in all member data:

    marchive >> CHNVP(bflag);
    bool mflag;  // more readable flag output in case of ASCII in/out
    marchive >> CHNVP(mflag, "is_fixed");
    BFlagSet(BF_FIXED, mflag);
    marchive >> CHNVP(mflag, "collide");
    BFlagSet(BF_COLLIDE, mflag);
    marchive >> CHNVP(mflag, "limit_speed");
    BFlagSet(BF_LIMITSPEED, mflag);
    marchive >> CHNVP(mflag, "no_gyro_torque");
    BFlagSet(BF_NOGYROTORQUE, mflag);
    marchive >> CHNVP(mflag, "use_sleeping");
    BFlagSet(BF_USESLEEPING, mflag);
    marchive >> CHNVP(mflag, "is_sleeping");
    BFlagSet(BF_SLEEPING, mflag);

    // marchive >> CHNVP(marklist);
    // do rather a custom array load:
    this->RemoveAllMarkers();
    size_t nummarkers;
    marchive.in_array_pre("markers", nummarkers);
    for (int i = 0; i < nummarkers; i++) {
        std::shared_ptr<ChMarker> a_marker;
        marchive >> CHNVP(a_marker, "");
        this->AddMarker(a_marker);
        marchive.in_array_between("markers");
    }
    marchive.in_array_end("markers");

    // marchive >> CHNVP(forcelist);
    // do rather a custom array load:
    this->RemoveAllForces();
    size_t numforces;
    marchive.in_array_pre("forces", numforces);
    for (int i = 0; i < numforces; i++) {
        std::shared_ptr<ChForce> a_force;
        marchive >> CHNVP(a_force, "");
        this->AddForce(a_force);
        marchive.in_array_between("forces");
    }
    marchive.in_array_end("forces");

    marchive >> CHNVP(body_id);
    marchive >> CHNVP(collision_model);
    collision_model->SetContactable(this);
    marchive >> CHNVP(gyro);
    marchive >> CHNVP(Xforce);
    marchive >> CHNVP(Xtorque);
    // marchive << CHNVP(Force_acc); // not useful in serialization
    // marchive << CHNVP(Torque_acc);// not useful in serialization
    // marchive << CHNVP(Scr_force); // not useful in serialization
    // marchive << CHNVP(Scr_torque);// not useful in serialization
    marchive >> CHNVP(matsurface);
    // marchive << CHNVP(last_coll_pos);// not useful in serialization
    marchive >> CHNVP(density);
    marchive >> CHNVP(variables);
    marchive >> CHNVP(max_speed);
    marchive >> CHNVP(max_wvel);
    marchive >> CHNVP(sleep_time);
    marchive >> CHNVP(sleep_minspeed);
    marchive >> CHNVP(sleep_minwvel);
    marchive >> CHNVP(sleep_starttime);
}

void ChBody::StreamOUTstate(ChStreamOutBinary& mstream) {
    // Do not serialize parent classes and do not
    // implement versioning, because this must be efficient
    // and will be used just for domain decomposition.
    mstream << this->coord.pos.x;
    mstream << this->coord.pos.x;
    mstream << this->coord.pos.x;
    mstream << this->coord.rot.e0;
    mstream << this->coord.rot.e1;
    mstream << this->coord.rot.e2;
    mstream << this->coord.rot.e3;
    mstream << this->coord_dt.pos.x;
    mstream << this->coord_dt.pos.x;
    mstream << this->coord_dt.pos.x;
    mstream << this->coord_dt.rot.e0;
    mstream << this->coord_dt.rot.e1;
    mstream << this->coord_dt.rot.e2;
    mstream << this->coord_dt.rot.e3;
}

void ChBody::StreamINstate(ChStreamInBinary& mstream) {
    // Do not serialize parent classes and do not
    // implement versioning, because this must be efficient
    // and will be used just for domain decomposition.
    mstream >> this->coord.pos.x;
    mstream >> this->coord.pos.x;
    mstream >> this->coord.pos.x;
    mstream >> this->coord.rot.e0;
    mstream >> this->coord.rot.e1;
    mstream >> this->coord.rot.e2;
    mstream >> this->coord.rot.e3;
    this->SetCoord(coord);
    mstream >> this->coord_dt.pos.x;
    mstream >> this->coord_dt.pos.x;
    mstream >> this->coord_dt.pos.x;
    mstream >> this->coord_dt.rot.e0;
    mstream >> this->coord_dt.rot.e1;
    mstream >> this->coord_dt.rot.e2;
    mstream >> this->coord_dt.rot.e3;
    this->SetCoord_dt(coord_dt);

    this->Update();
    this->SyncCollisionModels();
}

}  // end namespace chrono
