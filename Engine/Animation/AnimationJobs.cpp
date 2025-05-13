#include "AnimationJobs.h"

#include "Engine/ECS/Components.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Animation/OzzUtils.h"
#include "Engine/Animation/AnimationSystem.h"
#include "Engine/Animation/AnimationObjects.h"
#include "Engine/Animation/AnimationHelpers.h"
#include "Engine/Animation/AnimationComponents.h"

#include "ozz/base/span.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_float4x4.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/simd_quaternion.h"

#include "ImGui/imgui.h"

float g_twist = 0.0f;
float g_soften = 1.0f;
float g_weight = 1.0f;

float g_dtMult = 1;
Mega::Animation* pLastAnimation = nullptr;
namespace Mega
{
	// Functions to handle the different animation jobs
	// used by the animation system - abstractions over the ozz sampling, blending, etc jobs
	bool PlaybackJob::RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt)
	{
		// TODO: the other IK and feet planting jobs use m_localsBlended so those wont work when using PlaybackJob instead of BlendedJob
		// so fix it so the final output locals are stored in the same place so u dont have to switch between them in the other jobs
		// TODO: a way to auto organize the playback jobs so the order called by the user doesn't matter - also optimizes it
		const ozz::animation::Skeleton& skeleton = in_pAnimationSystem->m_skeletons[in_pModel->skeleton.skeletonIndex];
		const ozz::animation::Animation& animationData = in_pAnimationSystem->m_animations[pActiveAnimation->animationIndex];

		ozz::sample::PlaybackController& controller = pActiveAnimation->controller;

		// Restart controller timer for new animations
		if (pActiveAnimation != pLastAnimation) // TODO better solution
		{
			controller.set_time_ratio(0);
			pLastAnimation = pActiveAnimation;
		}

		float timeRatio = controller.time_ratio();
		ImGui::DragFloat("Animation Runtime", &timeRatio);
		
		ImGui::DragFloat("Animation Speed", &g_dtMult, 0.01f);
		controller.set_loop(pActiveAnimation->shouldLoop);
		// Our tTimstep is a float containing number of milliseconds since last frame,
		// but the controller update function takes a float containing the number (or fraction)
		// of seconds since last frame, so in_dt needs a quick conversion between the two
		controller.Update(animationData, (in_dt * g_dtMult) / 1000);

		ozz::animation::SamplingJob sampling_job;
		sampling_job.animation = &animationData;
		sampling_job.context = &in_pAnimationSystem->m_contexts[0];
		sampling_job.ratio = controller.time_ratio();
		sampling_job.output = make_span(in_pAnimationSystem->m_locals[0]);
		if (!sampling_job.Run()) {
			std::cout << "Sampling job failed" << std::endl;
			return false;
		}
		
		// Converts from local space to model space matrices.
		const ozz::math::Float4x4 worldSpaceTransform = GLMToOzz(pEntityTransform->GetTransform());

		ozz::animation::LocalToModelJob ltm_job;
		ltm_job.root = &worldSpaceTransform;
		ltm_job.skeleton = &skeleton;
		ltm_job.input = make_span(in_pAnimationSystem->m_locals[0]);
		ltm_job.output = make_span(in_pAnimationSystem->m_models);
		if (!ltm_job.Run()) {
			std::cout << "Local to Model job failed" << std::endl;
			return false;
		}

		return true;
	}

	bool BlendJob::RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt)
	{
		const ozz::animation::Skeleton& skeleton = in_pAnimationSystem->m_skeletons[in_pModel->skeleton.skeletonIndex];
		const ozz::animation::Animation& activeAnimationData = in_pAnimationSystem->m_animations[pActiveAnimation->animationIndex];
		const ozz::animation::Animation& blendedAnimationData = in_pAnimationSystem->m_animations[pBlendedAnimation->animationIndex];

		ozz::sample::PlaybackController& activeController = pActiveAnimation->controller;
		ozz::sample::PlaybackController& blendedController = pBlendedAnimation->controller;

		activeController.set_loop(pActiveAnimation->shouldLoop);
		blendedController.set_loop(pActiveAnimation->shouldLoop);

		// Restart controller timer for new animations
		if (pActiveAnimation != pLastAnimation) // TODO better solution
		{
			blendedController.set_time_ratio(0);
			pLastAnimation = pActiveAnimation;
		}

		const uint32_t layersCount = 2;
		float weights[layersCount] = { 0 };
		MEGA_ASSERT(layersCount <= MAX_BLEND_LAYERS, "Too many blended layers");

		// Computes weight parameters for all samplers.
		const float kNumIntervals = layersCount - 1;
		const float kInterval = 1.f / kNumIntervals;
		for (int i = 0; i < layersCount; ++i) {
			const float med = i * kInterval;
			const float x = blendFactor - med;
			const float y = ((x < 0.f ? x : -x) + kInterval) * kNumIntervals;
			weights[i] = ozz::math::Max(0.f, y);
		}

		// Interpolates animation durations using their respective weights, to
		// find the loop cycle duration that matches blend_ratio_.
		const float loop_duration = pActiveAnimation->duration * weights[0] + pBlendedAnimation->duration * weights[1];

		// Finally finds the speed coefficient for all samplers.
		const float inv_loop_duration = 1.f / loop_duration;
		//activeController.set_playback_speed(pActiveAnimation->duration * inv_loop_duration);
		//blendedController.set_playback_speed(pBlendedAnimation->duration * inv_loop_duration);

		activeController.Update(activeAnimationData, in_dt / 1000);
		blendedController.Update(blendedAnimationData, in_dt / 1000);

		// Samples optimized animation at t = animation_time_.
		ozz::animation::SamplingJob sampling_job1;
		sampling_job1.animation = &activeAnimationData;
		sampling_job1.context = &in_pAnimationSystem->m_contexts[0];
		sampling_job1.ratio = activeController.time_ratio();
		sampling_job1.output = make_span(in_pAnimationSystem->m_locals[0]);
		if (!sampling_job1.Run()) {
			std::cout << "Sampling job failed" << std::endl;
			return false;
		}

		// Samples optimized animation at t = animation_time_.
		ozz::animation::SamplingJob sampling_job2;
		sampling_job2.animation = &blendedAnimationData;
		sampling_job2.context = &in_pAnimationSystem->m_contexts[1];
		sampling_job2.ratio = blendedController.time_ratio();
		sampling_job2.output = make_span(in_pAnimationSystem->m_locals[1]);
		if (!sampling_job2.Run()) {
			std::cout << "Sampling job failed" << std::endl;
			return false;
		}

		// Blends animations.
		// Blends the local spaces transforms computed by sampling all animations
		// (1st stage just above), and outputs the result to the local space
		// transform buffer blended_locals_

		// Prepares blending layers.
		ozz::animation::BlendingJob::Layer layers[MAX_BLEND_LAYERS];
		for (int i = 0; i < layersCount; i++)
		{
			layers[i].transform = make_span(in_pAnimationSystem->m_locals[i]);
			layers[i].weight = weights[i];
		}

		// Setups blending job.
		ozz::animation::BlendingJob blend_job;
		blend_job.threshold = 0.1f;
		blend_job.layers = layers;
		blend_job.rest_pose = skeleton.joint_rest_poses();
		blend_job.output = make_span(in_pAnimationSystem->m_localsBlended);

		// Blends.
		if (!blend_job.Run()) {
			std::cout << "Blending job failed" << std::endl;
			return false;
		}

		// Converts from local space to model space matrices.
		const ozz::math::Float4x4 worldSpaceTransform = GLMToOzz(pEntityTransform->GetTransform());

		ozz::animation::LocalToModelJob ltm_job;
		ltm_job.skeleton = &skeleton;
		ltm_job.root = &worldSpaceTransform;
		ltm_job.input = make_span(in_pAnimationSystem->m_localsBlended);
		ltm_job.output = make_span(in_pAnimationSystem->m_models);
		if (!ltm_job.Run()) {
			std::cout << "Local to Model job failed" << std::endl;
			return false;
		}

		return true;
	}

	bool InverseKinematicsJob::RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt)
	{
		const ozz::animation::Skeleton& skeleton      = in_pAnimationSystem->m_skeletons[in_pModel->skeleton.skeletonIndex];
		const ozz::span<const char* const>& jointNames = skeleton.joint_names();

		ozz::vector<ozz::math::SoaTransform>& localMats = in_pAnimationSystem->m_localsBlended;// [0] ;
		ozz::vector<ozz::math::Float4x4>& modelMats = in_pAnimationSystem->m_models;

		// Find the model matrixes for each joint we're modifying
		int32_t startJointModelIndex = -1;
		int32_t midJointModelIndex   = -1;
		int32_t endJointModelIndex   = -1;

		int32_t index = 0;
		for (const char* name : jointNames)
		{
			if (startJoint.compare(name) == 0) { startJointModelIndex = index; }
			if (midJoint.compare(name)   == 0) { midJointModelIndex   = index; }
			if (endJoint.compare(name)   == 0) { endJointModelIndex   = index; }

			index++;
		}

		MEGA_ASSERT(startJointModelIndex != -1, "IK Job Start Joint Not Found!!!");
		MEGA_ASSERT(midJointModelIndex   != -1, "IK Job Middle Joint Not Found!!!");
		MEGA_ASSERT(endJointModelIndex   != -1, "IK Job End Joint Not Found!!!");

		ozz::math::Float4x4& startJointModel = modelMats[startJointModelIndex];
		ozz::math::Float4x4& midJointModel   = modelMats[midJointModelIndex];
		ozz::math::Float4x4& endJointModel   = modelMats[endJointModelIndex];

		//ImGui::DragFloat4("IK Pole", &pole.x, 0.01);
		//ImGui::DragFloat("Weight", &g_weight, 0.001);
		//ImGui::DragFloat("Soften", &g_soften, 0.001);
		//ImGui::DragFloat("Twist", &g_twist, 0.01);
		// Run Job
		// Target and pole should be in model-space, so they must be converted from
		// world-space using character inverse root matrix.
		// IK jobs must support non invertible matrices (like 0 scale matrices).

		//ozz::math::Float4x4 GetRootTransform()
		//{
		//	return ozz::math::Float4x4::Translation(
		//		ozz::math::simd_float4::Load3PtrU(&root_translation_.x)) *
		//		ozz::math::Float4x4::FromEuler(
		//			ozz::math::simd_float4::Load3PtrU(&root_euler_.x)) *
		//		ozz::math::Float4x4::Scaling(
		//			ozz::math::simd_float4::Load1(root_scale_));
		//}

		ozz::math::SimdInt4 invertible;
		const ozz::math::Float4x4 invertRoot = Invert(GLMToOzz(pEntityTransform->GetTransform()), &invertible);
		
		const ozz::math::SimdFloat4 target_ms = ozz::math::simd_float4::Load3PtrU(&target.x); // TransformPoint(invertRoot, ozz::math::simd_float4::Load3PtrU(&target.x));
		const ozz::math::SimdFloat4 pole_vector_ms = ozz::math::simd_float4::Load3PtrU(&pole.x); // TransformVector(invertRoot, ozz::math::simd_float4::Load3PtrU(&pole.x));

		// Reset locals to skeleton rest pose if option is true.
		// This allows to always start IK from a fix position (required to test
		// weighting), or do IK from the latest computed pose
		for (size_t i = 0; i < localMats.size(); ++i)
		{
			//localMats[i] = skeleton.joint_rest_poses()[i]; // TODO: what does removing this do?
		}

		// Setup IK job.
		ozz::animation::IKTwoBoneJob ik_job;
		ik_job.target = target_ms;
		ik_job.pole_vector = pole_vector_ms;
		ik_job.mid_axis = -ozz::math::simd_float4::z_axis();  // Middle joint rotation axis is
															 // fixed, and depends on skeleton rig.

		ik_job.weight = g_weight;
		ik_job.soften = g_soften;
		ik_job.twist_angle = g_twist;

		// Provides start, middle and end joints model space matrices.
		ik_job.start_joint = &startJointModel;
		ik_job.mid_joint = &midJointModel;
		ik_job.end_joint = &endJointModel;

		// Setup output pointers.
		ozz::math::SimdQuaternion start_correction;
		ik_job.start_joint_correction = &start_correction;
		ozz::math::SimdQuaternion mid_correction;
		ik_job.mid_joint_correction = &mid_correction;
		ik_job.reached = &hasReached;

		in_pModel->ikHasReached |= hasReached;

		if (!ik_job.Run()) {
			return false;
		}

		// Apply IK quaternions to their respective local-space transforms.
		ozz::sample::MultiplySoATransformQuaternion(startJointModelIndex, start_correction, make_span(localMats));
		ozz::sample::MultiplySoATransformQuaternion(midJointModelIndex, mid_correction, make_span(localMats));
		
		// Updates model-space matrices now IK has been applied to local transforms.
		// All the ancestors of the start of the IK chain must be computed.
		const ozz::math::Float4x4 worldSpaceTransform = GLMToOzz(pEntityTransform->GetTransform());

		ozz::animation::LocalToModelJob ltm_job;
		//ltm_job.root = &worldSpaceTransform;
		ltm_job.skeleton = &skeleton;
		ltm_job.input = make_span(localMats);
		ltm_job.output = make_span(modelMats);
		ltm_job.from = startJointModelIndex; // Only have to edit the matrixes past the firstjoint changed
		ltm_job.to = ozz::animation::Skeleton::kMaxJoints;

		if (!ltm_job.Run()) {
			std::cout << "Local to Model job failed" << std::endl;
			return false;
		}

		return true;
	}

	bool FootPlantingJob::RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt)
	{
		const ozz::animation::Skeleton& skeleton = in_pAnimationSystem->m_skeletons[in_pModel->skeleton.skeletonIndex];
		const ozz::span<const char* const>& jointNames = skeleton.joint_names();
		ozz::vector<ozz::math::Float4x4>& modelMatrices = in_pAnimationSystem->m_models;

		ozz::vector<ozz::math::SoaTransform>& localMats = in_pAnimationSystem->m_localsBlended;
		ozz::vector<ozz::math::Float4x4>& modelMats = in_pAnimationSystem->m_models;

		ozz::math::SimdInt4 invertible;
		const ozz::math::Float4x4& root = GLMToOzz(pEntityTransform->GetTransform());
		const ozz::math::Float4x4& inverseRoot = Invert(root, &invertible);

		// Find the model matrixes for each joint we're modifying
		int32_t ModelIndex = -1;
		int32_t ankleJointModelIndex = -1;

		int32_t index = 0;
		for (const char* name : jointNames)
		{
			if (ankleJoint.compare(name) == 0) { ankleJointModelIndex = index; }
			index++;
		}

		MEGA_ASSERT(ankleJointModelIndex != -1, "Foot Planting Job Ankle Joint Not Found!!!");
		ozz::math::Float4x4& ankleJointModel = modelMats[ankleJointModelIndex];

		// Set up job
		// Target position and pole vectors must be in model space.
		// NOTE: For some reason with the foot planting job and the inverse kinematics jobs inverting the root led to
		// slightly correct but whacky results especially far away from zero. {assing in the entity's forward vector
		// as the pole (plus some sort of offset sometimes) seems to work as an alternative. Here we also have to rotate the foot planting normal
		// by the opposite of the entity's rotation.
		const auto yaw = -1 * pEntityTransform->GetRotation().y;
		glm::mat4 rotMat = glm::rotate(glm::mat4(1.0), yaw, glm::vec3(0, 1, 0));
		angleNormal = glm::vec3(rotMat * glm::vec4(angleNormal, 1));

		const ozz::math::SimdFloat4 target_ms = TransformPoint(root, ozz::math::simd_float4::Load3PtrU(&angleNormal.x));
		const ozz::math::SimdFloat4 pole_vector_ms = ozz::math::simd_float4::Load3PtrU(&pole.x);   // TransformVector(inverseRoot, ozz::math::simd_float4::Load3PtrU(&pole.x));

		// Run job
		ozz::animation::IKAimJob ik_job;
		ik_job.forward = -ozz::math::simd_float4::x_axis();
		ik_job.up = ozz::math::simd_float4::y_axis();
		ik_job.target = target_ms; // Model space targetted direction (floor normal in this case).
		ik_job.pole_vector = pole_vector_ms;
		ik_job.joint = &ankleJointModel;
		ik_job.weight = g_weight;
		ik_job.twist_angle = g_twist;

		ozz::math::SimdQuaternion correction;
		ik_job.joint_correction = &correction;
		if (!ik_job.Run()) {
			std::cout << "Foot planting job failed" << std::endl;
			return false;
		}

		// Apply IK quaternions to their respective local-space transforms.
		// Model-space transformations needs to be updated after a call to this function.
		ozz::sample::MultiplySoATransformQuaternion(ankleJointModelIndex, correction, make_span(localMats));

		// Updates model-space matrices now IK has been applied to local transforms.
		// All the ancestors of the start of the IK chain must be computed.
		// TODO: multiple jobs will be updating LTM multiple times - remove redunt work when doing a lot of jobs like playing animation with IK, etc
		ozz::animation::LocalToModelJob ltm_job;
		ltm_job.skeleton = &skeleton;
		ltm_job.input = make_span(localMats);
		ltm_job.output = make_span(modelMats);
		ltm_job.from = ankleJointModelIndex; // Only have to update the matrixes past the ankle joint
		ltm_job.to = ozz::animation::Skeleton::kMaxJoints;

		if (!ltm_job.Run()) {
			std::cout << "Local to Model job failed" << std::endl;
			return false;
		}

		return true;
	}

	bool JointAttachmentJob::RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt)
	{
		const ozz::animation::Skeleton& skeleton        = in_pAnimationSystem->m_skeletons[in_pModel->skeleton.skeletonIndex];
		const ozz::span<const char* const>& jointNames   = skeleton.joint_names();
		ozz::vector<ozz::math::Float4x4>& modelMatrices = in_pAnimationSystem->m_models;

		// Find the model matrixes for each joint we're modifying
		int32_t jointModelIndex = ozz::animation::FindJoint(skeleton, m_attachmentJointName.data());
		MEGA_ASSERT(jointModelIndex >= 0, "Could not find joint for attachment job");

		const ozz::math::Float4x4& jointModelMatrix = modelMatrices[jointModelIndex]; // TODO: this only works with one animated model
																					  // (modelMatrixes is global but we're using the local index stored and given by
																					  // the animated model component)

		glm::mat4x4 glmTransform = OzzToGLM(jointModelMatrix);
		m_pBarnacleTransform->SetTransform(glm::scale(glmTransform, glm::vec3(0.01f))); // TODO: fix scaling problems

		return true;
	}
	
	bool LookAtJob::RunJob(AnimationSystem* in_pAnimationSystem, Component::AnimatedModel* in_pModel, const tTimestep in_dt)
	{
		assert(false);

		return false;
	}
}