#include "PhysicsSystem.h"

#include <Bullet3D/LinearMath/btQuickprof.h>
#include <Bullet3D/BulletCollision/NarrowPhaseCollision/btRaycastCallback.h>
#include "Bullet3D/Bullet3Collision/NarrowPhaseCollision/b3RaycastInfo.h"

#include "Engine/Engine.h"
#include "Engine/ECS/Entity.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Physics/PhysicsComponents.h"

namespace Mega
{
	eMegaResult PhysicsSystem::OnInitialize()
	{
		// Initialize Bullet 3D //
		m_collisionConfiguration = new btDefaultCollisionConfiguration();

		m_dispatcher = new btCollisionDispatcher(m_collisionConfiguration);
		m_overlappingPairCache = new btDbvtBroadphase();
		m_solver = new btSequentialImpulseConstraintSolver;

		m_pPhysicsWorld = new btDiscreteDynamicsWorld(m_dispatcher, m_overlappingPairCache, m_solver, m_collisionConfiguration);
		m_pPhysicsWorld->setGravity(btVector3(0.0f, m_globalGravity, 0.0f));
		//m_pPhysicsWorld->getDebugDrawer()->setDebugMode(btIDebugDraw::DBG_NoDebug);

		// Connect the creation callbacks for each physics component so they can be added to the world during creation
		auto& registry = Engine::GetScene()->GetRegistry();
		registry.on_construct<Component::CollisionBox>().connect<&PhysicsSystem::OnConstructCollisionBoxComponent>(this);
		registry.on_destroy<Component::CollisionBox>().connect<&PhysicsSystem::OnDestroyCollisionBoxComponent>(this);
		registry.on_construct<Component::CollisionSphere>().connect<&PhysicsSystem::OnConstructCollisionSphereComponent>(this);
		registry.on_destroy<Component::CollisionSphere>().connect<&PhysicsSystem::OnDestroyCollisionSphereComponent>(this);
		registry.on_construct<Component::CollisionHeightField>().connect<&PhysicsSystem::OnConstructCollisionHeightFieldComponent>(this);
		registry.on_destroy<Component::CollisionHeightField>().connect<&PhysicsSystem::OnDestroyCollisionHeightFieldComponent>(this);
		registry.on_construct<Component::CollisionCapsule>().connect<&PhysicsSystem::OnConstructCollisionCapsuleComponent>(this);
		registry.on_destroy<Component::CollisionCapsule>().connect<&PhysicsSystem::OnDestroyCollisionCapsuleComponent>(this);
		registry.on_construct<Component::CollisionTriangleMesh>().connect<&PhysicsSystem::OnConstructCollisionTriangleMeshComponent>(this);
		registry.on_destroy<Component::CollisionTriangleMesh>().connect<&PhysicsSystem::OnDestroyCollisionTriangleMeshComponent>(this);
		registry.on_construct<Component::RigidBody>().connect<&PhysicsSystem::OnConstructRigidBodyComponent>(this);
		registry.on_destroy<Component::RigidBody>().connect<&PhysicsSystem::OnDestroyRigidBodyComponent>(this);

		return eMegaResult::SUCCESS;
	};

	eMegaResult PhysicsSystem::OnDestroy()
	{
		// Cleanup Bullet3D //
		delete m_pPhysicsWorld;
		delete m_solver;
		delete m_overlappingPairCache;
		delete m_dispatcher;
		delete m_collisionConfiguration;

		return eMegaResult::SUCCESS;
	};

	eMegaResult PhysicsSystem::OnUpdate(const tTimestep in_dt, Scene* in_pScene)
	{
		// Update Bullet 3D world //
		m_pPhysicsWorld->stepSimulation(in_dt, 1, 0.033333f);

		// Connect entity's transform and rigid body
		auto view2 = in_pScene->GetRegistry().view<Component::Transform, Component::RigidBody>();
		for (const auto& [entity, t, r] : view2.each())
		{
			// TODO: skip in active / sleeping objects
			if (r.syncRot) { t.SetRotation(r.GetMotionStateTransform()); }
			t.SetPosition(r.GetMotionStatePosition() - r.localOffset);
		}

		// Handle collisions
		int manifoldCount = m_pPhysicsWorld->getDispatcher()->getNumManifolds();
		for (int i = 0; i < manifoldCount; i++)
		{
			btPersistentManifold* contactManifold = m_pPhysicsWorld->getDispatcher()->getManifoldByIndexInternal(i);
			const btRigidBody* body0 = static_cast<const btRigidBody*>(contactManifold->getBody0());
			const btRigidBody* body1 = static_cast<const btRigidBody*>(contactManifold->getBody1());

			MEGA_ASSERT(body0->getUserPointer(), "User pointer is null");
			MEGA_ASSERT(body1->getUserPointer(), "User pointer is null");

			// Pointer magic to get the entity involved in the collision
			// https://www.youtube.com/watch?v=l2VemFmfkG4
			Entity* e0 = static_cast<Entity*>(body0->getUserPointer());
			Entity* e1 = static_cast<Entity*>(body1->getUserPointer());

			e0->OnCollision(e1, { contactManifold });
			e1->OnCollision(e0, { contactManifold });
		}

		return eMegaResult::SUCCESS;
	};

