#pragma once

#include <vector>

#include "OpenAL/al.h"
#include "Engine/Core/Core.h"
#include "Engine/Sound/SoundData.h"

namespace Mega
{
	class SoundBuffer
	{
	public:
		SoundData AddSoundEffect(const tFilePath in_filename);
		bool RemoveSoundEffect(const ALuint in_buffer);

	private:
		std::vector<ALuint> m_pSoundBuffers;
	};
}