#pragma once

#include <array>

#include "Engine/Core/Math/Math.h"
#include "../Vulkan/VulkanDefines.h"

struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;

namespace Mega
{
	struct Vertex {
		Vertex() = default;
		Vertex(glm::vec3 in_pos)
			: pos(in_pos) {};
		glm::vec3 pos = { 0.0f, 0.0f, 0.0f };
		glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec2 texCoord = { 0.0f, 0.0f };
		glm::vec3 normal = { 0.0f, 0.0f, 1.0f };

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();

		bool operator==(const Vertex& other) const {
			return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
		}
	};

	struct GrassVertex {
		GrassVertex() = default;
		GrassVertex(glm::vec3 in_pos)
			: pos(glm::vec4(in_pos, 0.0f)) {};
		glm::vec4 pos = { 0.0f, 0.0f, 0.0f, 0.0f };
		glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec4 normal = { 0.0f, 0.0f, 1.0f, 1.0f };

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();

		bool operator==(const GrassVertex& other) const {
			return pos == other.pos && color == other.color && normal == other.normal;
		}
	};

	struct AnimatedVertex {
		glm::vec3 pos = { 0.0f, 0.0f, 0.0f };
		glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec2 texCoord = { 0.0f, 0.0f };
		glm::vec3 normal = { 0.0f, 0.0f, 1.0f };
	
		float weights[MAX_BONE_INFLUENCE] = { 0.0f };
		int32_t boneIDs[MAX_BONE_INFLUENCE] = { -1 };
	
		static VkVertexInputBindingDescription GetBindingDescription();
		static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
	
		bool operator==(const AnimatedVertex& other) const {
			return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
		}
	};
}
