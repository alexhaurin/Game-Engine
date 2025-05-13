#include "SoundBuffer.h"

#include <cinttypes>
#include <iostream>

#include "AudioFile/AudioFile.h"
#include "Engine/Core/Debug.h"
#include "Engine/Sound/SoundData.h"

namespace Mega
{
	// Courtesy of  enigma tutorials on youtube: https://www.youtube.com/watch?v=WvND0djMcfE
	SoundData SoundBuffer::AddSoundEffect(const tFilePath in_filename)
	{
		std::cout << "Loading sound from " << in_filename << ": " << std::endl;

		// Open file
		AudioFile<float> soundFile;
		DUMP_OPENAL_ERRORS("Sound loading error");
		bool result = soundFile.load(in_filename.data());
		MEGA_ASSERT(result, "Failed to load audio file");

		std::vector<uint8_t> pcmDataBytes;
		soundFile.writePCMToBuffer(pcmDataBytes);
		DUMP_OPENAL_ERRORS("Sound loading error");

		// Get format of the sound data (stereo, mono, etc)
		// Lambda to deteermine which open al format the loaded sound file is in
		auto convertFileToOpenALFormat = [](const AudioFile<float>& in_audioFile)
		{
			int bitDepth = in_audioFile.getBitDepth();
			switch (bitDepth)
			{
			case (16):
				return in_audioFile.isStereo() ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
				break;
			case (8):
				return in_audioFile.isStereo() ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
				break;
			default:
				MEGA_ASSERT(true, "Failed to find audio file format");
				return -1; // Shouldn't happen
			}
		};
		DUMP_OPENAL_ERRORS("Sound loading error");

		// Load into openal
		SoundData soundBuffer;
		alGenBuffers(1, &soundBuffer.soundID);
		alBufferData(soundBuffer.soundID, convertFileToOpenALFormat(soundFile), pcmDataBytes.data(), static_cast<ALsizei>(pcmDataBytes.size()), soundFile.getSampleRate());
		DUMP_OPENAL_ERRORS("Sound loading error");

		ALint sizeInBytes;
		ALint channels;
		ALint bits;
		ALint frequency;
		alGetBufferi(soundBuffer.soundID, AL_SIZE, &sizeInBytes);
		alGetBufferi(soundBuffer.soundID, AL_CHANNELS, &channels);
		alGetBufferi(soundBuffer.soundID, AL_BITS, &bits);
		alGetBufferi(soundBuffer.soundID, AL_FREQUENCY, &frequency);
		auto lengthInSamples = sizeInBytes * 8 / (channels * bits);
		soundBuffer.length = (float)lengthInSamples / (float)frequency;

		MEGA_ASSERT(soundBuffer.length > 0, "Loading sound did not work properly - length of 0");

		DUMP_OPENAL_ERRORS("Sound loading error");
		return soundBuffer;
	}

	bool SoundBuffer::RemoveSoundEffect(const ALuint in_buffer)
	{
		MEGA_ASSERT(false, "Not implemented yet");
		return false;
	}
}