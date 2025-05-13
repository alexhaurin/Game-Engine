#pragma once

#include <vector>
#include <iostream>
#include <typeinfo>
#include <type_traits>
#include <entt/entt.hpp>

#include "Engine/Core/Core.h"
#include "Engine/ECS/Components.h"
#include "Engine/Physics/PhysicsComponents.h"

#define ENTITY_PHYSICS const type_info& GetTypeID() const override { return typeid(this); }

namespace Mega
{
	// Forward declaration
	class Scene;

	enum class eEntityState
	{
		Created = 1,
		Initialized = 2,
		Destroyed = 3
	};

	class Entity
	{
	public:
		friend Scene;

		using tChildIndex = int32_t;
		using tChild = Entity*;
		using tOwner = Entity*;

		// Determines whether or not the eneity is updated every frame
		void SetIsActive(bool in_isActive) { m_isActive = in_isActive; }
		bool IsActive() const { return m_isActive; }

		// Entity Guard
		eEntityState GetLifetimeState() const { return m_state; }
		bool IsCreated() const { return GetLifetimeState() == eEntityState::Created; }
		bool IsInitialized() const { return GetLifetimeState() == eEntityState::Initialized; }
		bool IsDestroyed() const { return GetLifetimeState() == eEntityState::Destroyed; }
		
		// ECS
		template<typename T>
		bool HasComponent() const
		{
			MEGA_ASSERT(m_pRegistry->valid(m_enttID), "Checking if an invalid entity has a component");
			return m_pRegistry->all_of<T>(m_enttID);
		}

		// Overwritable functions for scripting
		virtual void OnCollision(const Entity* in_pEntity, const CollisionData& in_collisionData) {};
		Vec3 GetPosition() const { return m_pTransformComponent->GetPosition(); }

		void Destroy()
		{
			MEGA_ASSERT(IsInitialized(), "Destroying an entity before it has been initialized");

			OnDestroy();

			// Remove from ownership chain
			if (m_owner != nullptr && m_owner != this)
			{
				m_owner->RemoveChild(m_childIndex);
			}

			// Remove Children
			for (auto child : m_children)
			{
				child->Destroy();
			}

			SetIsActive(false);
			SetLifetimeState(eEntityState::Destroyed);
		};

	protected:
		// Protected constructor / destructor - only Scene can create and destroy entities
		Entity() = default;
		~Entity()
		{
			MEGA_ASSERT(IsDestroyed(), "Deleting entity before it has been properly destroyed");
		}

		virtual void OnInitialize() {};
		virtual void OnUpdate(const tTimestep in_dt) {};
		virtual void OnUpdatePost(const tTimestep in_dt) {};
		virtual void OnDestroy() {};

		// Ownership System
		//template<typename tEntityType, class... Args>
		//tEntityType* AddChild(Args&&... in_args)
		//{
		//	m_pScene->AddEntity<tEntityType>(in_args);
		//}

		// ECS
		// Adds a child entity to the scene
		template<typename T, class... Args>
		T& AddComponent(Args&&... args)
		{
			MEGA_STATIC_ASSERT(std::is_base_of<Mega::Component::ComponentBase, T>::value == true, "Structs added to Entities as components must be derived from Mega::Component!");
			MEGA_ASSERT(m_pRegistry->valid(m_enttID), "Adding component to entity that is not valid");
			MEGA_ASSERT(!HasComponent<T>(), "Entity already has component!");
			return m_pRegistry->emplace<T>(m_enttID, std::forward<Args>(args)...);
		}
		template<typename T>
		void RemoveComponent()
		{
			MEGA_STATIC_ASSERT(std::is_base_of<Mega::Component::ComponentBase, T>::value == true, "Removing a component not derived from Mega::Component!");
			MEGA_ASSERT(m_pRegistry->valid(m_enttID), "Removing component from invalid entity");
			MEGA_ASSERT(HasComponent<T>(), "Entity does not have component");
			m_pRegistry->remove<T>(m_enttID);
		}
		template<typename T>
		// TODO: Add move semantics
		T& GetComponent()
		{
			MEGA_STATIC_ASSERT(std::is_base_of<Mega::Component::ComponentBase, T>::value == true, "Retrieving a component not derived from Mega::Component!");
			MEGA_ASSERT(m_pRegistry->valid(m_enttID), "Getting component from invalid entity");
			MEGA_ASSERT(HasComponent<T>(), "Entity does not have component");
			return m_pRegistry->get<T>(m_enttID);
		}

