#pragma once

#include <iostream>

#include "OpenAL/alc.h"
#include "OpenAL/al.h"

#include "Engine/ECS/System.h"
#include "Engine/Sound/SoundData.h"
#include "Engine/Sound/SoundBuffer.h"
#include "Engine/Sound/SoundComponents.h"

namespace Mega
{
	class SoundSystem : public System
	{
	public:
		eMegaResult OnInitialize() override;
		eMegaResult OnDestroy() override;
		eMegaResult OnUpdate(const tTimestep in_dt, Scene* in_pScene) override;

		SoundData LoadSound(const tFilePath in_soundPath);

	private:
		SoundBuffer* m_pSoundBuffer = nullptr;

		ALCdevice* m_pALCDevice = nullptr;
		ALCcontext* m_pALCContext = nullptr;
	};
}