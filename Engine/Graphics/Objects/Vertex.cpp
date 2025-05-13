#include "Vertex.h"

#include "Engine/Graphics/Vulkan/VulkanInclude.h"

namespace Mega
{
	// ======================== VERTEX ====================== //
	VkVertexInputBindingDescription Vertex::GetBindingDescription()
	{
		VkVertexInputBindingDescription out_bindingDescription{};
		out_bindingDescription.binding = 0;
		out_bindingDescription.stride = sizeof(Vertex);
		out_bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return out_bindingDescription;
	}

	std::vector<VkVertexInputAttributeDescription> Vertex::GetAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> out_attributeDescriptions{};
		out_attributeDescriptions.resize(4);

		out_attributeDescriptions[0].binding = 0;
		out_attributeDescriptions[0].location = 0;
		out_attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		out_attributeDescriptions[0].offset = offsetof(Vertex, pos);

		out_attributeDescriptions[1].binding = 0;
		out_attributeDescriptions[1].location = 1;
		out_attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT; // I CHANGED THIS
		out_attributeDescriptions[1].offset = offsetof(Vertex, color);

		out_attributeDescriptions[2].binding = 0;
		out_attributeDescriptions[2].location = 2;
		out_attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		out_attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		out_attributeDescriptions[3].binding = 0;
		out_attributeDescriptions[3].location = 3;
		out_attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		out_attributeDescriptions[3].offset = offsetof(Vertex, normal);

		return out_attributeDescriptions;
	}

	// ======================== GRASS VERTEX ====================== //
	VkVertexInputBindingDescription GrassVertex::GetBindingDescription()
	{
		VkVertexInputBindingDescription out_bindingDescription{};
		out_bindingDescription.binding = 0;
		out_bindingDescription.stride = sizeof(GrassVertex);
		out_bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return out_bindingDescription;
	}

	std::vector<VkVertexInputAttributeDescription> GrassVertex::GetAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> out_attributeDescriptions{};
		out_attributeDescriptions.resize(3);

		out_attributeDescriptions[0].binding = 0;
		out_attributeDescriptions[0].location = 0;
		out_attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		out_attributeDescriptions[0].offset = offsetof(GrassVertex, pos);

		out_attributeDescriptions[1].binding = 0;
		out_attributeDescriptions[1].location = 1;
		out_attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		out_attributeDescriptions[1].offset = offsetof(GrassVertex, color);

		out_attributeDescriptions[2].binding = 0;
		out_attributeDescriptions[2].location = 2;
		out_attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		out_attributeDescriptions[2].offset = offsetof(GrassVertex, normal);

		return out_attributeDescriptions;
	}

	// ===================== ANIMATED VERTEX ====================== //
	VkVertexInputBindingDescription AnimatedVertex::GetBindingDescription()
	{
		VkVertexInputBindingDescription out_bindingDescription{};
		out_bindingDescription.binding = 0;
		out_bindingDescription.stride = sizeof(AnimatedVertex);
		out_bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	
		return out_bindingDescription;
	}

	std::vector<VkVertexInputAttributeDescription> AnimatedVertex::GetAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> out_attributeDescriptions{};
		out_attributeDescriptions.resize(8);
	
		out_attributeDescriptions[0].binding = 0;
		out_attributeDescriptions[0].location = 0;
		out_attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		out_attributeDescriptions[0].offset = offsetof(AnimatedVertex, pos);

		out_attributeDescriptions[1].binding = 0;
		out_attributeDescriptions[1].location = 1;
		out_attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		out_attributeDescriptions[1].offset = offsetof(AnimatedVertex, color);
	
		out_attributeDescriptions[2].binding = 0;
		out_attributeDescriptions[2].location = 2;
		out_attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		out_attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
	
		out_attributeDescriptions[3].binding = 0;
		out_attributeDescriptions[3].location = 3;
		out_attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		out_attributeDescriptions[3].offset = offsetof(Vertex, normal);

		out_attributeDescriptions[4].binding = 0;
		out_attributeDescriptions[4].location = 4;
		out_attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		out_attributeDescriptions[4].offset = offsetof(AnimatedVertex, weights);

		out_attributeDescriptions[5].binding = 0;
		out_attributeDescriptions[5].location = 5;
		out_attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		out_attributeDescriptions[5].offset = offsetof(AnimatedVertex, weights) + sizeof(float) * 4;
		
		out_attributeDescriptions[6].binding = 0;
		out_attributeDescriptions[6].location = 6;
		out_attributeDescriptions[6].format = VK_FORMAT_R32G32B32A32_SINT;
		out_attributeDescriptions[6].offset = offsetof(AnimatedVertex, boneIDs);

		out_attributeDescriptions[7].binding = 0;
		out_attributeDescriptions[7].location = 7;
		out_attributeDescriptions[7].format = VK_FORMAT_R32G32B32A32_SINT;
		out_attributeDescriptions[7].offset = offsetof(AnimatedVertex, boneIDs) + sizeof(int) * 4;
	
		return out_attributeDescriptions;
	}
}