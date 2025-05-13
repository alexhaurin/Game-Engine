#pragma once

#include <cstdint>

#include <Bullet3D/btBulletCollisionCommon.h>
#include <Bullet3D/btBulletDynamicsCommon.h>
#include <Bullet3D/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

#include "Engine/ECS/Components.h"
#include "Engine/Physics/PhysicsSystem.h"
#include "Engine/Graphics/Objects/Model.h"
#include "Engine/Graphics/Objects/Vertex.h"

namespace Mega
{
	namespace Component
	{
		// Physics
		struct RigidBody : public ComponentBase
		{
			using tScalar = PhysicsSystem::tScalar;
			using eRigidBodyType = PhysicsSystem::eRigidBodyType;

			RigidBody() {};
			RigidBody(eRigidBodyType in_type)
				: type(in_type) {};
			RigidBody(eRigidBodyType in_type, tScalar in_mass, tScalar in_friction, tScalar in_restitution)
				: type(in_type), mass(in_mass), friction(in_friction), restitution(in_restitution) {};

			// Getters/Setters
			Mat4x4 GetMotionStateTransform() const;
			Vec3 GetMotionStatePosition() const;
			Vec3 GetLinearVelocity() const;
			tScalar GetGravity() const { return gravity; }
			void SetLinearVelocity(const Vec3& in_velocity) { pPhysicsBody->setLinearVelocity({ in_velocity.x, in_velocity.y, in_velocity.z }); }
			void SetGravity(tScalar in_gravity) { pPhysicsBody->setGravity({ 0, in_gravity, 0 }); gravity = in_gravity; }
			void SyncRotation(bool in_bool);

			eRigidBodyType type = eRigidBodyType::Dynamic;
			tScalar mass = 1.0;
			tScalar friction = 1.0;
			tScalar restitution = 0.3f;
			tScalar gravity = -9.8f;

			bool syncRot = true;

			btPersistentManifold* pCollisionManifoldData = nullptr;
			btRigidBody* pPhysicsBody = nullptr;
			btDefaultMotionState* pMotionState = nullptr;
			Vec3 localOffset = { 0, 0, 0 };
		};

		struct CollisionSphere : public ComponentBase
		{
			CollisionSphere() {};
			CollisionSphere(const float in_radius)
				: radius(in_radius) {};

			float radius = 1.0f;

			btSphereShape* pSphere = nullptr;
		};

		struct CollisionBox : public ComponentBase
		{
			CollisionBox() {};
			CollisionBox(const Vec3& in_dim)
				: dimensions(in_dim) {};

			Vec3 dimensions = Vec3(1.0f, 1.0f, 1.0f);

			btBoxShape* pBox = nullptr;
		};

		// TODO: move components to system folders?
		struct CollisionCapsule : public ComponentBase
		{
			CollisionCapsule(const float in_radius, const float in_height)
				: radius(in_radius), height(in_height) {};

			float radius = 0.5f;
			float height = 1.0f;
			btCapsuleShape* pCapsule = nullptr;
		};

		struct CollisionTriangleMesh : public ComponentBase
		{
			CollisionTriangleMesh(const VertexData& in_vertexData)
				: vertexData(in_vertexData) {};
			~CollisionTriangleMesh() {};

			btBvhTriangleMeshShape* pTriangleMeshShape = nullptr;
			btTriangleIndexVertexArray* pTriangleArray = nullptr;

			VertexData vertexData;
		};

		struct CollisionHeightField : public ComponentBase
		{
			CollisionHeightField() {};
			~CollisionHeightField() { delete data; };

			btHeightfieldTerrainShape* pHeightField = nullptr;

			uint8_t* data = nullptr;
		};
	} // namespace Component
} // namespace Mega