	Mega::Vec3 PhysicsSystem::PerformRayTestPosition(const Vec3& in_from, const Vec3& in_to) const
	{
		if (length(in_from + in_to) <= 0) { return { 0, 0, 0 }; }

		btCollisionWorld::ClosestRayResultCallback closestResults({ in_from.x, in_from.y, in_from.z }, { in_to.x, in_to.y, in_to.z });
		closestResults.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;

		m_pPhysicsWorld->rayTest({ in_from.x, in_from.y, in_from.z }, { in_to.x, in_to.y, in_to.z }, closestResults);

		// For now just two seperate functions
		return { closestResults.m_hitPointWorld.x(), closestResults.m_hitPointWorld.y(), closestResults.m_hitPointWorld.z() };
	}

	Mega::Vec3 PhysicsSystem::PerformRayTestNormal(const Vec3& in_from, const Vec3& in_to) const
	{
		if (length(in_from + in_to) <= 0) { return { 0, 0, 0 }; }

		btCollisionWorld::ClosestRayResultCallback closestResults({ in_from.x, in_from.y, in_from.z }, { in_to.x, in_to.y, in_to.z });
		closestResults.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;

		m_pPhysicsWorld->rayTest({ in_from.x, in_from.y, in_from.z }, { in_to.x, in_to.y, in_to.z }, closestResults);

		// For now just returning the normal
		return { closestResults.m_hitNormalWorld.x(), closestResults.m_hitNormalWorld.y(), closestResults.m_hitNormalWorld.z() };
	}

	bool PhysicsSystem::PerformRayTestCollision(const Vec3& in_from, const Vec3& in_to) const
	{
		btCollisionWorld::ClosestRayResultCallback closestResults({ in_from.x, in_from.y, in_from.z }, { in_to.x, in_to.y, in_to.z });
		closestResults.m_flags |= btTriangleRaycastCallback::kF_FilterBackfaces;

		m_pPhysicsWorld->rayTest({ in_from.x, in_from.y, in_from.z }, { in_to.x, in_to.y, in_to.z }, closestResults);

		// For now just returning the normal
		return closestResults.hasHit();
	}

	// ----------------------- ECS Component Callbacks --------------------------- //
	// ============ Rigid Body ============ //
	void PhysicsSystem::OnConstructRigidBodyComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		// NOT SETTING INITIALIZE ROTATION BTW //
		MEGA_ASSERT(in_registry.all_of<Component::Transform>(in_entityID), \
			"A rigid body needs a transform component to interact with! Add one first");

		Component::RigidBody& bodyComponent = in_registry.get<Component::RigidBody>(in_entityID);
		btRigidBody* physicsBody = bodyComponent.pPhysicsBody;

		// Find correct collision shape
		btCollisionShape* pCollisionShape = nullptr;

		if (in_registry.all_of<Component::CollisionBox>(in_entityID))
			pCollisionShape = in_registry.get<Component::CollisionBox>(in_entityID).pBox;
		if (in_registry.all_of<Component::CollisionSphere>(in_entityID))
			pCollisionShape = in_registry.get<Component::CollisionSphere>(in_entityID).pSphere;
		if (in_registry.all_of<Component::CollisionTriangleMesh>(in_entityID))
			pCollisionShape = in_registry.get<Component::CollisionTriangleMesh>(in_entityID).pTriangleMeshShape;
		if (in_registry.all_of<Component::CollisionCapsule>(in_entityID))
			pCollisionShape = in_registry.get<Component::CollisionCapsule>(in_entityID).pCapsule; // Rename to pShape or sum
		MEGA_ASSERT(pCollisionShape, "Entity does not have suitable collision shape component for rigid body");

