#pragma once

#include <string_view>
#include <unordered_map>

#include "Engine/Core/Time.h"
#include "Engine/ECS/Components.h"
#include "Engine/Sound/SoundData.h"
#include "Engine/Sound/SoundSource.h"

namespace Mega
{
	namespace Component
	{
		struct SoundPlayer : public ComponentBase
		{
			using SoundName = std::string_view;

			SoundPlayer() = default; // Dont remove!!!

			// Functionality
			inline void AddSoundEffect(const SoundName in_name, const SoundData& in_sound)
			{
				MEGA_ASSERT(!sounds.contains(in_name), "Sound has already been added");
				MEGA_ASSERT(in_sound.soundID != INVALID_SOUND_ID, "Adding a sound that has not been properly loaded");
			
				sounds[in_name] = in_sound;
			}
			inline void Play(const SoundName in_name)
			{
				MEGA_ASSERT(sounds.contains(in_name), "Sound has not been added");
				MEGA_ASSERT(sounds[in_name].soundID != INVALID_SOUND_ID, "Playing a sound that has not been loaded properly");
				if (timeToPlay > 0) { return; }

				timeToPlay = (sounds[in_name].length * 1000) + buffer;

				//current = in_name;
				shouldPlay = true;
				current = in_name;
			}
			inline void Pause()
			{
				shouldPlay = false;
			}


			inline void SetPitch(float in_pitch) { soundSource.SetPitch(in_pitch); }
			inline void SetBuffer(const tTimestep in_buffer) { buffer = in_buffer; }
			inline void SetGain(float in_gain) { soundSource.SetGain(in_gain); }

			//SoundName current = "";
			//std::unordered_map<SoundName, SoundData> sounds{};
			SoundName current = "";
			std::unordered_map<SoundName, SoundData> sounds{};
			
			SoundSource soundSource{};
			tTimestep buffer = 0.0f; // How long to wait before playing another sound
			tTimestep timeToPlay = 0.0f;
			tTimestep playSpeed = 1.0f; // Number of times per second, set to 0 for no limit
			bool shouldPlay = false;
		};
	}
}