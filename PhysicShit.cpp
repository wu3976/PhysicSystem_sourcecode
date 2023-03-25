#include "PhysicShit.h"
#include "PrimeEngine/Lua/LuaEnvironment.h"

namespace PE {
namespace Components {

PE_IMPLEMENT_CLASS1(PhysicComponent, Component)

PhysicComponent::PhysicComponent(PE::GameContext& context, PE::MemoryArena arena, Handle hMyself, bool is_sphere) 
	: Component(context, arena, hMyself), is_sphere(is_sphere), normals(nullptr), cartest(false){}

void PhysicComponent::addDefaultComponents() {
	Component::addDefaultComponents();
}

PhysicComponent::~PhysicComponent(){
	if (this->normals) { delete this->normals; }
}

bool PhysicComponent::calculatePlaneEquations() {
	if (is_sphere) {
		return false;
	}
	if (this->normals) { delete this->normals; }
	this->normals = calculatePlaneEquations_helper(A, B, C, D, E, F, G, H);
}

Vector3 *PhysicComponent::calculatePlaneEquations_helper(
	Vector3 A, Vector3 B, Vector3 C, Vector3 D,
	Vector3 E, Vector3 F, Vector3 G, Vector3 H
) {
	Vector3* result = new Vector3[6];
	result[0] = (G - E).crossProduct(F - E);
	result[1] = (C - D).crossProduct(B - D);
	result[2] = (G - H).crossProduct(D - H);
	result[3] = (E - A).crossProduct(B - A);
	result[4] = (H - F).crossProduct(B - F);
	result[5] = (G - C).crossProduct(A - C);
	result[0].normalize();
	result[1].normalize();
	result[2].normalize();
	result[3].normalize();
	result[4].normalize();
	result[5].normalize();
	return result;
}
}
}