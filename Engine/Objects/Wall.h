#pragma once

#include "Engine/Engine.h"

namespace Mega
{
	class Wall : public Entity
	{
	public:
		Wall() = default;
		Wall(const Vec3& in_dims)
			: m_dimensions(in_dims) {};
		Wall(const Vec3& in_pos, const Vec3& in_dims)
			: m_dimensions(in_dims), m_position(in_pos) {};
		Wall(const Vec3& in_pos, const Vec3& in_dims, const Vec3& in_rot)
			: m_dimensions(in_dims), m_position(in_pos), m_rotation(in_rot) {};
		Wall(const Vec3& in_pos, const Vec3& in_dims, const TextureData& in_texture)
			: m_dimensions(in_dims), m_position(in_pos), m_texture(in_texture) {};
		Wall(const Vec3& in_pos, const Vec3& in_dims, const Vec3& in_rot, const TextureData& in_texture)
			: m_dimensions(in_dims), m_position(in_pos), m_rotation(in_rot), m_texture(in_texture) {};

		void OnInitialize() override
		{
			SetPosition(m_position);
			SetRotation(m_rotation); // TODO: set rotation wont work with rigid body -> transform is completely cleared in physics system update

			AddComponent<Component::Model>(Mega::Engine::LoadOBJ("Assets/Models/Shapes/Cube.obj"), m_texture, MaterialData(0.1f, 0.5f, 0.5f));
			AddComponent<Component::CollisionBox>(m_dimensions);
			AddComponent<Component::RigidBody>(Component::RigidBody::eRigidBodyType::Static, 0.0f, 0.5f, 0.5f);

			SetScale(m_dimensions);
		};

		// ======== Getters/Setters ========== //
		const Vec3& GetDimensions() const { return m_dimensions; }
		Vec2 GetDimensions2D() const { return Vec2(m_dimensions.x, m_dimensions.z); } // 2D stuff just for 2d raycasting used in enemy tank AI
		const Vec3& GetPosition() const { return m_position; }
		Vec2 GetPosition2D() const { return Vec2(m_position.x, -m_position.z); }
		const Vec2& GetRotation() const { return m_rotation; }

		void SetTexture(const TextureData& in_texture)
		{
			m_texture = in_texture;
			GetComponent<Component::Model>().textureData = in_texture;
		}

	private:
		Vec3 m_dimensions = Vec3(1, 5, 1);
		Vec3 m_position = Vec3(0, 0, 0);
		Vec3 m_rotation = Vec3(0, 0, 0);

		TextureData m_texture;
	};
}