		Component::Transform& transformComponent = in_registry.get<Component::Transform>(in_entityID);
		Vec3 t = transformComponent.GetPosition();
		Vec3 r = transformComponent.GetRotation();

		// Set tranform
		btQuaternion quat;
		quat.setEulerZYX(r.x, r.y, r.z);
		btTransform transform = btTransform(quat, btVector3(t.x, t.y, t.z));
		bodyComponent.pMotionState = new btDefaultMotionState(transform);

		// Create Bullet3D body
		btVector3 inertia = btVector3(0.5f, 0.5f, 0.5f);
		if (bodyComponent.type == Component::RigidBody::eRigidBodyType::Static) { bodyComponent.mass = 0; }
		btRigidBody::btRigidBodyConstructionInfo rigidBodyInfo(bodyComponent.mass, bodyComponent.pMotionState, nullptr, inertia);
		rigidBodyInfo.m_friction = bodyComponent.friction;
		rigidBodyInfo.m_collisionShape = pCollisionShape;
		rigidBodyInfo.m_startWorldTransform = transform;
		rigidBodyInfo.m_restitution = bodyComponent.restitution;

		bodyComponent.pPhysicsBody = new btRigidBody(rigidBodyInfo);
		bodyComponent.pPhysicsBody->setCollisionFlags(bodyComponent.pPhysicsBody->getCollisionFlags() | btCollisionObject::CollisionFlags::CF_CUSTOM_MATERIAL_CALLBACK);
		bodyComponent.pPhysicsBody->setActivationState(DISABLE_DEACTIVATION);
		AddInitializedRigidBody(bodyComponent.pPhysicsBody);
	};
	void PhysicsSystem::OnDestroyRigidBodyComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		Component::RigidBody& bodyComponent = in_registry.get<Component::RigidBody>(in_entityID);

		RemoveRigidBody(bodyComponent.pPhysicsBody);

		delete bodyComponent.pMotionState;
		delete bodyComponent.pPhysicsBody;
	};

	// ======== Collision Box ============ //
	void PhysicsSystem::OnConstructCollisionBoxComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		MEGA_ASSERT(!in_registry.all_of<Component::RigidBody>(in_entityID), \
			"Rigid body is already set up! Add collision shapes first");

		Component::CollisionBox& boxComponent = in_registry.get<Component::CollisionBox>(in_entityID);
		btVector3 halfDim = btVector3(boxComponent.dimensions.x / 2.0f, boxComponent.dimensions.y / 2.0f, boxComponent.dimensions.z / 2.0f);

		boxComponent.pBox = new btBoxShape(halfDim);
		boxComponent.pBox->setImplicitShapeDimensions(halfDim);

		btTransform  localTransform;
	};
	void PhysicsSystem::OnDestroyCollisionBoxComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		Component::CollisionBox& boxComponent = in_registry.get<Component::CollisionBox>(in_entityID);

		delete boxComponent.pBox;
	};

	// ================ Collision Triangle Mesh ============== //
	void PhysicsSystem::OnConstructCollisionTriangleMeshComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		Component::CollisionTriangleMesh& meshComponent = in_registry.get<Component::CollisionTriangleMesh>(in_entityID);

		// Load in the triangle data from component's vertex data
		MEGA_ASSERT(!meshComponent.pTriangleArray, "Triangle array already loaded or not nullptr");

		// btTrangleIndexVertexArray constructor arguments
		int triangleCount = (meshComponent.vertexData.indices[1] - meshComponent.vertexData.indices[0]) / 3; //  Triangle stored as 3 indices, so num of indices / 3

		int* indexData = (int*)const_cast<INDEX_TYPE*>(meshComponent.vertexData.pIndexData + meshComponent.vertexData.indices[0]); // The offset of the index data into the global index buffer
		int triangleSize = sizeof(INDEX_TYPE) * 3;
		int vertexCount = meshComponent.vertexData.vertexCount;
		btScalar* vertexData = (btScalar*)const_cast<Vertex*>(meshComponent.vertexData.pVertexData);
		int vertexSize = sizeof(Vertex);

		meshComponent.pTriangleArray = new btTriangleIndexVertexArray(triangleCount, indexData, triangleSize, vertexCount, vertexData, vertexSize);

		// Create the collision shape using the loaded triangle array
		meshComponent.pTriangleMeshShape = new btBvhTriangleMeshShape(meshComponent.pTriangleArray, true, true);
	};

	void PhysicsSystem::OnDestroyCollisionTriangleMeshComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		Component::CollisionTriangleMesh& meshComponent = in_registry.get<Component::CollisionTriangleMesh>(in_entityID);

		delete meshComponent.pTriangleMeshShape;
		delete meshComponent.pTriangleArray;
	};

	// =========== Collision Height Field =========== //
	void PhysicsSystem::OnConstructCollisionHeightFieldComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		MEGA_ASSERT(false, "Collision height fields dont work yet my b");
		MEGA_ASSERT(!in_registry.all_of<Component::RigidBody>(in_entityID), \
			"Rigid body is already set up! Add collision shapes first");

		Component::CollisionHeightField& hf = in_registry.get<Component::CollisionHeightField>(in_entityID);

		const size_t bytes = 257 * 257 * sizeof(float);

		hf.data = new uint8_t[bytes];
		for (size_t i = 0; i < bytes; ++i)
		{
			hf.data[i] = 1; // HEIGHTFIELD'S RANGE IS FROM 0 - 255
		}
		hf.pHeightField = new btHeightfieldTerrainShape(17, 17, &hf.data, 1, -1, 1, 2, PHY_FLOAT, false);
	}
	void PhysicsSystem::OnDestroyCollisionHeightFieldComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		Component::CollisionHeightField& hf = in_registry.get<Component::CollisionHeightField>(in_entityID);

		delete hf.pHeightField;
	}

	// =========== Collision Sphere =========== //
	void PhysicsSystem::OnConstructCollisionSphereComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		MEGA_ASSERT(!in_registry.all_of<Component::RigidBody>(in_entityID), \
			"Rigid body is already set up! Add collision shapes first");

		Component::CollisionSphere& sphereComponent = in_registry.get<Component::CollisionSphere>(in_entityID);
		float rad = sphereComponent.radius;

		sphereComponent.pSphere = new btSphereShape(rad);
		sphereComponent.pSphere->setImplicitShapeDimensions(btVector3(rad, rad, rad));

		btTransform  localTransform;
	};

	void PhysicsSystem::OnDestroyCollisionSphereComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		Component::CollisionSphere& sphereComponent = in_registry.get<Component::CollisionSphere>(in_entityID);

		delete sphereComponent.pSphere;
	};

	// ---------------- Collision Capsule ------------------- //
	void PhysicsSystem::OnConstructCollisionCapsuleComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		MEGA_ASSERT(!in_registry.all_of<Component::RigidBody>(in_entityID), "Rigid body is already set up! Add collision shapes first");

		Component::CollisionCapsule& capsule = in_registry.get<Component::CollisionCapsule>(in_entityID);

		capsule.pCapsule = new btCapsuleShape(capsule.radius, capsule.height / static_cast<tScalar>(2.0));

		btTransform  localTransform;
	};

	void PhysicsSystem::OnDestroyCollisionCapsuleComponent(entt::registry& in_registry, entt::entity in_entityID)
	{
		Component::CollisionCapsule& capsule = in_registry.get<Component::CollisionCapsule>(in_entityID);

		delete capsule.pCapsule;
	};



} // namespace Mega










	// Checks if two bodies collided in the last world step
//bool RigidBody3D::HasCollidedWith(const std::shared_ptr<RigidBody3D> in_body)
//{
//	assert(m_currentScene == in_body->m_currentScene && "ERROR: Testing collision between bodies in different scenes");
//
//	auto pWorld = m_currentScene->m_pPhysicsWorld;
//	int manifoldCount = pWorld->getDispatcher()->getNumManifolds();
//
//	for (int i = 0; i < manifoldCount; i++)
//	{
//		btPersistentManifold* contactManifold = pWorld->getDispatcher()->getManifoldByIndexInternal(i);
//		const btCollisionObject* body0 = contactManifold->getBody0();
//		const btCollisionObject* body1 = contactManifold->getBody1();
//
//		if ((m_pRigidBody == body0 || in_body->m_pRigidBody == body0)
//			&& (m_pRigidBody == body1 || in_body->m_pRigidBody == body1)) {
//			return true;
//		}
//	}
//
//	return false;
//}