		// RTTI for collision interface
		static const char* TypeOf(const Entity* in_pEntity) { return typeid(*in_pEntity).name(); }
		template<typename T> static const char* TypeOf() { return typeid(T).name(); }


		// Default transform Getters/Setters
		inline Vec3 GetScale() const { return m_pTransformComponent->GetScale(); }
		inline Vec3 GetRotation() const { return m_pTransformComponent->GetRotation(); }
		inline void SetPosition(const Vec3& in_pos) const { return m_pTransformComponent->SetPosition(in_pos); }
		inline void SetRotation(const Vec3& in_rot) const { return m_pTransformComponent->SetRotation(in_rot); }
		inline void SetScale(const Vec3& in_scale) const { return m_pTransformComponent->SetScale(in_scale); }

		inline Component::Transform* GetTransformComponent() { return m_pTransformComponent; }

	private:
		// Not copyable or movable
		Entity(const Entity&) = delete;
		Entity(Entity&&) = delete;
		Entity& operator=(const Entity&) = delete;
		Entity& operator=(Entity&&) = delete;

		void SetLifetimeState(eEntityState in_state) { m_state = in_state; }

		// Update Control System
		void Initialize()
		{
			MEGA_ASSERT(IsCreated(), "Initializing entity from a state other than 'created'");
			MEGA_ASSERT(m_pScene != nullptr, "Initializing entity with null scene");
			MEGA_ASSERT(m_enttID != entt::null, "Initializing entity with null entt ID");
			MEGA_ASSERT(m_pRegistry != nullptr, "Initializing entity with null registry");

			MEGA_ASSERT(m_owner != nullptr, "Initializing entity without a parent");

			// Add default transform component
			m_pTransformComponent = &AddComponent<Component::Transform>();

			OnInitialize();

			if (HasComponent<Component::RigidBody>())
			{
				auto& body = GetComponent<Component::RigidBody>();
				body.pPhysicsBody->setUserPointer(this);
			}

			SetLifetimeState(eEntityState::Initialized);
		};
		void Update(const tTimestep in_dt)
		{
			MEGA_ASSERT(IsInitialized(), "Updating an entity in a state other than initialized");

			OnUpdate(in_dt);
		};
		void UpdatePost(const tTimestep in_dt)
		{
			MEGA_ASSERT(IsInitialized(), "Updating an entity in a state other than initialized");

			OnUpdatePost(in_dt);
		}

		// Ownership System
		void SetOwner(const tOwner in_owner)
		{
			const bool isRoot = in_owner == this;
			MEGA_ASSERT(m_owner == nullptr, "Setting owner on entity that already has an owner");
			//MEGA_ASSERT(in_owner->IsInitialized() || isRoot, "Setting an owner that isn't initialized"); TODO: should this be allowed?

			m_owner = in_owner;

			if (!isRoot)
			{
				m_childIndex = m_owner->AppendChild(this);
			}
		}
		// Called by SetOwner
		tChildIndex AppendChild(const tChild in_child)
		{
			const tChildIndex newIndex = (tChildIndex)m_children.size();
			in_child->m_childIndex = newIndex;
			m_children.push_back(in_child);

			return newIndex;
		}
		void RemoveChild(const tChildIndex in_childIndex)
		{
			// Swap child to be removed and back of the list, then pop
			m_children[in_childIndex] = m_children.back();
			m_children[in_childIndex]->m_childIndex = in_childIndex;
			m_children.pop_back();
		}

		// Children Entities
		tChildIndex m_childIndex = -1;
		tOwner m_owner = nullptr;
		std::vector<tChild> m_children;

		// Entity guard
		eEntityState m_state = eEntityState::Created;

		// ENTT
		// Defailt transform componentx
		Component::Transform* m_pTransformComponent = nullptr;
		entt::entity m_enttID = entt::null;
		entt::registry* m_pRegistry = nullptr;
		
		Scene* m_pScene = nullptr; // the scene this entity belongs to
		bool m_isActive = true; // Determines if entity is updated every frame / returned in Scene's Get() function
	};
} // namespace Mega