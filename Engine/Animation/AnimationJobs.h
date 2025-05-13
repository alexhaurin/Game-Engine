#pragma once

#include "ozz/animation/runtime/ik_aim_job.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/ik_two_bone_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"

#include "Engine/Core/Core.h"
#include "Engine/Animation/AnimationObjects.h"

namespace Mega
{
	using tMesh = AnimatedMesh;
	using tAnimation = Animation;
	using tSkeleton = AnimatedSkeleton;
	using tJointName = std::string_view;
	using tAnimationName = std::string_view;

	// Forawrd declarations
	class AnimationSystem;
	struct Animation;
	namespace Component { class AnimatedModel; struct Transform; };

	// ---------- Abstract Pure Virtual Job Class ---------- //
	struct AnimationJob
	{
		friend AnimationSystem;
		virtual bool RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt) = 0;
		
		bool isValid = false;
	};

	// ---------- Simple animation playing ---------- //
	struct PlaybackJob : public AnimationJob
	{
		friend AnimationSystem;
		bool RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt) override;

		Animation* pActiveAnimation;
		const Component::Transform* pEntityTransform = nullptr;
	};

	// ---------- Blending two animations in transition ---------- //
	struct BlendJob : public AnimationJob
	{
		friend AnimationSystem;
		bool RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt) override;

		float blendFactor;
		Animation* pActiveAnimation;
		Animation* pBlendedAnimation;
		const Component::Transform* pEntityTransform = nullptr;
	};

	// ---------- Points the end joint at the target, oriented around the pole vector, used for feet planting, wall climbing, hand reaching, etc  ---------- //
	struct InverseKinematicsJob : public AnimationJob
	{
		friend AnimationSystem;
		bool RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt) override;

		tJointName startJoint = "";
		tJointName midJoint = "";
		tJointName endJoint = "";

		bool hasReached = false;
		Vec3 target = { 0, 0, 0 };
		Vec3 pole   = { 0, 1, 0 };
		const Component::Transform* pEntityTransform = nullptr;
	};

	// ---------- Orientates the ankle joint to match the normal vector. Used for feet planting for realistid standing/walking on edges ---------- //
	struct FootPlantingJob : public AnimationJob
	{
		friend AnimationSystem;
		bool RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt) override;

		Vec3 angleNormal = { 0, 0, 0 };
		Vec3 pole = { 1, 0, 0 };
		tJointName ankleJoint = "";
		const Component::Transform* pEntityTransform = nullptr;
	};

	// ---------- Attaches the barnacle's transform to the joints transform. Used for attaching a sword or other object to a hand, etc ---------- //
	struct JointAttachmentJob : public AnimationJob
	{
		friend AnimationSystem;

		bool RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt) override;

		tJointName m_attachmentJointName = "";
		Component::Transform* m_pBarnacleTransform = nullptr; // Thing getting attached
		const Component::Transform* m_pJointTransform = nullptr; // Thing the barnacle is attaching too
	};

	// ---------- Points joint in a certain direction. Used to turn the face to look at a target  ---------- //
	struct LookAtJob : public AnimationJob
	{
		friend AnimationSystem;
		bool RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt) override;
	};

}