#pragma once

#include <limits>
#include <OpenAL/al.h>

inline constexpr ALuint INVALID_SOUND_ID = UINT_MAX;

#define DUMP_OPENAL_ERRORS(message)\
{\
	ALenum err = alGetError();\
	if (err != AL_NO_ERROR)\
	{\
		std::cout << "OpenAL error: " << message << ": " << alGetString(err) << " " << err << std::endl;\
	}\
}

namespace Mega
{
	struct SoundData
	{
		ALuint soundID = INVALID_SOUND_ID;
		ALfloat length = 0;
	};
}