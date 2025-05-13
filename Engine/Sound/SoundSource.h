#pragma once

#include "SoundData.h"
#include "Engine/Core/Math/Vector.h"

namespace Mega
{
	class SoundSource
	{
	public:
		SoundSource();
		~SoundSource();

		void Play(const SoundData& in_sound);
		inline void SetPitch(float in_pitch) { pitch = in_pitch; alSourcef(pSoundSource, AL_PITCH, in_pitch); }
		inline void SetGain(float in_gain) { gain = in_gain, alSourcef(pSoundSource, AL_GAIN, in_gain); }

	private:
		ALuint pSoundSource = 0;
		float pitch = 1.0f;
		float gain = 1.0f;
		Vec3 position = { 0.0f, 0.0f, 0.0f };
		Vec3 velocity = { 0.0f, 0.0f, 0.0f };
	};
}