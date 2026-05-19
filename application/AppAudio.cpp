#include "AppAudio.h"

bool AppAudio::Initialize() {
    return audioSystem_.Initialize();
}

void AppAudio::Finalize() {
    audioSystem_.Shutdown();
}

void AppAudio::Update() {
    audioSystem_.Update();
}

audio::SoundHandle AppAudio::LoadSound(const std::string& path) {
    return audioSystem_.LoadSound(path);
}

audio::SoundHandle AppAudio::LoadWave(const std::string& path) {
    return audioSystem_.LoadWave(path);
}

bool AppAudio::Play(audio::SoundHandle handle, float volume, bool loop) {
    return audioSystem_.Play(handle, volume, loop);
}

void AppAudio::Unload(audio::SoundHandle handle) {
    audioSystem_.UnloadSound(handle);
}
