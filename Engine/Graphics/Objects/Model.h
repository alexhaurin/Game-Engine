#pragma once

#include <cstdint>

#include "Engine/Core/Math/Vector.h"
#include "Engine/Core/Math/Matrix.h"

#include "Engine/Graphics/Objects/Vertex.h"

#define TEXTURE_INDEX_T int32_t
#define SKINNING_MAT_INDEX_T int32_t
#define INDEX_TYPE uint32_t

namespace Mega
{
	struct VertexData {
		uint32_t indices[2] = { 0, 0 }; // Indices into the index buffer
		const INDEX_TYPE* pIndexData = nullptr; // Pointer to the indexBuffer that contains this VertexData's index data
		const Vertex* pVertexData = nullptr; // Same thing but for the vertex data

		uint32_t vertexCount = 0;
		Vec3 max = Vec3(0, 0, 0); // Max x, y, z coordinates
	};

	// AnimatedVertex or AnimationVertex
	struct AnimatedVertexData {
		uint32_t indices[2] = { 0, 0 }; // Indices into the index buffer
		const INDEX_TYPE* pIndexData = nullptr; // Pointer to the indexBuffer that contains this VertexData's index data
		const AnimatedVertex* pVertexData = nullptr; // Same thing but for the vertex data

		uint32_t vertexCount = 0;
		Vec3 max = Vec3(0, 0, 0); // Max x, y, z coordinates
	};

	struct TextureData {
		TEXTURE_INDEX_T index = -1; // -1 means no texture
		Vec2 dimensions = { 0, 0 };
	};

	struct MaterialData
	{
		MaterialData() {};
		MaterialData(float in_metal, float in_rough, float in_ao)
			: metallic(in_metal), roughness(in_rough), ao(in_ao) {};
		MaterialData(float in_metal, float in_rough, float in_ao, float in_ambient)
			: metallic(in_metal), roughness(in_rough), ao(in_ao), ambient(in_ambient) {};

		float metallic = 0.01f;
		float roughness = 0.5f;
		float ao = 0.7f; // Ambient occulsion
		float ambient = 0.08f;

		void SetValues(glm::vec4* in_vec) const
		{
			in_vec->x = metallic;
			in_vec->y = roughness;
			in_vec->z = ao;
			in_vec->w = ambient;
		}
	};
	struct Model
	{
		struct PushConstant
		{
			glm::mat4 transform = glm::mat4(1.0f);
			glm::vec4 materialValues;

			TEXTURE_INDEX_T textureIndex = -1; // -1 == no texture
		};
	};

	struct Water
	{
		struct PushConstant
		{
			glm::mat4 transform = glm::mat4(1.0f);
			glm::vec4 materialValues;

			TEXTURE_INDEX_T textureIndex = -1;
			int time = 0;
		};
	};

	struct AnimatedModel
	{
		struct PushConstant
		{
			glm::mat4 transform = glm::mat4(1.0f);
			glm::vec4 materialValues;
		
			TEXTURE_INDEX_T textureIndex = -1; // -1 == no texture
			SKINNING_MAT_INDEX_T skinningMatsIndiceStart = 0;
		};
	};

	struct Grass
	{
		using glScalarF = float;
		using glScalarUI = uint32_t;

		struct Instance
		{
			glm::vec4 pos;
			glm::vec4 color;
			glm::vec4 windForce;
			float rotation;
			float heightOffset;
			float p1;
			float p2;
		};

		struct Plane
		{
			Plane() = default;
			Plane(const glm::vec3& point, const glm::vec3& normal)
				: vector(glm::vec3(normalize(normal)), glm::dot(normalize(normal), point)) {};
			// xyz is the normal, w is the distance from the origin to the closest point on the plane
			glm::vec4 vector;
		};
		struct Frustum
		{
			Plane top;
			Plane bottom;
			Plane right;
			Plane left;
			Plane front;
			Plane back;
		};

		struct UBO
		{
			using glScalarF = float;
			using glScalarUI = uint32_t;

			Frustum frustum;

			glm::vec4 viewPos;
			glm::vec4 viewDir;
			glm::vec4 windSimCenter;
			glm::vec4 pad;

			float r;
			float time;
		};

		struct PushConstant
		{
			int grassType = 0;
		};
	};

	struct ShadowMap
	{
		struct PushConstant
		{
			glm::mat4 transform;
			glm::mat4 lightSpaceMat;
		};
	};

	struct ShadowMapAnimation
	{
		struct PushConstant
		{
			glm::mat4 transform;
			glm::mat4 lightSpaceMat;

			SKINNING_MAT_INDEX_T skinningMatsIndiceStart = 0;
		};
	};
}