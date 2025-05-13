#include "AnimationComponents.h"

namespace Mega
{
	namespace Component
	{
		AnimatedModel::AnimatedModel(const tMesh& in_mesh, const tSkeleton& in_skeleton)
		{
			MEGA_ASSERT(in_skeleton.skeletonIndex >= 0, "Making animated model with invalid skeleton");
			MEGA_ASSERT(in_mesh.meshIndex >= 0, "Making animated model with invalid mesh");

			mesh = in_mesh;
			skeleton = in_skeleton;

			// Check the skeleton matches with the mesh, especially that the mesh
			// doesn't expect more joints than the skeleton has.
			if (skeleton.jointCount < mesh.highestJointIndex)
			{
				MEGA_ASSERT(false, "The provided mesh doesn't match the skeleton");
			}
		}
		AnimatedModel::~AnimatedModel()
		{
			ClearAnimationJobs();
		}

		void AnimatedModel::AddAnimation(const tFilePath in_name, tAnimation in_animation, bool in_shouldLoop)
		{
			MEGA_ASSERT(skeleton.skeletonIndex >= 0, "Animation needs a skeleton loaded first");
			MEGA_ASSERT(mesh.meshIndex >= 0, "Animation needs a mesh loaded first");

			// Skeleton and animation needs to match.
			if (skeleton.jointCount != in_animation.trackCount)
			{
				std::cout << "Skeleton Joint Count: " << skeleton.jointCount << std::endl;
				std::cout << "Animation Track Count: " << in_animation.trackCount << std::endl;
				// For some reason LoadAnimation doesn't fail if the file doesn't exist so check that if this pops
				MEGA_ASSERT(false, "Ozz skeleton joints and animation tracks don't match");
			}

			animationNameMap.insert(std::make_pair(in_name, in_animation));
			in_animation.shouldLoop = in_shouldLoop;
			in_animation.controller.set_loop(in_shouldLoop);
		}
		void AnimatedModel::Reset(const tAnimationName in_animName)
		{
			MEGA_ASSERT(IsLoaded(in_animName), "Reseting an animation that is not loaded");

			Animation* pActiveAnimation = &animationNameMap.at(in_animName);
			pActiveAnimation->controller.set_time_ratio(0);
		}
		void AnimatedModel::Play(const tAnimationName in_animName, const Transform* in_pEntityTransform)
		{
			MEGA_ASSERT(IsLoaded(in_animName), "Animation name 1 was not loaded");

			isPlaying = true;

			// Create the animation job
			PlaybackJob* pPlaybackJob = new PlaybackJob();

			pPlaybackJob->isValid = true;
			pPlaybackJob->pActiveAnimation = &animationNameMap.at(in_animName);
			pPlaybackJob->pEntityTransform = in_pEntityTransform;

			pAnimationJobs.push_back(pPlaybackJob);
		}
		void AnimatedModel::Blend(const tAnimationName in_animName1, const tAnimationName in_animName2, float in_blendFactor, const Transform* in_pEntityTransform)
		{
			MEGA_ASSERT(IsLoaded(in_animName1), "Animation name 1 was not loaded");
			MEGA_ASSERT(IsLoaded(in_animName2), "Animation name 2 was not loaded");
			MEGA_ASSERT(in_blendFactor >= 0.0 && in_blendFactor <= 1.0, "Blend factor must be between 0 and 1");

			isPlaying = true;

			// Create the animation job
			BlendJob* pBlendJob = new BlendJob();

			pBlendJob->isValid = true;
			pBlendJob->blendFactor = in_blendFactor;
			pBlendJob->pActiveAnimation = &animationNameMap.at(in_animName1);
			pBlendJob->pBlendedAnimation = &animationNameMap.at(in_animName2);
			pBlendJob->pEntityTransform = in_pEntityTransform;

			pAnimationJobs.push_back(pBlendJob);
		}
		void AnimatedModel::InverseKinematics(const tJointName in_startJoint, const tJointName in_midJoint, const tJointName in_endJoint, const Vec3& in_target, const Vec3& in_pole, const Transform* in_pEntityTransform)
		{
			InverseKinematicsJob* pIKJob = new InverseKinematicsJob();

			pIKJob->startJoint = in_startJoint;
			pIKJob->midJoint = in_midJoint;
			pIKJob->endJoint = in_endJoint;
			pIKJob->target = in_target;
			pIKJob->pole = in_pole;
			pIKJob->pEntityTransform = in_pEntityTransform;
			pIKJob->isValid = true;

			pAnimationJobs.push_back(pIKJob);
		}
		void AnimatedModel::AttachToJoint(const tJointName in_joint, Transform* in_pBarnacleTransform, const Transform* in_pJointTransform)
		{
			MEGA_ASSERT(in_pBarnacleTransform != nullptr, "Creating Joint Attachment Job with a nullptr transform");
			MEGA_ASSERT(in_pJointTransform != nullptr, "Creating Joint Attachment Job with a nullptr transform");
			JointAttachmentJob* pAttachmentJob = new JointAttachmentJob();

			pAttachmentJob->m_attachmentJointName = in_joint;
			pAttachmentJob->m_pBarnacleTransform = in_pBarnacleTransform;
			pAttachmentJob->m_pJointTransform = in_pJointTransform;
			pAttachmentJob->isValid = true;

			pAnimationJobs.push_back(pAttachmentJob);
		}
		void AnimatedModel::PlantFoot(const tJointName in_ankleJoint, const Vec3& in_angleNormal, const Vec3& in_pole, Transform* in_pEntityTransform)
		{
			MEGA_ASSERT(in_pEntityTransform != nullptr, "Creating Foot Planting Job with a nullptr transform");
			FootPlantingJob* pAttachmentJob = new FootPlantingJob();

			// For some reason the foot will not turn all the way unless these vectors are large
			pAttachmentJob->angleNormal = in_angleNormal; // * Vec3(100);
			pAttachmentJob->pole = in_pole; // *Vec3(100);
			pAttachmentJob->ankleJoint = in_ankleJoint;
			pAttachmentJob->pEntityTransform = in_pEntityTransform;
			pAttachmentJob->isValid = true;

			pAnimationJobs.push_back(pAttachmentJob);
		}
		void AnimatedModel::ClearAnimationJobs()
		{
			for (AnimationJob* pJob : pAnimationJobs)
			{
				if (pJob) { delete pJob; }
			}

			pAnimationJobs.clear();
		}
	} // namespace Component
} // namespace Mega