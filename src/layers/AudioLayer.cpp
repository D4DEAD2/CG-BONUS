#include "AudioLayer.h"
#include "AudioEngine.h"

void AudioLayer::Initialize()
{
	AudioEngine::Init();
	AudioEngine::LoadBank("banks/Master", FMOD_STUDIO_LOAD_BANK_NORMAL);
	AudioEngine::LoadEvent("Music", "{f51c7b6b-6861-4f9f-a6df-c2195fd68c8a}");
	AudioEngine::PlayEvent("Music");
	AudioEngine::SetEventParameter("Music", "Volume", 0.5f);
}

void AudioLayer::Shutdown()
{
	AudioEngine::Shutdown();
}

void AudioLayer::Update()
{
	AudioEngine::Update();
}
