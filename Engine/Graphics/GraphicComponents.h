#pragma once

#include "Engine/ECS/Components.h"
#include "Engine/Graphics/Objects/Model.h"
#include "Engine/Graphics/Objects/Light.h"

namespace Mega
{
	namespace Component
	{
		struct Model : public ComponentBase
		{
			Model(const VertexData& in_vertexData)
				: vertexData(in_vertexData) {};
			Model(const VertexData& in_vertexData, const TextureData& in_textureData)
				: vertexData(in_vertexData), textureData(in_textureData) {};
			Model(const VertexData& in_vertexData, const TextureData& in_textureData, const MaterialData& in_materialData)
				: vertexData(in_vertexData), textureData(in_textureData), materialData(in_materialData) {};

			VertexData vertexData;
			TextureData textureData;
			MaterialData materialData;
		};
		struct Water : public ComponentBase
		{
			Water(const VertexData& in_vertexData)
				: vertexData(in_vertexData) {}

			VertexData vertexData;
			TextureData textureData;
			MaterialData materialData = MaterialData(0.9f, 0.1f, 0.5f);
		};
		struct Light : public ComponentBase
		{
			Light(const LightData& in_lightData)
				: lightData(in_lightData) {};
			Light(const Vec3 in_pos, float in_strength)
			{
				lightData.position = in_pos;
				lightData.strength = in_strength;
			}

			LightData lightData;
		};
	} // namespace Component
} // namespace Mega