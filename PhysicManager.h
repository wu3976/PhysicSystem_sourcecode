#pragma once
#define NOMINMAX
// API Abstraction
#include "PhysicShit.h"
#include <vector>



struct PhysicManager{

	PhysicManager(const PhysicManager& other) = delete;
	PhysicManager& operator=(const PhysicManager& other) = delete;

	void register_physcomponent(PE::Handle& h);
	static PhysicManager *get_instance();

	std::vector<PE::Handle> physcomponents;

private:
	PhysicManager();
	~PhysicManager();

	static PhysicManager* instance;

	
};
