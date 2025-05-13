#include "AnimationSystem.h"

#include <cmath>
#include <algorithm>
#include <filesystem>

#include "Engine/Engine.h"
#include "Engine/Core/Debug.h"
#include "Engine/Scene/Scene.h"
#include "Engine/ECS/Components.h"
#include "Engine/Animation/AnimationHelpers.h"
#include "Engine/Animation/AnimationObjects.h"

// For loading
#include "ozz/base/log.h"
#include "ozz/base/span.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/archive.h"

namespace Mega
{
	// =============== Base System Class Functions ============= //
	eMegaResult AnimationSystem::OnInitialize()
	{
		return eMegaResult::SUCCESS;
	}
	eMegaResult AnimationSystem::OnDestroy()
	{
		//for (auto& vector : m_locals)   { ozz::memory::default_allocator()->Deallocate((void*)&vector); }
		//for (auto& vector : m_contexts) { ozz::memory::default_allocator()->Deallocate((void*)&vector); }
		//ozz::memory::default_allocator()->Deallocate((void*)&m_models);

		//ozz::memory::default_allocator()->Deallocate((void*)&m_skeletons[0]);

		//ozz::memory::default_allocator()->Deallocate((void*)&m_skinningMats);
		//ozz::memory::default_allocator()->Deallocate((void*)&m_meshes);

		return eMegaResult::SUCCESS;
	}
	eMegaResult AnimationSystem::OnUpdate(const tTimestep in_dt, Scene* in_pScene)
	{
		auto view = in_pScene->GetRegistry().view<Component::AnimatedModel>();
		for (auto [entity, model] : view.each())
		{
			if (!model.IsPlaying()) { continue; }

			ImGui::Text("Playing Animation");

			const ozz::vector<ozz::sample::Mesh>& meshes = m_meshes[model.mesh.meshIndex];
			const ozz::animation::Skeleton& skeleton = m_skeletons[model.skeleton.skeletonIndex];

			// Run Animation Jobs
			for (AnimationJob* pJob : model.pAnimationJobs)
			{
				if (pJob->isValid)
				{
					bool result = pJob->RunJob(this, &model, in_dt);
					MEGA_ASSERT(result, "Animation job did not return success");
				}
			}

			// Builds skinning matrices, based on the output of the animation stage.
			// The mesh might not use (aka be skinned by) all skeleton joints. We
			// use the joint remapping table (available from the mesh object) to
			// reorder model-space matrices and build skinning ones.
			size_t offset = 0;
			for (const ozz::sample::Mesh& m : meshes) {
				for (size_t i = 0; i < m.joint_remaps.size(); ++i) {
					m_skinningMats[i + offset] = m_models[m.joint_remaps[i]] * m.inverse_bind_poses[i];
				}
				offset += m.joint_remaps.size();
			}

			for (size_t i = model.mesh.skinningMatsIndiceStart; i < offset; i++)
			{
				auto& m = m_skinningMats[i];
				m_glmModels[i] = OzzToGLM(m);
			}

			// Clear the animation jobs
			model.ClearAnimationJobs();
		}

		return eMegaResult::SUCCESS;
	}

	// ============== Loaders =========================== //
	AnimatedMesh AnimationSystem::LoadAnimatedMesh(const tFilePath in_filePath)
	{
		AnimatedMesh out_mesh;

		tAnimationDataIndex meshIndex = (tAnimationDataIndex)m_meshes.size();
		m_meshes.resize(meshIndex + 1);
		if (!ozz::sample::LoadMeshes(in_filePath.data(), &m_meshes[meshIndex])) {
			std::cout << "Couldn't Load Meshes" << std::endl;
			return out_mesh;
		}

		// Computes the number of skinning matrices required to skin all meshes.
		// A mesh is skinned by only a subset of joints, so the number of skinning
		// matrices might be less that the number of skeleton joints.
		// Mesh::joint_remaps is used to know how to order skinning matrices. So
		// the number of matrices required is the size of joint_remaps.
		uint32_t numSkinningMats = 0;
		for (const ozz::sample::Mesh& mesh : m_meshes[meshIndex]) {
			numSkinningMats += (uint32_t)mesh.joint_remaps.size();
		}
		m_maxSkinningMats = std::max(m_maxSkinningMats, numSkinningMats);
		m_totalSkiningMats += m_maxSkinningMats;

		// Allocates skinning matrices.
		m_skinningMats.resize(m_maxSkinningMats);
		m_glmModels.resize(m_glmModels.size() + m_maxSkinningMats);

		// Get highest joint index
		uint32_t highestJointIndex = 0;
		for (const ozz::sample::Mesh& mesh : m_meshes[meshIndex]) {
			highestJointIndex = std::max(highestJointIndex, (uint32_t)mesh.highest_joint_index());
		}

		out_mesh.highestJointIndex = highestJointIndex;
		out_mesh.meshIndex = meshIndex;
		return out_mesh;
	}
	AnimatedSkeleton AnimationSystem::LoadAnimatedSkeleton(const tFilePath in_filePath)
	{
		AnimatedSkeleton out_skeleton;

		// Add raw skeleton data to our systems array
		const tAnimationDataIndex skeletonIndex = (tAnimationDataIndex)m_skeletons.size();
		m_skeletons.resize(skeletonIndex + 1);
		if (!ozz::sample::LoadSkeleton(in_filePath.data(), &m_skeletons[skeletonIndex])) {
			std::cout << "Couldn't Load Skeleton" << std::endl;
			return out_skeleton;
		}

		const ozz::animation::Skeleton& skeletonData = m_skeletons[skeletonIndex];

		// Allocates runtime buffers using data we now have
		m_maxSOAJointsCount = std::max(m_maxSOAJointsCount, (uint32_t)skeletonData.num_soa_joints());
		m_maxJointsCount = std::max(m_maxJointsCount, (uint32_t)skeletonData.num_joints());
		for (uint32_t i = 0; i < m_maxLayersCount; i++)
		{
			m_locals[i].resize(m_maxSOAJointsCount);
			m_contexts[i].Resize(m_maxJointsCount);
		}
		m_localsBlended.resize(m_maxSOAJointsCount);
		m_models.resize(m_maxJointsCount);

		out_skeleton.skeletonIndex = skeletonIndex;
		out_skeleton.jointCount = skeletonData.num_joints();
		out_skeleton.soaJointCount = skeletonData.num_soa_joints();

		//for (const char* name : skeletonData.joint_names())
		//{
		//	std::cout << name << std::endl;
		//}

		return out_skeleton;
	}

