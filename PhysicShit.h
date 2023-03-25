#pragma once
#define NOMINMAX
// API Abstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"
#include "PrimeEngine/Events/Component.h"
#include "PrimeEngine/Math/Vector3.h"
#include <vector>

namespace PE {
namespace Components {

struct PhysicComponent : public Component {
	PE_DECLARE_CLASS(PhysicComponent)

	// Constructor -------------------------------------------------------------
	PhysicComponent(PE::GameContext& context, PE::MemoryArena arena, Handle hMyself, bool is_sphere);
	
	~PhysicComponent();
	void addDefaultComponents();

	bool calculatePlaneEquations();

	static Vector3* calculatePlaneEquations_helper(
		Vector3 A, Vector3 B, Vector3 C, Vector3 D,
		Vector3 E, Vector3 F, Vector3 G, Vector3 H
	);

	Handle m_hComponentParent;

	// physic properties
	// it is 8 points of a box and a center of the box, and
	// if is_sphere then they are all in local space 
	// Else  they are all in global space
	Vector3 A, B, C, D, E, F, G, H;
	Vector3 center;

	// make sense only if is_sphere is false. 
	//Otherwise they need to be calcultaed in real time.
	Vector3 *normals;  

	/*Vector3 p024, p135,
		n0, 
		n1, 
		n2, 
		n3, 
		n4, 
		n5;*/
	//std::vector<Vector3> normals;
	//std::vector<std::vector<Vector3>> boundingpts;  // either in clockwise or counterclockwise order.
	bool is_sphere;

	// for TEST
	bool cartest;
};

}
}