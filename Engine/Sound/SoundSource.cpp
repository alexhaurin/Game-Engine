
#include "SoundSource.h"

#include <iostream>

#include "OpenAL/alc.h"
#include "OpenAL/al.h"

#include "Engine/Sound/SoundData.h"

namespace Mega
{
	SoundSource::SoundSource()
	{
		alGenSources(1, &pSoundSource);
		alSourcef(pSoundSource, AL_PITCH, pitch);
		alSourcef(pSoundSource, AL_GAIN, gain);
		alSource3f(pSoundSource, AL_POSITION, position[0], position[1], position[2]);
		alSource3f(pSoundSource, AL_VELOCITY, velocity[0], velocity[1], velocity[2]);

		DUMP_OPENAL_ERRORS("Sound source init error");
	}

	SoundSource::~SoundSource()
	{
		alDeleteSources(1, &pSoundSource);
	}

	void SoundSource::Play(const SoundData& in_sound)
	{
		alSourcei(pSoundSource, AL_BUFFER, in_sound.soundID);
		alSourcePlay(pSoundSource);

		DUMP_OPENAL_ERRORS("Sound source play error");
	}
}