	Animation AnimationSystem::LoadAnimation(const tFilePath in_filePath)
	{
		Animation out_animation;

		tAnimationDataIndex animationIndex = (tAnimationDataIndex)m_animations.size();
		m_animations.resize(m_animations.size() + 1);

		if (!ozz::sample::LoadAnimation(in_filePath.data(), &m_animations[animationIndex])) {
			std::cout << "Couldn't Load Animation" << std::endl;
			return out_animation;
		}

		out_animation.animationIndex = animationIndex;
		out_animation.trackCount = m_animations[animationIndex].num_tracks();
		out_animation.duration = m_animations[animationIndex].duration();
		return out_animation;
	}

	// ===================== Private Helpers ========================== //
	//ozz::animation::Skeleton AnimationSystem::LoadSkeleton(const char* in_filename)
	//{
	//	MEGA_ASSERT(IsInitialized(), "Animation system is not initialized");
	//
	//	// Open file
	//	ozz::io::File file(in_filename, "rb");
	//	if (!file.opened()) {
	//		std::cout << "Cannot open file " << in_filename << "." << std::endl;
	//	}
	//
	//	// Now we actually need to read
	//	// "The first step is to instantiate an read-capable (ozz::io::IArchive)
	//	// archive object, in opposition to write-capable (ozz::io::OArchive)"
	//	ozz::io::IArchive archive(&file);
	//
	//	// "Before actually reading the object from the file, we need to test that
	//	// the archive (at current seek position) contains the object type we
	//	// expect.
	//	// Archives uses a tagging system that allows to mark and detect the type of
	//	// the next object to deserialize. Here we expect a skeleton, so we test for
	//	// a skeleton tag.
	//	// Tagging is not mandatory for all object types. It's usually only used for
	//	// high level object types (skeletons, animations...), but not low level
	//	// ones (math, native types...)."
	//
	//	if (!archive.TestTag<ozz::animation::Skeleton>()) {
	//		std::cout << "Archive doesn't contain the expected object type." << std::endl;
	//	}
	//
	//	// Now the tag has been validated, the object can be read.
	//	// IArchive uses >> operator to read from the archive to the object.
	//	// Only objects that implement archive specifications can be used there,
	//	// along with all native types. Note that pointers aren't supported.
	//	ozz::animation::Skeleton out_skeleton;
	//		archive >> out_skeleton;
	//	std::cout << "Loaded skeleton" << std::endl;
	//	return out_skeleton;
	//}
	//ozz::animation::Animation AnimationSystem::LoadAnimation(const char* in_filename)
	//{
	//	MEGA_ASSERT(IsInitialized(), "Animation system is not initialized");
	//
	//	// Open file
	//	ozz::io::File file(in_filename, "rb");
	//	if (!file.opened()) {
	//		std::cout << "Cannot open file " << in_filename << "." << std::endl;
	//	}
	//
	//	ozz::io::IArchive archive(&file);
	//
	//	if (!archive.TestTag<ozz::animation::Animation>()) {
	//		std::cout << "Archive doesn't contain the expected object type." << std::endl;
	//	}
	//
	//	ozz::animation::Animation out_animation;
	//	archive >> out_animation;
	//	std::cout << "Loaded animation" << std::endl;
	//	return out_animation;
	//}
	//
	//ozz::sample::Mesh AnimationSystem::LoadMesh(const char* in_filename)
	//{
	//	MEGA_ASSERT(IsInitialized(), "Animation system is not initialized");
	//
	//	// Open file
	//	ozz::io::File file(in_filename, "rb");
	//	if (!file.opened()) {
	//		std::cout << "Cannot open file " << in_filename << "." << std::endl;
	//	}
	//
	//	ozz::io::IArchive archive(&file);
	//
	//	if (!archive.TestTag<ozz::sample::Mesh>()) {
	//		std::cout << "Archive doesn't contain the expected object type." << std::endl;
	//	}
	//
	//	ozz::sample::Mesh out_mesh;
	//	archive >> out_mesh;
	//	std::cout << "Loaded mesh" << std::endl;
	//	return out_mesh;
	//}
} // namespace Mega