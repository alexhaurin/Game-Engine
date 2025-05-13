#pragma once

#include <entt/entt.hpp>

#include "Engine/ECS/Entity.h"
#include "Engine/Core/Core.h"
#include "Engine/Scene/Input.h"

// Forward declarations
class Mega::Entity;

namespace Mega
{
	// TODO: Init guard
	class Scene
	{
	public:
		friend Engine;

		eMegaResult Initialize();
		eMegaResult Destroy();
		eMegaResult Update(const tTimestep in_dt);
		eMegaResult UpdatePost(const tTimestep in_dt);

		// Getters
		inline entt::registry& GetRegistry() { return m_registry; }
		inline const entt::registry& GetRegistry() const { return m_registry; }

		// All entities must have unbroken chain of ownership so the scene is
		// only in charge of creating and controlling the root entity
		template<typename tEntityType, class... Args>
		inline tEntityType* CreateRootEntity(Args&&... in_args)
		{
			static_assert(std::is_base_of<Entity, tEntityType>::value, "Root must be descendant of Entity class to be added to scene");
			MEGA_ASSERT(m_pRootEntity == nullptr, "Creating a duplicate root entity or old root entity not destroyed");

			// Create entity
			tEntityType* out_root = new tEntityType(std::forward<Args>(in_args)...);
			MEGA_ASSERT(out_root, "Root entity could not be allocated");

			entt::entity enttID = m_registry.create();
			out_root->m_enttID = enttID;
			out_root->m_pRegistry = &m_registry;
			out_root->m_pScene = this;

			// Create ownership chain
			out_root->SetOwner(out_root);
			m_pRootEntity = out_root;

			out_root->Initialize();

			return out_root;
		}

		// Returns a vector of all of a certain type of entity in the scene
		template<typename tEntityType>
		inline std::vector<tEntityType*>& GetAllOf()
		{
			MEGA_STATIC_ASSERT(std::is_base_of<Entity, tEntityType>::value, "Type must be descendant of Entity class");

			for (auto& sublist : m_pEntities)
			{
				if (Entity::TypeOf(sublist[0]) == Entity::TypeOf<tEntityType>())
				{
					return sublist;
				}
			}

			MEGA_ASSERT(false, "That entity type has not been added to scene");
			return {};
		}

	private:
		// Ownership System: Adding A Child Entity
		template<typename tEntityType, class... Args>
		inline tEntityType* AddEntity(Args&&... in_args)
		{
			Entity* in_owner = m_pRootEntity; // TODO: all entities are children of root for now

			MEGA_ASSERT(in_owner, "Creating a non-root entity with a null owner");
			static_assert(std::is_base_of<Entity, tEntityType>::value, "Type must be descendant of Entity class to be added to scene");
			//MEGA_ASSERT(in_owner->IsInitialized(), "Creating entity with an uninitialized owner"); TODO: should this be allowed?

			// Create entity
			tEntityType* out_entity = new tEntityType(std::forward<Args>(in_args)...);
			MEGA_ASSERT(out_entity, "Entity could not be allocated");

			entt::entity enttID = m_registry.create();
			out_entity->m_enttID = enttID;
			out_entity->m_pRegistry = &m_registry;
			out_entity->m_pScene = this;

			out_entity->SetOwner(in_owner);
			out_entity->Initialize();

			// Add to entity list
			for (auto& list : m_pEntities)
			{
				if (list.size() > 0)
				{
					// Fine the correct sub list to put this entity type in
					if (Entity::TypeOf(list[0]) == Entity::TypeOf(out_entity))
					{
						list.push_back(out_entity);
						return out_entity;
					};
				};
			};

			// If theres not a current list for this type of entity, make a new one
			std::vector<Entity*> newList = { out_entity };
			m_pEntities.push_back(newList);

			return out_entity;
		}

		// Scene is only creatable and destroyable by Engine
		Scene() = default;
		~Scene() = default;

		// Scene is not copyable or movable
		Scene(const Scene&) = delete;
		Scene(Scene&&) = delete;
		Scene& operator=(const Scene&) = delete;
		Scene& operator=(Scene&&) = delete;

		void DeleteEntity(Entity* in_entity);
		void DeleteAllDestroyed();

		// ENTT
		entt::registry m_registry{};
		Entity* m_pRootEntity = nullptr;
		std::vector<std::vector<Entity*>> m_pEntities{};
	};
} // namespace Mega