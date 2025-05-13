#pragma once

#include "OzzMesh.h"
#include "OzzUtils.h"

#include "Engine/Graphics/Objects/Model.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton_utils.h"
#include "ozz/animation/runtime/animation_utils.h"

namespace Mega
{
	using tAnimationDataIndex = int32_t;

	struct AnimatedMesh
	{
		AnimatedVertexData vertexData{};
		TextureData textureData{};
		MaterialData materialData{};

		SKINNING_MAT_INDEX_T skinningMatsIndiceStart = 0;
		uint32_t highestJointIndex = 0;
		tAnimationDataIndex meshIndex = -1; // -1 means invalid
	};
	struct AnimatedSkeleton
	{
		uint32_t jointCount = 0;
		uint32_t soaJointCount = 0;
		tAnimationDataIndex skeletonIndex = -1;
	};
	struct Animation
	{
		bool shouldLoop = true;
		float duration = 0.0f;
		uint32_t trackCount = 0;

		tAnimationDataIndex animationIndex = -1;
		ozz::sample::PlaybackController controller{};
	};
} // namespace Mega