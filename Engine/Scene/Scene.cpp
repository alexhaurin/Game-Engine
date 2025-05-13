#include "Scene.h"

#include <new>

#include "Engine/ECS/ECS.h"
#include "Engine/Core/Core.h"

namespace Mega
{
	eMegaResult Scene::Initialize()
	{
		// ENTT
		entt::entity entity = m_registry.create();

		return eMegaResult::SUCCESS;
	}
	eMegaResult Scene::Destroy()
	{
		// Destroy and delete all entities
		if (m_pRootEntity != nullptr)
		{
			m_pRootEntity->Destroy();
		}
		
		DeleteAllDestroyed();
		delete m_pRootEntity; // Root entity not part of the entity list

		m_registry.clear();
		m_pEntities.clear();

		return eMegaResult::SUCCESS;
	}

	eMegaResult Scene::Update(const tTimestep in_dt)
	{
		// Update all entities so long as they are marked active
		if (m_pRootEntity) { m_pRootEntity->Update(in_dt); }
		for (std::vector<Entity*>& sublist : m_pEntities) // TODO use one list not sublists
		{
			for (Entity* pEntity : sublist)
			{
				if (pEntity->IsActive()) { pEntity->Update(in_dt); }
			}
		}

		return eMegaResult::SUCCESS;
	}
	eMegaResult Scene::UpdatePost(const tTimestep in_dt)
	{
		if (m_pRootEntity) { m_pRootEntity->UpdatePost(in_dt); }
		for (std::vector<Entity*>& sublist : m_pEntities)
		{
			for (Entity* pEntity : sublist)
			{
				if (pEntity->IsActive()) { pEntity->UpdatePost(in_dt); }
			}
		}

		// Remove all entities that have been marked destroyed
		DeleteAllDestroyed();

		return eMegaResult::SUCCESS;
	}

	void Scene::DeleteEntity(Entity* in_pEntity)
	{
		m_registry.destroy(in_pEntity->m_enttID);

		delete in_pEntity;
	}
	void Scene::DeleteAllDestroyed()
	{
		for (std::vector<Entity*>& sublist : m_pEntities)
		{
			sublist.erase(
				std::remove_if(
					sublist.begin(), sublist.end(),
					[this](Entity* e)
					{
						if (e->IsDestroyed())
						{
							DeleteEntity(e);
							return true;
						}
						return false;
					}
				),
				sublist.end()
			);
		}
	}
};