// APIAbstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

// Outer-Engine includes

// Inter-Engine includes
#include "../Lua/LuaEnvironment.h"

// Sibling/Children includes
#include "GameObjectManager.h"
#include "../Sound/SoundManager.h"

#include "PrimeEngine/Scene/Skeleton.h"
#include "PrimeEngine/Scene/MeshInstance.h"
#include "PrimeEngine/Scene/SkeletonInstance.h"
// added
#include "PrimeEngine/Scene/DebugRenderer.h"
#include "PhysicShit.h"
#include "PhysicManager.h"

namespace PE {
namespace Components {

using namespace PE::Events;

PE_IMPLEMENT_CLASS1(GameObjectManager, Component);

// Singleton ------------------------------------------------------------------

// Constructor -------------------------------------------------------------
GameObjectManager::GameObjectManager(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself)
: Component(context, arena, hMyself), m_luaGameObjectTableRef(LUA_NOREF)
, Networkable(context, this, Networkable::s_NetworkId_GameObjectManager) // pre-assigned network id
{
}

	// Methods      ------------------------------------------------------------
void GameObjectManager::addDefaultComponents()
{
	Component::addDefaultComponents();

	addComponent(m_pContext->getLuaEnvironment()->getHandle());

	PE_REGISTER_EVENT_HANDLER(Event_SET_DEBUG_TARGET_HANDLE, GameObjectManager::do_SET_DEBUG_TARGET_HANDLE);
	PE_REGISTER_EVENT_HANDLER(Event_CONSTRUCT_SOUND, GameObjectManager::do_CONSTRUCT_SOUND);

	PE_REGISTER_EVENT_HANDLER(Event_CREATE_LIGHT, GameObjectManager::do_CREATE_LIGHT);
	PE_REGISTER_EVENT_HANDLER(Event_CREATE_MESH, GameObjectManager::do_CREATE_MESH);
	PE_REGISTER_EVENT_HANDLER(Event_CREATE_SKELETON, GameObjectManager::do_CREATE_SKELETON);
	PE_REGISTER_EVENT_HANDLER(Event_CREATE_ANIM_SET, GameObjectManager::do_CREATE_ANIM_SET);

	createGameObjectTableIfDoesntExist();
}

// Individual events -------------------------------------------------------
void GameObjectManager::do_SET_DEBUG_TARGET_HANDLE(Events::Event *pEvt)
{
	Event_SET_DEBUG_TARGET_HANDLE *pRealEvt = (Event_SET_DEBUG_TARGET_HANDLE *)(pEvt);

	Component::s_debuggedComponent = pRealEvt->m_hDebugTarget;
	Component::s_debuggedEvent = pRealEvt->m_debugEvent;
}

void GameObjectManager::do_CREATE_LIGHT(Events::Event *pEvt)
{
	Event_CREATE_LIGHT *pRealEvt = (Event_CREATE_LIGHT *)(pEvt);

	bool haveObject = false;
	Handle exisitngObject;

	putGameObjectTableIOnStack();

	if (!pRealEvt->m_peuuid.isZero())
	{
		// have a valid peeuid for the object. need to check if have one already

		haveObject = m_pContext->getLuaEnvironment()->checkTableValueByPEUUIDFieldExists(pRealEvt->m_peuuid);
		if (haveObject)
		{
			LuaEnvironment::popHandleFromTableOnStackAndPopTable(m_pContext->getLuaEnvironment()->L, exisitngObject);
			m_lastAddedObjHandle = exisitngObject;
		}
		else
		{
			// pop nil
			m_pContext->getLuaEnvironment()->pop();
		}
	}

	if (!haveObject)
	{
		Handle hLight("LIGHT", sizeof(Light));

		Light *pLight = new(hLight) Light(
			*m_pContext,
			m_arena,
			hLight,
			pRealEvt->m_pos, //Position
			pRealEvt->m_u, 
			pRealEvt->m_v, 
			pRealEvt->m_n, //Direction (z-axis)
			pRealEvt->m_ambient, //Ambient
			pRealEvt->m_diffuse, //Diffuse
			pRealEvt->m_spec, //Specular
			pRealEvt->m_att, //Attenuation (x, y, z)
			pRealEvt->m_spotPower, // Spot Power
			pRealEvt->m_range, //Range
			pRealEvt->m_isShadowCaster, //Whether or not it casts shadows
			(PrimitiveTypes::Int32)(pRealEvt->m_type) //0 = point, 1 = directional, 2 = spot
		);
		pLight->addDefaultComponents();

		RootSceneNode::Instance()->m_lights.add(hLight);
		RootSceneNode::Instance()->addComponent(hLight);

		m_pContext->getLuaEnvironment()->pushHandleAsFieldAndSet(pRealEvt->m_peuuid, hLight);
		m_lastAddedObjHandle = hLight;
	}
	else
	{
		// already have this object
		
		// need to reset the orientation
		// and light source settings
		Light *pLight = exisitngObject.getObject<Light>();
		
		pLight->m_base.setPos(pRealEvt->m_pos);
		pLight->m_base.setU(pRealEvt->m_u);
		pLight->m_base.setV(pRealEvt->m_v);
		pLight->m_base.setN(pRealEvt->m_n);
		

		pLight->m_cbuffer.pos = pLight->m_base.getPos();
		pLight->m_cbuffer.dir = pLight->m_base.getN();

		pLight->m_cbuffer.ambient = pRealEvt->m_ambient;
		pLight->m_cbuffer.diffuse = pRealEvt->m_diffuse;
		pLight->m_cbuffer.spec = pRealEvt->m_spec;
		pLight->m_cbuffer.att = pRealEvt->m_att;
		pLight->m_cbuffer.spotPower = pRealEvt->m_spotPower;
		pLight->m_cbuffer.range = pRealEvt->m_range;
		pLight->isTheShadowCaster = pRealEvt->m_isShadowCaster;
	}

	// pop the game object table
	m_pContext->getLuaEnvironment()->pop();
}

void GameObjectManager::do_CREATE_SKELETON(Events::Event *pEvt)
{
	Events::Event_CREATE_SKELETON *pRealEvent = (Events::Event_CREATE_SKELETON *)(pEvt);
	bool haveObject = false;
	Handle exisitngObject;

	putGameObjectTableIOnStack();

	if (!pRealEvent->m_peuuid.isZero())
	{
		// have a valid peeuid for the object. need to check if have one already

		haveObject = m_pContext->getLuaEnvironment()->checkTableValueByPEUUIDFieldExists(pRealEvent->m_peuuid);
		if (haveObject)
		{
			LuaEnvironment::popHandleFromTableOnStackAndPopTable(m_pContext->getLuaEnvironment()->L, exisitngObject);
			m_lastAddedObjHandle = exisitngObject;
		}
		else
		{
			// pop nil
			m_pContext->getLuaEnvironment()->pop();
		}

	}

	if (!haveObject)
	{
		// need to acquire redner context for this code to execute thread-safe
		m_pContext->getGPUScreen()->AcquireRenderContextOwnership(pRealEvent->m_threadOwnershipMask);

		PE::Handle hSkelInstance("SkeletonInstance", sizeof(SkeletonInstance));
		SkeletonInstance *pSkelInstance = new(hSkelInstance) SkeletonInstance(*m_pContext, m_arena, hSkelInstance, Handle());
		pSkelInstance->addDefaultComponents();

		pSkelInstance->initFromFiles(pRealEvent->m_skelFilename, pRealEvent->m_package, pRealEvent->m_threadOwnershipMask);

		m_pContext->getGPUScreen()->ReleaseRenderContextOwnership(pRealEvent->m_threadOwnershipMask);
		
		if (pRealEvent->hasCustomOrientation)
		{
			// need to create a scene node for this mesh
			Handle hSN("SCENE_NODE", sizeof(SceneNode));
			SceneNode *pSN = new(hSN) SceneNode(*m_pContext, m_arena, hSN);
			pSN->addDefaultComponents();

			pSN->m_base.setPos(pRealEvent->m_pos);
			pSN->m_base.setU(pRealEvent->m_u);
			pSN->m_base.setV(pRealEvent->m_v);
			pSN->m_base.setN(pRealEvent->m_n);

			pSN->addComponent(hSkelInstance);

			RootSceneNode::Instance()->addComponent(hSN);
		}
		else
		{
			RootSceneNode::Instance()->addComponent(hSkelInstance);
		}
		m_pContext->getLuaEnvironment()->pushHandleAsFieldAndSet(pRealEvent->m_peuuid, hSkelInstance);
		m_lastAddedObjHandle = hSkelInstance;
		m_lastAddedSkelInstanceHandle = hSkelInstance;
	}
	else
	{
		// already have this object
		// only care about orientation
		if (pRealEvent->hasCustomOrientation)
		{
			// need to reset the orientation
			// try finding scene node
			SkeletonInstance *pSkelInstance = exisitngObject.getObject<SkeletonInstance>();
			Handle hSN = pSkelInstance->getFirstParentByType<SceneNode>();
			if (hSN.isValid())
			{
				SceneNode *pSN = hSN.getObject<SceneNode>();
				pSN->m_base.setPos(pRealEvent->m_pos);
				pSN->m_base.setU(pRealEvent->m_u);
				pSN->m_base.setV(pRealEvent->m_v);
				pSN->m_base.setN(pRealEvent->m_n);
			}
		}
	}
	
	// pop the game object table
	m_pContext->getLuaEnvironment()->pop();
}

void GameObjectManager::do_CREATE_ANIM_SET(Events::Event *pEvt)
{
	Events::Event_CREATE_ANIM_SET *pRealEvent = (Events::Event_CREATE_ANIM_SET *)(pEvt);
	bool haveObject = false;
	Handle exisitngObject;

	putGameObjectTableIOnStack();

	if (!haveObject)
	{
		// need to acquire redner context for this code to execute thread-safe
		m_pContext->getGPUScreen()->AcquireRenderContextOwnership(pRealEvent->m_threadOwnershipMask);

		PEASSERT(m_lastAddedSkelInstanceHandle.isValid(), "Adding anim set, so we need a skeleton instance");
		m_lastAddedSkelInstanceHandle.getObject<SkeletonInstance>()->setAnimSet(pRealEvent->animSetFilename, pRealEvent->m_package);
		m_pContext->getGPUScreen()->ReleaseRenderContextOwnership(pRealEvent->m_threadOwnershipMask);
	}
	// pop the game object table
	m_pContext->getLuaEnvironment()->pop();
}

void GameObjectManager::do_CREATE_MESH(Events::Event *pEvt)
{
	Events::Event_CREATE_MESH *pRealEvent = (Events::Event_CREATE_MESH *)(pEvt);
	
	bool haveObject = false;
	bool haveOtherObject = false;
	Handle exisitngObject;

	putGameObjectTableIOnStack();

	if (!pRealEvent->m_peuuid.isZero())
	{
		// have a valid peeuid for the object. need to check if have one already
		
		haveObject = m_pContext->getLuaEnvironment()->checkTableValueByPEUUIDFieldExists(pRealEvent->m_peuuid);
		if (haveObject)
		{
			LuaEnvironment::popHandleFromTableOnStackAndPopTable(m_pContext->getLuaEnvironment()->L, exisitngObject);
		}
		else
		{
			// pop nil
			m_pContext->getLuaEnvironment()->pop();
		}

		if (haveObject)
		{
			Component *pExisiting = exisitngObject.getObject<Component>();

			if (!pExisiting->isInstanceOf<MeshInstance>())
			{
				haveObject = false; // objects can have same id if they are different types, like skeleton + mesh
				haveOtherObject = true;
			}
		}
	}
	bool test = false;

	if (!haveObject)
	{
	
		// need to acquire redner context for this code to execute thread-safe
		m_pContext->getGPUScreen()->AcquireRenderContextOwnership(pRealEvent->m_threadOwnershipMask);
		
			PE::Handle hMeshInstance("MeshInstance", sizeof(MeshInstance));
			MeshInstance *pMeshInstance = new(hMeshInstance) MeshInstance(*m_pContext, m_arena, hMeshInstance);
			pMeshInstance->addDefaultComponents();

			pMeshInstance->initFromFile(pRealEvent->m_meshFilename, pRealEvent->m_package, pRealEvent->m_threadOwnershipMask);
			// added 
			PEINFO("MESH NAME: %s", pRealEvent->m_meshFilename);
			if (!strcmp(pRealEvent->m_meshFilename, "nazicar.x_carmesh_mesh.mesha")) {
				test = true;
			}
		m_pContext->getGPUScreen()->ReleaseRenderContextOwnership(pRealEvent->m_threadOwnershipMask);
		
		// we need to add this mesh to a scene node or to an existing skeleton
		if (pMeshInstance->hasSkinWeights())
		{
			// this mesh has skin weights, so it should belong to a skeleton. assume the last added skeleton is skeleton we need
			PEASSERT(m_lastAddedSkelInstanceHandle.isValid(), "Adding skinned mesh, so we need a skeleton instance");
			m_lastAddedSkelInstanceHandle.getObject<Component>()->addComponent(hMeshInstance);
		}
		else
		{
			if (pRealEvent->hasCustomOrientation)
			{
				// need to create a scene node for this mesh
				Handle hSN("SCENE_NODE", sizeof(SceneNode));
				SceneNode *pSN = new(hSN) SceneNode(*m_pContext, m_arena, hSN);
				pSN->addDefaultComponents();

				pSN->addComponent(hMeshInstance);

				RootSceneNode::Instance()->addComponent(hSN);
				pSN->m_base.setPos(pRealEvent->m_pos);
				pSN->m_base.setU(pRealEvent->m_u);
				pSN->m_base.setV(pRealEvent->m_v);
				pSN->m_base.setN(pRealEvent->m_n);
				// added 
				MeshInstance* temp = hMeshInstance.getObject<MeshInstance>();
				Mesh* themash = temp->getFirstParentByTypePtr<Mesh>();
				if (DEBUG) {
					DebugRenderer::Instance()->createBoundingBoxMesh(
						themash->localXmin, themash->localXmax,
						themash->localYmin, themash->localYmax,
						themash->localZmin, themash->localZmax, pSN->m_base);
				}
				PE::Handle hPhsyComp("PhysicComponent", sizeof(PhysicComponent));
				PhysicComponent *phsyComp = new(hPhsyComp) PhysicComponent(
					*m_pContext, m_arena, hSN, false
				);
				phsyComp->addDefaultComponents();
				if (test) {
					phsyComp->cartest = true;
				}
				Vector3 pmin(themash->localXmin, themash->localYmin, themash->localZmin);
				Vector3 pmax(themash->localXmax, themash->localYmax, themash->localZmax);
				phsyComp->A = pSN->m_base * Vector3(pmin.getX(), pmin.getY(), pmin.getZ());
				phsyComp->B = pSN->m_base * Vector3(pmin.getX(), pmin.getY(), pmax.getZ());
				phsyComp->C = pSN->m_base * Vector3(pmin.getX(), pmax.getY(), pmin.getZ());
				phsyComp->D = pSN->m_base * Vector3(pmin.getX(), pmax.getY(), pmax.getZ());
				phsyComp->E = pSN->m_base * Vector3(pmax.getX(), pmin.getY(), pmin.getZ());
				phsyComp->F = pSN->m_base * Vector3(pmax.getX(), pmin.getY(), pmax.getZ());
				phsyComp->G = pSN->m_base * Vector3(pmax.getX(), pmax.getY(), pmin.getZ());
				phsyComp->H = pSN->m_base * Vector3(pmax.getX(), pmax.getY(), pmax.getZ());
				phsyComp->center = pSN->m_base * Vector3(
					(pmin.getX() + pmax.getX()) / 2, 
					(pmin.getY() + pmax.getY()) / 2,
					(pmin.getZ() + pmax.getZ()) / 2
				);

				phsyComp->calculatePlaneEquations();
				if (test) {
					PEINFO("nazi car CENTER: %f %f %f", phsyComp->center.m_x, phsyComp->center.m_y, phsyComp->center.m_z);
					PEINFO("nazi car normal vectors: %f %f %f\n%f %f %f\n%f %f %f\n%f %f %f\n%f %f %f\n%f %f %f\n",
						phsyComp->normals[0].m_x, phsyComp->normals[0].m_y, phsyComp->normals[0].m_z,
						phsyComp->normals[1].m_x, phsyComp->normals[1].m_y, phsyComp->normals[1].m_z,
						phsyComp->normals[2].m_x, phsyComp->normals[2].m_y, phsyComp->normals[2].m_z,
						phsyComp->normals[3].m_x, phsyComp->normals[3].m_y, phsyComp->normals[3].m_z,
						phsyComp->normals[4].m_x, phsyComp->normals[4].m_y, phsyComp->normals[4].m_z,
						phsyComp->normals[5].m_x, phsyComp->normals[5].m_y, phsyComp->normals[5].m_z);
					PEINFO("nazicar 8 POINTS: %f %f %f\n%f %f %f\n%f %f %f\n%f %f %f\n%f %f %f\n%f %f %f\n%f %f %f\n%f %f %f\n",
						phsyComp->A.m_x, phsyComp->A.m_y, phsyComp->A.m_z,
						phsyComp->B.m_x, phsyComp->B.m_y, phsyComp->B.m_z,
						phsyComp->C.m_x, phsyComp->C.m_y, phsyComp->C.m_z,
						phsyComp->D.m_x, phsyComp->D.m_y, phsyComp->D.m_z,
						phsyComp->E.m_x, phsyComp->E.m_y, phsyComp->E.m_z,
						phsyComp->F.m_x, phsyComp->F.m_y, phsyComp->F.m_z,
						phsyComp->G.m_x, phsyComp->G.m_y, phsyComp->G.m_z,
						phsyComp->H.m_x, phsyComp->H.m_y, phsyComp->H.m_z);
				}

				static int allowedEvts[] = { 0 };
				pSN->addComponent(hPhsyComp, &allowedEvts[0]);
				PhysicManager::get_instance()->register_physcomponent(hPhsyComp);
			}
			else
			{
				RootSceneNode::Instance()->addComponent(hMeshInstance);
			}
		}

		if (!haveOtherObject)
			m_pContext->getLuaEnvironment()->pushHandleAsFieldAndSet(pRealEvent->m_peuuid, hMeshInstance);
	

	}
	else
	{
		// already have this object
		// only care about orientation
		if (pRealEvent->hasCustomOrientation)
		{
			// need to reset the orientation
			// try finding scene node
			MeshInstance *pMeshInstance = exisitngObject.getObject<MeshInstance>();
			Handle hSN = pMeshInstance->getFirstParentByType<SceneNode>();
			if (hSN.isValid())
			{
				SceneNode *pSN = hSN.getObject<SceneNode>();
				pSN->m_base.setPos(pRealEvent->m_pos);
				pSN->m_base.setU(pRealEvent->m_u);
				pSN->m_base.setV(pRealEvent->m_v);
				pSN->m_base.setN(pRealEvent->m_n);
			}
		}
	}

	// pop the game object table
	m_pContext->getLuaEnvironment()->pop();
}



void GameObjectManager::do_CONSTRUCT_SOUND(Events::Event *pEvt)
{
	Event_CONSTRUCT_SOUND *pRealEvent = (Event_CONSTRUCT_SOUND *)(pEvt);
	SoundManager::Construct(*m_pContext, m_arena, pRealEvent->m_waveBankFilename);
}

void GameObjectManager::createGameObjectTableIfDoesntExist()
{
	if (m_luaGameObjectTableRef == LUA_NOREF)
		m_luaGameObjectTableRef = LuaEnvironment::createTableOnTopOfStackAndStoreReference(m_pContext->getLuaEnvironment()->L);

}
void GameObjectManager::putGameObjectTableIOnStack()
{
	if (m_luaGameObjectTableRef == LUA_NOREF)
		createGameObjectTableIfDoesntExist();
	
	LuaEnvironment::putTableOnTopOfStackByReference(m_luaGameObjectTableRef, m_pContext->getLuaEnvironment()->L);
}

	
}; // namespace Components
}; //namespace PE
