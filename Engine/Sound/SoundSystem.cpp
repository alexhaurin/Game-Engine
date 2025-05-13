#include "SoundSystem.h"

#include <iostream>

#include "Engine/ECS/ECS.h"
#include "Engine/Core/Core.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Sound/SoundSource.h"
#include "Engine/Sound/SoundComponents.h"

namespace Mega
{
	eMegaResult SoundSystem::OnInitialize()
	{
		// Init OpenAL
		m_pALCDevice = alcOpenDevice(nullptr);
		MEGA_ASSERT(m_pALCDevice, "Failed to openal sound device");

		m_pALCContext = alcCreateContext(m_pALCDevice, nullptr);
		MEGA_ASSERT(m_pALCContext, "Failed to create openal context");

		ALCboolean result = alcMakeContextCurrent(m_pALCContext);
		MEGA_ASSERT(result, "Failed to make openal context current");

		const ALCchar* name = nullptr;
		if (alcIsExtensionPresent(m_pALCDevice, "ALC_ENUMERATE_ALL_EXT"))
		{
			name = alcGetString(m_pALCDevice, ALC_ALL_DEVICES_SPECIFIER);
		}
		if (!name || alcGetError(m_pALCDevice) != AL_NO_ERROR)
		{
			name = alcGetString(m_pALCDevice, ALC_DEVICE_SPECIFIER);
		}
		std::cout << "OpenAL Context: " << name << std::endl;

		// Sound buffer and source
		m_pSoundBuffer = new SoundBuffer;

		return eMegaResult::SUCCESS;
	}

	eMegaResult SoundSystem::OnDestroy()
	{
		// Cleanup OpenAL
		delete m_pSoundBuffer;

		if (!alcMakeContextCurrent(nullptr))
		{
			DUMP_OPENAL_ERRORS("Sound system destroy error");
			MEGA_RUNTIME_ERROR("failed to set context to nullptr");
		}

		alcDestroyContext(m_pALCContext);
		DUMP_OPENAL_ERRORS("Sound system destroy error");

		if (!alcCloseDevice(m_pALCDevice))
		{
			DUMP_OPENAL_ERRORS("Sound system destroy error");
			MEGA_RUNTIME_ERROR("Failed to close sound device");
		}

		DUMP_OPENAL_ERRORS("OpenAL destroy error");

		return eMegaResult::SUCCESS;
	}

	eMegaResult SoundSystem::OnUpdate(const tTimestep in_dt, Scene* in_pScene)
	{
		auto view = in_pScene->GetRegistry().view<Component::SoundPlayer>();
		for (const auto& [entity, player] : view.each())
		{
			player.timeToPlay -= in_dt;

			if (player.shouldPlay && player.timeToPlay > 0)
			{
				MEGA_ASSERT(player.sounds[player.current].soundID != INVALID_SOUND_ID, "Sound has not been loaded properly");
				
				if (player.timeToPlay > player.sounds[player.current].length * 1000) { continue; }
				player.soundSource.Play(player.sounds[player.current]);

				DUMP_OPENAL_ERRORS("Error playing sound");
			}
			player.shouldPlay = false;
		}

		return eMegaResult::SUCCESS;
	}

	SoundData SoundSystem::LoadSound(const tFilePath in_soundPath)
	{
		MEGA_ASSERT(IsInitialized(), "Loading sound with unitialized Sound System");

		return m_pSoundBuffer->AddSoundEffect(in_soundPath);
	}
}