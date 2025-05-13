#pragma once

#include "ozz/base/maths/soa_float4x4.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/sampling_job.h"

#include "Engine/ECS/System.h"
#include "Engine/Core/Math/Matrix.h"
#include "Engine/Animation/AnimationJobs.h"
#include "Engine/Animation/AnimationObjects.h"

#define MAX_BLEND_LAYERS 2

// Forward Declarations
namespace Mega
{
	class Engine;
	class Scene;
}

namespace Mega
{
	class AnimationSystem : public System
	{
	public:
		friend AnimationJob;
		friend PlaybackJob;
		friend BlendJob;
		friend InverseKinematicsJob;
		friend FootPlantingJob;
		friend LookAtJob;
		friend JointAttachmentJob;
		friend Engine;

		using tDataIndex = tAnimationDataIndex;

		eMegaResult OnInitialize() override;
		eMegaResult OnDestroy() override;
		eMegaResult OnUpdate(const tTimestep in_dt, Scene* in_pScene) override;

		// Getters
		inline const std::vector<Mat4x4>& GetModelMatData() const { return m_glmModels; }

		// Loaders
		AnimatedMesh LoadAnimatedMesh(const tFilePath in_filePath);
		AnimatedSkeleton LoadAnimatedSkeleton(const tFilePath in_filePath);
		Animation LoadAnimation(const tFilePath in_filePath);

	private:
		std::vector<ozz::vector<ozz::sample::Mesh>> m_meshes;

		ozz::animation::SamplingJob::Context m_contexts[MAX_BLEND_LAYERS]; // stores 'hot keys' used during sampling
		ozz::vector<ozz::math::SoaTransform> m_locals[MAX_BLEND_LAYERS]; // local space mats for animation jobs
		ozz::vector<ozz::math::SoaTransform> m_localsBlended{};
		ozz::vector<ozz::math::Float4x4> m_skinningMats{}; // word space skinning mats in ozz format
		ozz::vector<ozz::math::Float4x4> m_models{}; // model space matrices

		std::vector<Mat4x4> m_glmModels{}; // world space matrices in GLM column major format for skinning and displaying graphics

		static const uint32_t m_maxLayersCount = MAX_BLEND_LAYERS;
		uint32_t m_maxSOAJointsCount = 0;
		uint32_t m_maxJointsCount = 0;
		uint32_t m_maxMatCount = 0;
		uint32_t m_totalMatCount = 0;
		uint32_t m_maxSkinningMats = 0;
		uint32_t m_totalSkiningMats = 0;

		// Raw Animation Data
		std::vector<ozz::animation::Skeleton> m_skeletons{};
		std::vector<ozz::animation::Animation> m_animations{};
	};
} // namespace Mega