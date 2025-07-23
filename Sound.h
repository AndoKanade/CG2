#pragma once
#include <cassert>
#include <fstream>
#include <string>
#include <wrl.h>
#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")

#pragma region 構造体
struct ChunkHeader {
  char id[4];
  int32_t size;
};

struct RiffHeader {
  ChunkHeader chunk;
  char type[4];
};

struct FormatChunk {
  ChunkHeader chunk;
  WAVEFORMATEX fmt;
};

struct SoundData {
  WAVEFORMATEX wfex;
  BYTE *pBuffer;
  unsigned int bufferSize;
};

#pragma endregion

class SoundManager {
public:
  void Initialize();

  SoundData SoundLoadWave(const char *filename);
  void SoundPlayWave(const SoundData &soundData);

  void SoundUnload(SoundData &soundData);

private:
  Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
  IXAudio2MasteringVoice *masterVoice_;
};