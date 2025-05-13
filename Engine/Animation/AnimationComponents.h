#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/ECS/Components.h"
#include "Engine/Animation/AnimationJobs.h"
#include "Engine/Animation/AnimationObjects.h"

namespace Mega
{
	namespace Component
	{
		class AnimatedModel : public ComponentBase
		{
		public:
			// ---------------- Set Up Functions --------------- //
			AnimatedModel(const tMesh& in_mesh, const tSkeleton& in_skeleton);
			~AnimatedModel();

			void AddAnimation(const tFilePath in_path, tAnimation in_animation, bool in_shouldLoop = true);
			void ClearAnimationJobs(); // Clears animation job vector and deletes jobs
			inline bool IsLoaded(const tAnimationName in_name) const { return animationNameMap.contains(in_name); }

			// ---------------- Animation Playback Helpers Functions --------------- //
			inline void Pause() { isPlaying = false; }
			inline void Resume() { isPlaying = true; }
			inline bool IsPlaying() const { return isPlaying; }

			// ---------------- Animation Job Functions --------------- //
			void Reset(const tAnimationName in_animName);
			void Play(const tAnimationName in_animName, const Transform* in_pEntityTransform);
			void Blend(const tAnimationName in_animName1, const tAnimationName in_animName2, float in_blendFactor, const Transform* in_pEntityTransform);
			void InverseKinematics(const tJointName in_startJoint, const tJointName in_midJoint, tJointName in_endJoint, const Vec3& in_target, const Vec3& in_pole, const Transform* in_pEntityTransform);
			void AttachToJoint(const tJointName in_joint, Transform* in_pBarnacleTransform, const Transform* in_pJointTransform);
			void PlantFoot(const tJointName in_ankleJoint, const Vec3& in_angleNormal, const Vec3& in_pole, Transform* in_pEntityTransform);

			// ---------------- Member Variables  --------------- //
			std::vector<AnimationJob*> pAnimationJobs;

			bool ikHasReached = false;
			bool isPlaying = true;
			tMesh mesh{};
			tSkeleton skeleton{};
			std::unordered_map<tAnimationName, tAnimation> animationNameMap{};
		};
	} // namespace Component
} // namespace Mega