#pragma once

#include <Bullet3D/btBulletCollisionCommon.h>
#include <Bullet3D/btBulletDynamicsCommon.h>
#include <Bullet3D/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

#include "Engine/ECS/System.h"
#include "Engine/Physics/RayTest.h"

// Forward Declarations
namespace Mega
{
	class Engine;
	class Scene;
};

namespace Mega
{
	class PhysicsSystem : public System
	{
	public:
		enum class eRigidBodyType : int32_t
		{
			Dynamic = 0,
			Static,
		};

		friend Engine;

		using tScalar = tScalarPrecision;
		using Vec3 = Vec3;

		eMegaResult OnInitialize() override;
		eMegaResult OnDestroy() override;
		eMegaResult OnUpdate(const tTimestep in_dt, Scene* in_pScene) override;

		// ------- Public Helpers ------- //
		Mega::Vec3 PerformRayTestPosition(const Vec3& in_from, const Vec3& in_to) const;
		Mega::Vec3 PerformRayTestNormal(const Vec3& in_from, const Vec3& in_to) const;
		bool PerformRayTestCollision(const Vec3& in_from, const Vec3& in_to) const;

		inline void AddInitializedRigidBody(btRigidBody* in_pBody) { m_pPhysicsWorld->addRigidBody(in_pBody); }
		inline void RemoveRigidBody(btRigidBody* in_pBody) { m_pPhysicsWorld->removeRigidBody(in_pBody); }

		// ---------- Getters ---------- //
		constexpr inline tScalar GetGravity() const { return m_globalGravity; }

	private:
		// --------------- ECS Component Callbacks ------------------ //
		void OnConstructRigidBodyComponent(entt::registry& in_registry, entt::entity in_entityID);
		void OnDestroyRigidBodyComponent(entt::registry& in_registry, entt::entity in_entityID);
		void OnConstructCollisionBoxComponent(entt::registry& in_registry, entt::entity in_entityID);
		void OnDestroyCollisionBoxComponent(entt::registry& in_registry, entt::entity in_entityID);
		void OnConstructCollisionHeightFieldComponent(entt::registry& in_registry, entt::entity in_entityID);
		void OnDestroyCollisionHeightFieldComponent(entt::registry& in_registry, entt::entity in_entityID);
		void OnConstructCollisionSphereComponent(entt::registry& in_registry, entt::entity in_entityID);
		void OnDestroyCollisionSphereComponent(entt::registry& in_registry, entt::entity in_entityID);
		void OnConstructCollisionCapsuleComponent(entt::registry& in_registry, entt::entity in_entityID);
		void OnDestroyCollisionCapsuleComponent(entt::registry& in_registry, entt::entity in_entityID);
		void OnConstructCollisionTriangleMeshComponent(entt::registry& in_registry, entt::entity in_entityID);
		void OnDestroyCollisionTriangleMeshComponent(entt::registry& in_registry, entt::entity in_entityID);

		// --------- Member Variables ----------- //
		btDiscreteDynamicsWorld* m_pPhysicsWorld = nullptr;

		btDefaultCollisionConfiguration* m_collisionConfiguration = nullptr;
		btCollisionDispatcher* m_dispatcher = nullptr;
		btBroadphaseInterface* m_overlappingPairCache = nullptr;
		btSequentialImpulseConstraintSolver* m_solver = nullptr;

		tScalar m_globalGravity = -9.8f;
	};

	struct CollisionData
	{
		CollisionData(btPersistentManifold* in_pManifold) { pManifold = in_pManifold; }

		btPersistentManifold* pManifold = nullptr;
	};
}