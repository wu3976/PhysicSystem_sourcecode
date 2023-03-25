#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

#include "PrimeEngine/Lua/LuaEnvironment.h"

#include "SoldierNPCMovementSM.h"
#include "SoldierNPCAnimationSM.h"
#include "SoldierNPC.h"
#include "PhysicManager.h"
#include "PhysicShit.h"

using namespace PE::Components;
using namespace PE::Events;
using namespace CharacterControl::Events;

namespace CharacterControl{

// Events sent by behavior state machine (or other high level state machines)
// these are events that specify where a soldier should move
namespace Events{

PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM_Event_MOVE_TO, Event);

SoldierNPCMovementSM_Event_MOVE_TO::SoldierNPCMovementSM_Event_MOVE_TO(Vector3 targetPos /* = Vector3 */)
: m_targetPosition(targetPos)
{ }

PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM_Event_STOP, Event);

PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM_Event_TARGET_REACHED, Event);
}

namespace Components{

PE_IMPLEMENT_CLASS1(SoldierNPCMovementSM, Component);


SoldierNPCMovementSM::SoldierNPCMovementSM(PE::GameContext &context, PE::MemoryArena arena, PE::Handle hMyself) 
: Component(context, arena, hMyself)
, m_state(STANDING)
{}

SceneNode *SoldierNPCMovementSM::getParentsSceneNode()
{
	PE::Handle hParent = getFirstParentByType<Component>();
	if (hParent.isValid())
	{
		// see if parent has scene node component
		return hParent.getObject<Component>()->getFirstComponent<SceneNode>();
		
	}
	return NULL;
}

void SoldierNPCMovementSM::addDefaultComponents()
{
	Component::addDefaultComponents();

	PE_REGISTER_EVENT_HANDLER(SoldierNPCMovementSM_Event_MOVE_TO, SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_MOVE_TO);
	PE_REGISTER_EVENT_HANDLER(SoldierNPCMovementSM_Event_STOP, SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_STOP);
	
	PE_REGISTER_EVENT_HANDLER(Event_UPDATE, SoldierNPCMovementSM::do_UPDATE);
}

void SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_MOVE_TO(PE::Events::Event *pEvt)
{
	SoldierNPCMovementSM_Event_MOVE_TO *pRealEvt = (SoldierNPCMovementSM_Event_MOVE_TO *)(pEvt);
	
	// change state of this state machine
	m_state = WALKING_TO_TARGET;
	m_targetPostion = pRealEvt->m_targetPosition;

	// make sure the animations are playing
	
	PE::Handle h("SoldierNPCAnimSM_Event_WALK", sizeof(SoldierNPCAnimSM_Event_WALK));
	Events::SoldierNPCAnimSM_Event_WALK *pOutEvt = new(h) SoldierNPCAnimSM_Event_WALK();
	
	SoldierNPC *pSol = getFirstParentByTypePtr<SoldierNPC>();
	pSol->getFirstComponent<PE::Components::SceneNode>()->handleEvent(pOutEvt);

	// release memory now that event is processed
	h.release();
}

void SoldierNPCMovementSM::do_SoldierNPCMovementSM_Event_STOP(PE::Events::Event *pEvt)
{
	Events::SoldierNPCAnimSM_Event_STOP Evt;

	SoldierNPC *pSol = getFirstParentByTypePtr<SoldierNPC>();
	pSol->getFirstComponent<PE::Components::SceneNode>()->handleEvent(&Evt);
}

// calculating a plane and a parametric line's intersection. 
// n: plane normal vector, 
// p: a point on plane, 
// a: a point on parametric line
// v: parametric line's direction vector
Vector3 point_plane_intersection(Vector3 n, Vector3 p, Vector3 a, Vector3 v, bool &success) {
	PrimitiveTypes::Float32 base = (n.getX() * v.getX() + n.getY() * v.getY() + n.getZ() * v.getZ());
	if (abs(base) < 0.0000001f) { 
		success = false;
		return Vector3();
	}
	PrimitiveTypes::Float32 t = (n.getX() * (p.getX() - a.getX()) + n.getY() * (p.getY() - a.getY()) + n.getZ() * (p.getZ() - a.getZ()))
		/ base;
	success = true;
	return a + v * t;
}

bool point_inbound(Vector3 point, std::vector<Vector3>& b_pts) {
	Vector3 AM = point - b_pts[1], AB = b_pts[0] - b_pts[1], AD = b_pts[2] - b_pts[1];
	return (0 < AM * AB < AB * AB) && (0 < AM * AD < AD * AD);
}
/*
Vector3 physic_update_perbox(Vector3 spdvec_in, Vector3 center, PrimitiveTypes::Float32 R, PhysicComponent* physcomp_other, bool dolog) {
	// step 1: for each plane see if distance from plane to center  < R, and if spdvec_in .* normalvector < 0
	//PEINFO("NORMALLLLLLLLLLLLL SIZE %d", (int)physcomp_other->normals.size());
	std::vector<bool> qualified(physcomp_other->normals.size(), false);
	int qualified_count = 0;
	for (int i = 0; i < (int)physcomp_other->normals.size(); i++) {
		if (spdvec_in * physcomp_other->normals[i] <= 0) { 
			Vector3 pvec = center - physcomp_other->boundingpts[i][0]; // vector from a point on plane to sphere center
			PrimitiveTypes::Float32 dist_to_plane = std::abs(pvec * physcomp_other->normals[i] / physcomp_other->normals[i].length()); // projection length
			if (dist_to_plane <= R) {
				qualified[i] = true;
				qualified_count++;
			}
		}
	}
	//PEINFO("INSIDE PER UPDATE");
	if (!qualified_count) { return spdvec_in; }

	// step 2: construct ray and get intersection, check intersection inbound. 
	std::vector<Vector3> intersections(physcomp_other->normals.size());
	for (int i = 0; i < (int)physcomp_other->normals.size(); i++) {
		if (!qualified[i]) { continue; }
		// get intersection
		bool success = true;
		Vector3 rayintersection = point_plane_intersection(
			physcomp_other->normals[i],
			physcomp_other->boundingpts[i][0],
			center, spdvec_in, success);
		if (!success) { // meaning the ray intersect plane everywhere 
			qualified[i] = false;
			continue;
		}
		if ((rayintersection - center) * spdvec_in < 0) { // meaning plane is behind the collision sphere
			qualified[i] = false;
			continue;
		}
		// check intersection inbound
		qualified[i] = point_inbound(rayintersection, physcomp_other->boundingpts[i]);
		intersections[i] = rayintersection;
		
	}
	//PEINFO("INSIDE PER UPDATE2");
	// step 3: find closest qualified intersection, the distancesqr and and its plane

	PrimitiveTypes::Float32 mindistancesqr = std::numeric_limits<float>::max();
	int planenumber = -1;
	for (int i = 0; i < (int)physcomp_other->normals.size(); i++) {
		if (!qualified[i]) { continue; }
		PrimitiveTypes::Float32 currsqr = (intersections[i] - center).lengthSqr();
		if (currsqr < mindistancesqr) {
			mindistancesqr = currsqr;
			planenumber = i;
		}
	}
	if (dolog && planenumber != -1) {
		PEINFO("COLLISION: %f %f %f", intersections[planenumber].getX(), intersections[planenumber].getY(), intersections[planenumber].getZ());
	}
	if (planenumber != -1){
		Vector3 collideNormal = physcomp_other->normals[planenumber];
		Vector3 speedproj = ((spdvec_in * collideNormal) / collideNormal.lengthSqr()) * collideNormal;
		Vector3 updated_spdvec = spdvec_in - speedproj;
		return updated_spdvec;
	}
	
	return spdvec_in;
	
}*/

bool point_inside_box(Vector3 point, Vector3* face_normals, std::vector<Vector3> &face_points, bool test) {
	if (test) {
		PEINFO("POINT, FACE NORMAL AND FACE POINTS for interaction with car");
	}
	for (int i = 0; i < 6; i++) {
		if (test) {
			PEINFO("%f %f %f\n%f %f %f\n%f %f %f\n", point.m_x, point.m_y, point.m_z,
				face_normals[i].m_x, face_normals[i].m_y, face_normals[i].m_z,
				face_points[i].m_x, face_points[i].m_y, face_points[i].m_z);
		}
		
		if ((point - face_points[i]) * face_normals[i] > 0.f) {
			return false;
		}
	}
	return true;
}

Vector3 projection(Vector3 a, Vector3 b) {
	return ((a * b) / b.lengthSqr()) * b;
}

Vector3 physic_update_perbox(Vector3 spdvec_in, Vector3 new_center, std::vector<Vector3> &new_vertices, Vector3 *new_normals, 
	Matrix4x4 m_base, PhysicComponent* physcomp_other) {
	Vector3 p024 = new_vertices[7], p135 = new_vertices[0],
		op024 = physcomp_other->H, op135 = physcomp_other->A;
	std::vector<Vector3> new_facepoints{ p024, p135, p024, p135, p024, p135 },
		other_facepoints{op024, op135, op024, op135, op024, op135};
	bool collide = false;
	for (int i = 0; i < 8; i++) { // check soldier point in static mesh
		if (point_inside_box(new_vertices[i], physcomp_other->normals, other_facepoints, physcomp_other->cartest)) {
			collide = true;
			break;
		}
	}
	if (!collide) { // check static mesh point in soldier
		collide = point_inside_box(physcomp_other->A, new_normals, new_facepoints, physcomp_other->cartest)
			|| point_inside_box(physcomp_other->B, new_normals, new_facepoints, physcomp_other->cartest)
			|| point_inside_box(physcomp_other->C, new_normals, new_facepoints, physcomp_other->cartest)
			|| point_inside_box(physcomp_other->D, new_normals, new_facepoints, physcomp_other->cartest)
			|| point_inside_box(physcomp_other->E, new_normals, new_facepoints, physcomp_other->cartest)
			|| point_inside_box(physcomp_other->F, new_normals, new_facepoints, physcomp_other->cartest)
			|| point_inside_box(physcomp_other->G, new_normals, new_facepoints, physcomp_other->cartest)
			|| point_inside_box(physcomp_other->H, new_normals, new_facepoints, physcomp_other->cartest);
	}
	if (!collide) { // if finally no collision to this box happens, return the original velocity. 
		return spdvec_in;
	}
	// ray_casting method to decide which plane it collided
	bool noDivisionError = true;
	// calculating a plane and a parametric line's intersection. 
// n: plane normal vector, 
// p: a point on plane, 
// a: a point on parametric line
// v: parametric line's direction vector
	int farthest_crashplane_idx = -1;
	PrimitiveTypes::Float32 farthest_distsq = std::numeric_limits<float>::min();
	for (int i = 0; i < 6; i++) {
		// if plane's normal faces different direction of velocity (means crashing on correct side)
		if (spdvec_in * physcomp_other->normals[i] < 0.f) {
			Vector3 curr_inter = point_plane_intersection(
				physcomp_other->normals[i], other_facepoints[i],
				new_center, spdvec_in, noDivisionError);
			if (noDivisionError) {
				Vector3 distvec = curr_inter - new_center;
				PrimitiveTypes::Float32 currdistsq = distvec.lengthSqr();
				if (distvec * spdvec_in < 0) {
					currdistsq *= -1;
				}

				if (currdistsq > farthest_distsq) {
					farthest_distsq = currdistsq;
					farthest_crashplane_idx = i;
				}
			}
		}
	}
	if (farthest_crashplane_idx == -1) {
		PEINFO("NEGATIVE ONE DETECTED, ABORT");
		return spdvec_in;
	}
	
	Vector3 spdProjOnNorm = projection(spdvec_in, physcomp_other->normals[farthest_crashplane_idx]),
		result = spdvec_in - spdProjOnNorm;
	if (result.m_x != spdvec_in.m_x || result.m_z != spdvec_in.m_z) {
		PEINFO("SPEED ADJUST: %f %f %f -> %f %f %f", spdvec_in.m_x, spdvec_in.m_y, spdvec_in.m_z,
			result.m_x, result.m_y, result.m_z);
	}
	
	return result;
}

Vector3 physic_update(Vector3 spdvec_in, SceneNode* soldier, bool dolog) {
	//PEINFO("HEEERRRRRREEEEE");
	Vector3 spdvec_out = spdvec_in;
	PhysicComponent *physcomp = soldier->getFirstComponent<PhysicComponent>();
	//assert(physcomp->is_sphere);
	//PrimitiveTypes::Float32 R = physcomp->data2.getX(); // collision sphere radius
	//Vector3 center = soldier->m_base * physcomp->data1; // collision sphere center
	PhysicManager* pm = PhysicManager::get_instance();
	Vector3 Anew = soldier->m_base * physcomp->A,
		Bnew = soldier->m_base * physcomp->B,
		Cnew = soldier->m_base * physcomp->C,
		Dnew = soldier->m_base * physcomp->D,
		Enew = soldier->m_base * physcomp->E,
		Fnew = soldier->m_base * physcomp->F,
		Gnew = soldier->m_base * physcomp->G,
		Hnew = soldier->m_base * physcomp->H,
		new_center = soldier->m_base * physcomp->center;
	std::vector<Vector3> new_vertices{ Anew, Bnew, Cnew, Dnew, Enew, Fnew, Gnew, Hnew };
	Vector3* new_normals = PhysicComponent::calculatePlaneEquations_helper(
		Anew, Bnew, Cnew, Dnew, Enew, Fnew, Gnew, Hnew
	);
	for (int i = 0; i < pm->physcomponents.size(); i++) {
		//PEINFO("DETECTING OBJECT %d", i);
		PhysicComponent* physcomp_other = pm->physcomponents[i].getObject<PhysicComponent>();
		//assert(!physcomp_other->is_sphere);
		spdvec_out = physic_update_perbox(spdvec_out, new_center, new_vertices, new_normals, soldier->m_base, physcomp_other);
	}
	delete new_normals;
	return spdvec_out;
}

// added for test
//static int framecount = 0;
//static bool back = false;

void SoldierNPCMovementSM::do_UPDATE(PE::Events::Event *pEvt)
{
	if (m_state == WALKING_TO_TARGET)
	{
		// see if parent has scene node component
		SceneNode *pSN = getParentsSceneNode();
		if (pSN)
		{
			Vector3 curPos = pSN->m_base.getPos();
			float dsqr = (m_targetPostion - curPos).lengthSqr();

			bool reached = true;
			if (dsqr > 0.01f)
			{
				// not at the spot yet
				Event_UPDATE *pRealEvt = (Event_UPDATE *)(pEvt);
				static float speed = 1.4f;
				// added
				static float gravityspeed = 6.0f;

				// added for test
				/*speed *= back ? -1.0f : 1.0f;
				if (framecount == 100) {
					back = !back;
					framecount = 0;
				}
				framecount++;*/
				// end
				//PEINFO("%f", speed);
				float allowedDisp = speed * pRealEvt->m_frameTime;
				float dist_gravity = gravityspeed * pRealEvt->m_frameTime;

				Vector3 horizontal_dir = (m_targetPostion - curPos);
				horizontal_dir.m_y = 0.f;
				horizontal_dir.normalize();
				float dist = sqrt(dsqr);
				if (dist > allowedDisp)
				{
					dist = allowedDisp; // can move up to allowedDisp
					reached = false; // not reaching destination yet
				}
				Vector3 horizontal_vector = dist * horizontal_dir,
					gravity_vector = dist_gravity * Vector3(0.f, -1.f, 0.f);
					//totalspd_vector = horizontal_vector + gravity_vector;
				//PEINFO("Collision DeTECTION");
				//Vector3 totalspd_vector_modified = physic_update(totalspd_vector, pSN);
				//PEINFO("HORIZONTAL");
				//PEINFO("VERTICAL");
				Vector3 total_vector = horizontal_vector + gravity_vector,
					total_vector_updated = physic_update(total_vector, pSN, false);
				// instantaneous turn
				pSN->m_base.turnInDirection(horizontal_dir, 3.1415f);
				//pSN->m_base.setPos(curPos + horizontal_dir * dist); // TODO: modify this
				pSN->m_base.setPos(curPos + total_vector_updated);
			}

			if (reached)
			{
				m_state = STANDING;
				
				// target has been reached. need to notify all same level state machines (components of parent)
				{
					PE::Handle h("SoldierNPCMovementSM_Event_TARGET_REACHED", sizeof(SoldierNPCMovementSM_Event_TARGET_REACHED));
					Events::SoldierNPCMovementSM_Event_TARGET_REACHED *pOutEvt = new(h) SoldierNPCMovementSM_Event_TARGET_REACHED();

					PE::Handle hParent = getFirstParentByType<Component>();
					if (hParent.isValid())
					{
						hParent.getObject<Component>()->handleEvent(pOutEvt);
					}
					
					// release memory now that event is processed
					h.release();
				}

				if (m_state == STANDING)
				{
					// no one has modified our state based on TARGET_REACHED callback
					// this means we are not going anywhere right now
					// so can send event to animation state machine to stop
					{
						Events::SoldierNPCAnimSM_Event_STOP evt;
						
						SoldierNPC *pSol = getFirstParentByTypePtr<SoldierNPC>();
						pSol->getFirstComponent<PE::Components::SceneNode>()->handleEvent(&evt);
					}
				}
			}
		}
	}
}

}}




