#include "PhysicManager.h"

PhysicManager* PhysicManager::instance = nullptr;

PhysicManager::PhysicManager(){}
PhysicManager::~PhysicManager(){}
void PhysicManager::register_physcomponent(PE::Handle& h) {
	physcomponents.push_back(h);
}

PhysicManager* PhysicManager::get_instance() {
	if (!instance) {
		instance = new PhysicManager();
	}
	return instance;
}
