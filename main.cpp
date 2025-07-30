#define _USE_MATH_DEFINES
#define PI 3.14159265f
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string.h>
#include <strsafe.h>
#include <vector>
#include <wrl.h>
#include <xaudio2.h>
#define DRECTINPUT_VERSION 0x0800 // DirectInput version 8.0
#include <dinput.h>

#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <iostream>
#include <map>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lPalam);

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace DirectX;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
    return true;
  }

  switch (msg) {
  case WM_DESTROY:

    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

#pragma region 構造体
struct Vector2 {
  float x, y;
};

inline bool operator<(const Vector2 &a, const Vector2 &b) {
  if (a.x != b.x)
    return a.x < b.x;
  return a.y < b.y;
}

inline bool operator!=(const Vector2 &a, const Vector2 &b) {
  return a.x != b.x || a.y != b.y;
}

struct Vector3 {
  float x, y, z;
};

inline bool operator<(const Vector3 &a, const Vector3 &b) {
  if (a.x != b.x)
    return a.x < b.x;
  if (a.y != b.y)
    return a.y < b.y;
  return a.z < b.z;
}

inline bool operator!=(const Vector3 &a, const Vector3 &b) {
  return a.x != b.x || a.y != b.y || a.z != b.z;
}

struct Vector4 {
  float x, y, z, w;
};

inline bool operator<(const Vector4 &a, const Vector4 &b) {
  if (a.x != b.x)
    return a.x < b.x;
  if (a.y != b.y)
    return a.y < b.y;
  if (a.z != b.z)
    return a.z < b.z;
  return a.w < b.w;
}

inline bool operator!=(const Vector4 &a, const Vector4 &b) {
  return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w;
}
typedef struct Matrix4x4 {
  float m[4][4];
} Matrix4x4;

typedef struct Matrix3x3 {
  float m[3][3];
} Matrix3x3;

typedef struct Transform {
  Vector3 scale;
  Vector3 rotate;
  Vector3 translate;
} Ttansform;

typedef struct TransformationMatrix {
  Matrix4x4 WVP;
  Matrix4x4 World;
} TransformationMatrix;

typedef struct Material {
  Vector4 color;
  int32_t enableLighting;
  float padding[3];
  Matrix4x4 uvTransform;
} Material;

struct VertexData {
  Vector4 position;
  Vector2 texcoord;
  Vector3 normal;

  bool operator<(const VertexData &other) const {
    if (position != other.position)
      return position < other.position;
    if (texcoord != other.texcoord)
      return texcoord < other.texcoord;
    if (normal != other.normal)
      return normal < other.normal;
    return false;
  }
};

struct DirectionalLight {
  Vector4 color;     // ライトの色
  Vector3 direction; // ライトの方向
  float intensity;   // ライトの強度
};

struct MaterialData {
  std::string textureFilePath;
};

struct ModelData {
  std::vector<VertexData> vertices;
  MaterialData material;
};

struct D3DResourceLeakChecker {
  ~D3DResourceLeakChecker() {

    Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
      debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
      debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
      debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
    }
  }
};

#pragma region サウンド再生
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
  BYTE *pBUffer;
  unsigned int bufferSize;
  ;
};

#pragma endregion

struct WindowData {
  HINSTANCE hInstance;
  HWND hwnd;
  // 他にも必要な情報を含む
};

#pragma endregion

#pragma region 関数たち

#pragma region 音声データの読み込み

SoundData SoundLoadWave(const char *filename) {

  std::ifstream file;
  file.open(filename, std::ios_base::binary);
  assert(file.is_open());

  RiffHeader riff;
  file.read((char *)&riff, sizeof(riff));

  if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
    assert(0);
  }

  if (strncmp(riff.type, "WAVE", 4) != 0) {
    assert(0);
  }

  FormatChunk format = {};
  file.read((char *)&format, sizeof(ChunkHeader));

  if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
    assert(0);
  }

  assert(format.chunk.size <= sizeof(format.fmt));
  file.read((char *)&format.fmt, format.chunk.size);

  ChunkHeader data;
  file.read((char *)&data, sizeof(data));

  if (strncmp(data.id, "JUNK", 4) == 0) {
    file.seekg(data.size, std::ios_base::cur);

    file.read((char *)&data, sizeof(data));
  }

  if (strncmp(data.id, "data", 4) != 0) {
    assert(0);
  }

  char *pBuffer = new char[data.size];
  file.read(pBuffer, data.size);

  file.close();

  SoundData soundData = {};

  soundData.wfex = format.fmt;
  soundData.pBUffer = reinterpret_cast<BYTE *>(pBuffer);
  soundData.bufferSize = data.size;

  return soundData;
}

#pragma endregion

#pragma region 音声データの解放
void SoundUnload(SoundData *soundData) {
  delete[] soundData->pBUffer;

  soundData->pBUffer = 0;
  soundData->bufferSize = 0;
  soundData->wfex = {};
}

#pragma endregion

#pragma region サウンドの再生

void SoundPlayWave(IXAudio2 *xAudio2, const SoundData &soundData) {
  HRESULT result;

  IXAudio2SourceVoice *pSourceVoice = nullptr;
  result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
  assert(SUCCEEDED(result));

  XAUDIO2_BUFFER buf{};

  buf.pAudioData = soundData.pBUffer;
  buf.AudioBytes = soundData.bufferSize;
  buf.Flags = XAUDIO2_END_OF_STREAM;

  result = pSourceVoice->SubmitSourceBuffer(&buf);
  result = pSourceVoice->Start();
}

#pragma endregion

#pragma region ConvertString関数
std::wstring ConvertString(const std::string &str) {
  if (str.empty()) {
    return std::wstring();
  }

  auto sizeNeeded =
      MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]),
                          static_cast<int>(str.size()), NULL, 0);
  if (sizeNeeded == 0) {
    return std::wstring();
  }
  std::wstring result(sizeNeeded, 0);
  MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]),
                      static_cast<int>(str.size()), &result[0], sizeNeeded);
  return result;
}

std::string ConvertString(const std::wstring &str) {
  if (str.empty()) {
    return std::string();
  }

  auto sizeNeeded =
      WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                          NULL, 0, NULL, NULL);
  if (sizeNeeded == 0) {
    return std::string();
  }
  std::string result(sizeNeeded, 0);
  WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                      result.data(), sizeNeeded, NULL, NULL);
  return result;
}

#pragma endregion

#pragma region log関数

// ログファイルの方
void Log(std::ostream &os, const std::string &message) {
  os << message << std::endl;
  OutputDebugStringA(message.c_str());
}

//
void Log(const std::string &message) { OutputDebugStringA(message.c_str()); }
#pragma endregion

#pragma region ダンプ
static LONG WINAPI ExportDump(EXCEPTION_POINTERS *exception) {

  SYSTEMTIME time;
  GetLocalTime(&time);
  wchar_t filePath[MAX_PATH] = {0};
  CreateDirectory(L"./Dumps", nullptr);
  StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
                   time.wYear, time.wMonth, time.wDay, time.wHour,
                   time.wMinute);
  HANDLE dumpFileHandle =
      CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
                 FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

  // processIdとクラッシュの発生したthreadIdを取得
  DWORD processId = GetCurrentProcessId();
  DWORD threadId = GetCurrentThreadId();

  // 設定情報を入力
  MINIDUMP_EXCEPTION_INFORMATION minidumpinformation{0};
  minidumpinformation.ThreadId = threadId;
  minidumpinformation.ExceptionPointers = exception;
  minidumpinformation.ClientPointers = TRUE;

  // Dumpを出力、MiniDumpWriteNormalは最低限の情報を出力するフラグ
  MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle,
                    MiniDumpNormal, &minidumpinformation, nullptr, nullptr);

  return EXCEPTION_EXECUTE_HANDLER;
}
#pragma endregion

#pragma region CompileShader関数
Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
    // CompilerするShaderファイルへのパス
    const std::wstring &filePath,
    // Compilerに使用するProfile
    const wchar_t *profile,
    // 初期化で生成したもの3つt
    const Microsoft::WRL::ComPtr<IDxcUtils> &dxcUtils,
    const Microsoft::WRL::ComPtr<IDxcCompiler3> &dxCompiler,
    IDxcIncludeHandler *includeHandler) {

#pragma region HLSLファイルを読み込む

  Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n",
                                filePath, profile)));

  Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
  HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr,
                                  shaderSource.GetAddressOf());

  // 読み込めなかったら止まる
  assert(SUCCEEDED(hr));

  // 読み込んだファイルの内容を設定する
  DxcBuffer shaderSourceBuffer;
  shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
  shaderSourceBuffer.Size = shaderSource->GetBufferSize();
  shaderSourceBuffer.Encoding = DXC_CP_UTF8;
#pragma endregion

#pragma region コンパイルする

  LPCWSTR arguments[] = {
      filePath.c_str(), // コンパイル対象のファイル名
      L"-E",
      L"main", // エントリーポイントの指定
      L"-T",
      profile, // ShaderCompileの設定
      L"-Zi",
      L"-Qembed_debug", // デバッグ用の情報を埋め込む
      L"-Od",           // 最適化を外しておく
      L"-Zpr",          // メモリレイアウトは行優先
  };

  // コンパイラの設定
  Microsoft::WRL::ComPtr<IDxcResult> shaderResult = nullptr;
  hr = dxCompiler->Compile(&shaderSourceBuffer, // 読み込んだファイル
                           arguments,           // コンパイルオプション
                           _countof(arguments), // コンパイルオプションの数
                           includeHandler,      // includeが含まれた数
                           IID_PPV_ARGS(&shaderResult) // コンパイル結果
  );

  // コンパイルエラーでなくdxcが起動できないなど致命的なエラーが起きたら止まる
  assert(SUCCEEDED(hr));
#pragma endregion

#pragma region 警告 エラーが出ていないか確認する

  Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError = nullptr;
  shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
  if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
    Log(shaderError->GetStringPointer());

    // 警告、エラーダメゼッタイ
    assert(false);
  }

#pragma endregion

#pragma region コンパイル結果を取得して返す

  IDxcBlob *shaderBlob = nullptr;
  hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob),
                               nullptr);
  assert(SUCCEEDED(hr));

  // 成功したらログを出す
  Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n",
                                filePath, profile)));

  // コンパイル結果を返す
  return shaderBlob;

#pragma endregion
}

#pragma endregion

#pragma region DescriptorHeap関数

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
CreateDiscriptorHeap(const Microsoft::WRL::ComPtr<ID3D12Device> &device,
                     D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
                     bool shaderVisible) {
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
  descriptorHeapDesc.Type = heapType;
  descriptorHeapDesc.NumDescriptors = numDescriptors;
  descriptorHeapDesc.Flags = shaderVisible
                                 ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                 : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc,
                                            IID_PPV_ARGS(&descriptorHeap));
  assert(SUCCEEDED(hr));

  return descriptorHeap;
}

#pragma endregion

#pragma region Resource関数
Microsoft::WRL::ComPtr<ID3D12Resource>
CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device> &device,
                     size_t sizeInBytes) {

  D3D12_HEAP_PROPERTIES uploadHeapProperties{};
  uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC resourceDesc{};
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resourceDesc.Width = sizeInBytes;
  resourceDesc.Height = 1;
  resourceDesc.DepthOrArraySize = 1;
  resourceDesc.MipLevels = 1;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  Microsoft::WRL::ComPtr<ID3D12Resource> buffer;

  HRESULT hr = device->CreateCommittedResource(
      &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));

  assert(SUCCEEDED(hr));

  return buffer;
}

#pragma endregion

#pragma region DepthStencilTexture関数

Microsoft::WRL::ComPtr<ID3D12Resource>
CreateDepthStencilResource(const Microsoft::WRL::ComPtr<ID3D12Device> &device,
                           int32_t width, int32_t height) {
  // 生成するResourceの設定
  D3D12_RESOURCE_DESC resourceDesc{};

  resourceDesc.Width = width;
  resourceDesc.Height = height;
  resourceDesc.MipLevels = 1;
  resourceDesc.DepthOrArraySize = 1;
  resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  // 利用するheapの設定
  D3D12_HEAP_PROPERTIES heapProperties{};

  heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

  D3D12_CLEAR_VALUE depthClearValue{};

  depthClearValue.DepthStencil.Depth = 1.0f;
  depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

  Microsoft::WRL::ComPtr<ID3D12Resource> resource;

  HRESULT hr = device->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
      D3D12_RESOURCE_STATE_COPY_DEST, &depthClearValue,
      IID_PPV_ARGS(&resource));
  assert(SUCCEEDED(hr));
  return resource;
}

#pragma endregion

#pragma region Texture関数

DirectX::ScratchImage LoadTexture(const std::string &filePath) {
  DirectX::ScratchImage image{};
  std::wstring filePathW = ConvertString(filePath);
  HRESULT hr = DirectX::LoadFromWICFile(
      filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
  assert(SUCCEEDED(hr));

  DirectX::ScratchImage mipImages{};
  hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
                                image.GetMetadata(), DirectX::TEX_FILTER_SRGB,
                                0, mipImages);
  assert(SUCCEEDED(hr));

  return mipImages;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device> &device,
                      const DirectX::TexMetadata &metadata) {

  // meradataをもとにresourceの設定
  D3D12_RESOURCE_DESC resourceDesc = {};
  resourceDesc.Width = UINT(metadata.width);
  resourceDesc.Height = UINT(metadata.height);
  resourceDesc.MipLevels = UINT16(metadata.mipLevels);
  resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
  resourceDesc.Format = metadata.format;
  resourceDesc.SampleDesc.Count = 1;
  resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

  // 利用するheapの設定
  D3D12_HEAP_PROPERTIES heapProperties{};

  heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
  heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
  heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

  // resourceを生成

  Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
  HRESULT hr = device->CreateCommittedResource(
      &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
  assert(SUCCEEDED(hr));

  return resource;
}

[[nodiscard]]
Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(
    const Microsoft::WRL::ComPtr<ID3D12Resource> &texture,
    const DirectX::ScratchImage &mipImages,
    const Microsoft::WRL::ComPtr<ID3D12Device> &device,
    const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> &commandList) {

  std::vector<D3D12_SUBRESOURCE_DATA> subresources;
  DirectX::PrepareUpload(device.Get(), mipImages.GetImages(),
                         mipImages.GetImageCount(), mipImages.GetMetadata(),
                         subresources);
  uint64_t intermediateSize =
      GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
  Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource =
      CreateBufferResource(device, intermediateSize);
  UpdateSubresources(commandList.Get(), texture.Get(),
                     intermediateResource.Get(), 0, 0,
                     UINT(subresources.size()), subresources.data());

  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = texture.Get();
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
  commandList->ResourceBarrier(1, &barrier);
  return intermediateResource;
}

D3D12_CPU_DESCRIPTOR_HANDLE
GetCPUDescriptorHandle(ID3D12DescriptorHeap *descriptorHeap,
                       uint32_t descriptorSize, uint32_t index) {
  D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
      descriptorHeap->GetCPUDescriptorHandleForHeapStart();
  handleCPU.ptr += descriptorSize * index;
  return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE
GetGPUDscriptorHandle(ID3D12DescriptorHeap *descriptorHeap,
                      uint32_t descriptorSize, uint32_t index) {
  D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
      descriptorHeap->GetGPUDescriptorHandleForHeapStart();
  handleGPU.ptr += descriptorSize * index;
  return handleGPU;
}

#pragma endregion

#pragma region MaterialTemplate関数
MaterialData LoadMaterialTemplateFile(const std::string &directoryPath,
                                      const std::string &filename) {
  MaterialData materialData;
  std::string line;
  std::ifstream file(directoryPath + "/" + filename);

  assert(file.is_open());

  while (std::getline(file, line)) {
    std::string identifier;
    std::istringstream s(line);

    s >> identifier;

    if (identifier == "map_Kd") {
      std::string textureFilename;
      s >> textureFilename;

      materialData.textureFilePath = directoryPath + "/" + textureFilename;
    }
  }
  return materialData;
}
#pragma endregion

#pragma region Objファイルを読む関数

ModelData LoadObjFile(const std::string &directoryPath,
                      const std::string &filename) {
  ModelData modelData;
  std::vector<Vector4> positions;
  std::vector<Vector3> normals;
  std::vector<Vector2> texcoords;

  std::ifstream file(directoryPath + "/" + filename);
  if (!file.is_open()) {
    std::cerr << "Failed to open OBJ file: " << directoryPath + "/" + filename
              << std::endl;
    return {};
  }

  std::string line;

  while (std::getline(file, line)) {
    std::istringstream s(line);
    std::string identifier;
    s >> identifier;

    if (identifier == "v") {
      Vector4 position{};

      s >> position.x >> position.y >> position.z;

      position.w = 1.0f;
        position.x *= -1.0f;
      positions.push_back(position);

    } else if (identifier == "vt") {
      Vector2 texcoord{};
      s >> texcoord.x >> texcoord.y;

      texcoord.y = 1.0f - texcoord.y;
      texcoords.push_back(texcoord);

    } else if (identifier == "vn") {

      Vector3 normal{};

      s >> normal.x >> normal.y >> normal.z;
      //  normal.x *= -1.0f;
      normals.push_back(normal);

    } else if (identifier == "f") {

      VertexData triangle[3] = {};

      for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
        std::string vdef;
        s >> vdef;

        std::istringstream vstream(vdef);
        uint32_t elementIndices[3];

        for (int32_t element = 0; element < 3; ++element) {
          std::string index;
          std::getline(vstream, index, '/');
          elementIndices[element] = std::stoi(index);
        }

        Vector4 position = positions[elementIndices[0] - 1];
        Vector2 texcoord = texcoords[elementIndices[1] - 1];
        Vector3 normal = normals[elementIndices[2] - 1];

        triangle[faceVertex] = {position, texcoord, normal};
      }
      modelData.vertices.push_back(triangle[2]);
      modelData.vertices.push_back(triangle[1]);
      modelData.vertices.push_back(triangle[0]);

    } else if (identifier == "mtllib") {
      std::string materialFilename;
      s >> materialFilename;

      modelData.material =
          LoadMaterialTemplateFile(directoryPath, materialFilename);
    }
  }
  file.close();
  return modelData;
}

#pragma endregion

#pragma endregion

#pragma region 数学関数

Matrix4x4 MakeIdentity4x4() {
  Matrix4x4 result;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (i == j) {
        result.m[i][j] = 1.0f;
      } else {
        result.m[i][j] = 0.0f;
      }
    }
  }
  return result;
}
Matrix4x4 MakeScaleMatrix(const Vector3 &scale) {
  Matrix4x4 matrix = {}; // すべて0で初期化
  // スケール行列の設定
  matrix.m[0][0] = scale.x;
  matrix.m[1][1] = scale.y;
  matrix.m[2][2] = scale.z;
  matrix.m[3][3] = 1.0f;
  return matrix;
}
Matrix4x4 MakeTranslateMatrix(const Vector3 &translate) {
  Matrix4x4 matrix = {}; // すべて0で初期化
  // 単位行列の形に設定
  matrix.m[0][0] = 1.0f;
  matrix.m[1][1] = 1.0f;
  matrix.m[2][2] = 1.0f;
  matrix.m[3][3] = 1.0f;
  // 平行移動成分を設定
  matrix.m[3][0] = translate.x;
  matrix.m[3][1] = translate.y;
  matrix.m[3][2] = translate.z;
  return matrix;
}
Matrix4x4 MakeRotateXMatrix(float radian) {
  Matrix4x4 result{};

  result.m[0][0] = 1;
  result.m[3][3] = 1;

  // X軸回転に必要な部分だけ上書き
  result.m[1][1] = std::cos(radian);
  result.m[1][2] = std::sin(radian);
  result.m[2][1] = -std::sin(radian);
  result.m[2][2] = std::cos(radian);

  return result;
}
Matrix4x4 MakeRotateYMatrix(float radian) {
  Matrix4x4 result{};

  result.m[1][1] = 1.0f;
  result.m[3][3] = 1.0f;

  result.m[0][0] = std::cos(radian);
  result.m[0][2] = -std::sin(radian);
  result.m[2][0] = std::sin(radian);
  result.m[2][2] = std::cos(radian);

  return result;
}
Matrix4x4 MakeRotateZMatrix(float radian) {
  Matrix4x4 result{};

  result.m[2][2] = 1;
  result.m[3][3] = 1;

  result.m[0][0] = std::cos(radian);
  result.m[0][1] = std::sin(radian);
  result.m[1][0] = -std::sin(radian);
  result.m[1][1] = std::cos(radian);

  return result;
}

Matrix4x4 Multiply(const Matrix4x4 &m1, const Matrix4x4 &m2) {
  Matrix4x4 result{};
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      result.m[row][col] = 0.0f;
      for (int k = 0; k < 4; ++k) {
        result.m[row][col] += m1.m[row][k] * m2.m[k][col];
      }
    }
  }
  return result;
}

Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate,
                           const Vector3 &translate) {

  Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
  Matrix4x4 rotateX = MakeRotateXMatrix(rotate.x);
  Matrix4x4 rotateY = MakeRotateYMatrix(rotate.y);
  Matrix4x4 rotateZ = MakeRotateZMatrix(rotate.z);

  // 回転順: Z → X → Y →（スケーリング）→ 平行移動
  Matrix4x4 rotateMatrix = Multiply(Multiply(rotateX, rotateY), rotateZ);

  Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

  Matrix4x4 affineMatrix =
      Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);

  return affineMatrix;
}
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio,
                                   float nearClip, float farClip) {

  float f = 1.0f / tanf(fovY * 0.5f);
  float range = farClip / (farClip - nearClip);

  Matrix4x4 result = {};

  result.m[0][0] = f / aspectRatio;
  result.m[1][1] = f;
  result.m[2][2] = range;
  result.m[2][3] = 1.0f;
  result.m[3][2] = -range * nearClip; // ← DirectX ではマイナス
  result.m[3][3] = 0.0f;

  return result;
}

Matrix4x4 Inverse(const Matrix4x4 &m) {
  Matrix4x4 result{};

  // 行列の行列式を計算
  float det =
      m.m[0][0] *
          (m.m[1][1] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) -
           m.m[1][2] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) +
           m.m[1][3] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1])) -
      m.m[0][1] *
          (m.m[1][0] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) -
           m.m[1][2] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) +
           m.m[1][3] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0])) +
      m.m[0][2] *
          (m.m[1][0] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) -
           m.m[1][1] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) +
           m.m[1][3] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) -
      m.m[0][3] * (m.m[1][0] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1]) -
                   m.m[1][1] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0]) +
                   m.m[1][2] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0]));

  if (det == 0) {
    // 行列式がゼロの場合、逆行列は存在しません
    return result; // 逆行列は存在しないのでゼロ行列を返す
  }

  // 行列式の逆数を計算
  float invDet = 1.0f / det;

  // 各要素を余因子行列から計算
  result.m[0][0] =
      (m.m[1][1] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) -
       m.m[1][2] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) +
       m.m[1][3] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1])) *
      invDet;
  result.m[0][1] =
      (-m.m[0][1] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) +
       m.m[0][2] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) -
       m.m[0][3] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1])) *
      invDet;
  result.m[0][2] =
      (m.m[0][1] * (m.m[1][2] * m.m[3][3] - m.m[1][3] * m.m[3][2]) -
       m.m[0][2] * (m.m[1][1] * m.m[3][3] - m.m[1][3] * m.m[3][1]) +
       m.m[0][3] * (m.m[1][1] * m.m[3][2] - m.m[1][2] * m.m[3][1])) *
      invDet;
  result.m[0][3] =
      (-m.m[0][1] * (m.m[1][2] * m.m[2][3] - m.m[1][3] * m.m[2][2]) +
       m.m[0][2] * (m.m[1][1] * m.m[2][3] - m.m[1][3] * m.m[2][1]) -
       m.m[0][3] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1])) *
      invDet;

  result.m[1][0] =
      (-m.m[1][0] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) +
       m.m[1][2] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) -
       m.m[1][3] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0])) *
      invDet;
  result.m[1][1] =
      (m.m[0][0] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) -
       m.m[0][2] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) +
       m.m[0][3] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0])) *
      invDet;
  result.m[1][2] =
      -(m.m[0][0] * (m.m[1][2] * m.m[3][3] - m.m[1][3] * m.m[3][2]) -
        m.m[0][2] * (m.m[1][0] * m.m[3][3] - m.m[1][3] * m.m[3][0]) +
        m.m[0][3] * (m.m[1][0] * m.m[3][2] - m.m[1][2] * m.m[3][0])) *
      invDet;

  result.m[1][3] =
      (m.m[0][0] * (m.m[1][2] * m.m[2][3] - m.m[1][3] * m.m[2][2]) -
       m.m[0][2] * (m.m[1][0] * m.m[2][3] - m.m[1][3] * m.m[2][0]) +
       m.m[0][3] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0])) *
      invDet;

  result.m[2][0] =
      (m.m[1][0] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) -
       m.m[1][1] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) +
       m.m[1][3] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) *
      invDet;

  result.m[2][1] =
      (-m.m[0][0] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) +
       m.m[0][1] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) -
       m.m[0][3] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) *
      invDet;

  result.m[2][2] =
      (m.m[0][0] * (m.m[1][1] * m.m[3][3] - m.m[1][3] * m.m[3][1]) -
       m.m[0][1] * (m.m[1][0] * m.m[3][3] - m.m[1][3] * m.m[3][0]) +
       m.m[0][3] * (m.m[1][0] * m.m[3][1] - m.m[1][1] * m.m[3][0])) *
      invDet;

  result.m[2][3] =
      (-m.m[0][0] * (m.m[1][1] * m.m[2][3] - m.m[1][3] * m.m[2][1]) +
       m.m[0][1] * (m.m[1][0] * m.m[2][3] - m.m[1][3] * m.m[2][0]) -
       m.m[0][3] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0])) *
      invDet;

  result.m[3][0] =
      (-m.m[1][0] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1]) +
       m.m[1][1] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0]) -
       m.m[1][2] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) *
      invDet;

  result.m[3][1] =
      (m.m[0][0] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1]) -
       m.m[0][1] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0]) +
       m.m[0][2] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) *
      invDet;

  result.m[3][2] =
      (-m.m[0][0] * (m.m[1][1] * m.m[3][2] - m.m[1][2] * m.m[3][1]) +
       m.m[0][1] * (m.m[1][0] * m.m[3][2] - m.m[1][2] * m.m[3][0]) -
       m.m[0][2] * (m.m[1][0] * m.m[3][1] - m.m[1][1] * m.m[3][0])) *
      invDet;

  result.m[3][3] =
      (m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) -
       m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) +
       m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0])) *
      invDet;

  return result;
}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right,
                                 float bottom, float nearClip, float farClip) {
  Matrix4x4 result = {};

  result.m[0][0] = 2.0f / (right - left);
  result.m[1][1] = 2.0f / (top - bottom);
  result.m[2][2] = 1.0f / (farClip - nearClip);
  result.m[3][0] = (left + right) / (left - right);
  result.m[3][1] = (top + bottom) / (bottom - top);
  result.m[3][2] = -nearClip / (farClip - nearClip);
  result.m[3][3] = 1.0f;

  return result;
}

#pragma endregion

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
  D3DResourceLeakChecker leakCheck;

  Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;
  Microsoft::WRL::ComPtr<ID3D12Device> device;

  Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
  IXAudio2MasteringVoice *masterVoice;

  CoInitializeEx(0, COINIT_MULTITHREADED);
  // 例外が発生したらダンプを出力する
  SetUnhandledExceptionFilter(ExportDump);

#pragma region 前準備
#pragma region ログ

  std::filesystem::create_directory("logs");
  // 現在時刻を取得(ロンドン時刻)
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

  // ログファイルの名前を秒に変換
  std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
      nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

  // 日本時間に変換
  std::chrono::zoned_time localTime{std::chrono::current_zone(), nowSeconds};

  // formatを使って年月火_時分秒に変換
  std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);

  // 時刻を使ってファイル名を決定
  std::string logFilePath = std::string("logs/") + dateString + ".log";

  // ファイルを作って書き込み準備
  std::ofstream logStream(logFilePath);
#pragma endregion

  WNDCLASS wc{};

  wc.lpfnWndProc = WindowProc;
  wc.lpszClassName = L"CG2WindowClass";
  wc.hInstance = GetModuleHandle(nullptr);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

  RegisterClass(&wc);

  const int32_t kCliantWidth = 1280;
  const int32_t kCliantHeight = 720;

  RECT wrc = {0, 0, kCliantWidth, kCliantHeight};

  AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

  HWND hwnd =
      CreateWindow(wc.lpszClassName, L"CG2", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                   CW_USEDEFAULT, wrc.right - wrc.left, wrc.bottom - wrc.top,
                   nullptr, nullptr, wc.hInstance, nullptr);

  ShowWindow(hwnd, SW_SHOW);

#pragma region デバッグレイヤー

#ifdef _DEBUG

  Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    // デバッグレイヤーを有効にする
    debugController->EnableDebugLayer();

    // GPUでもチェックするようにする
    debugController->SetEnableGPUBasedValidation(TRUE);
  }
#endif

#pragma endregion

#pragma endregion

#pragma region DXGIFactoryの作成
  //  Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;

  HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

  assert(SUCCEEDED(hr));

#pragma endregion

#pragma region 使用するGPUを決める
  // 使用するアダプターの変数、最初はnullptrを入れておく
  Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;

  // 良い順にアダプターを頼む
  for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
                       i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                       IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
       i++) {

    // アダプターの情報を取得する
    DXGI_ADAPTER_DESC3 adapterDesc{};

    hr = useAdapter->GetDesc3(&adapterDesc);
    assert(SUCCEEDED(hr));

    // ソフトウェアアダプター出なければ採用
    if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
      Log(logStream, std::format("Use Adapter : {}\n",
                                 ConvertString(adapterDesc.Description)));
      break;
    }
    useAdapter = nullptr;
  }

  // 適切なアダプターが見つからなかったら起動しない

  assert(useAdapter != nullptr);
#pragma endregion

#pragma region D3D12Deviceの作成
  // Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;
  //  機能レベルとログ出力用の文字列
  D3D_FEATURE_LEVEL featureLevels[] = {

      D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};

  const char *featureLevelStrings[] = {"12.2", "12.1", "12.0"};

  // 高い順に生成できるか試していく
  for (size_t i = 0; i < _countof(featureLevels); i++) {
    hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i],
                           IID_PPV_ARGS(&device));

    if (SUCCEEDED(hr)) {
      // 生成できたのでループを抜ける
      Log(logStream,
          std::format("FeatureLevels : {}\n", featureLevelStrings[i]));
      break;
    }
  }

  // 生成がうまくいかなかったので起動しない
  assert(SUCCEEDED(hr));
  Log("Complete create D3D12Device!!\n");

#pragma endregion

#pragma region コマンドキューの作成

  Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
  D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
  hr = device->CreateCommandQueue(&commandQueueDesc,
                                  IID_PPV_ARGS(&commandQueue));

  // コマンドキューの生成に失敗したら起動しない
  assert(SUCCEEDED(hr));

  // コマンドアロケータの生成
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
  hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      IID_PPV_ARGS(&commandAllocator));

  // コマンドリストを生成する
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
  hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                 commandAllocator.Get(), nullptr,
                                 IID_PPV_ARGS(&commandList));
  // コマンドリストの生成に失敗したら起動しない
  assert(SUCCEEDED(hr));

  Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
  swapChainDesc.Width = kCliantWidth;
  swapChainDesc.Height = kCliantHeight;
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc.SampleDesc.Count = 1;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount = 2;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

  // コマンドキュー、ウィンドウハンドル、スワップチェインの設定

  hr = dxgiFactory->CreateSwapChainForHwnd(
      commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr,
      reinterpret_cast<IDXGISwapChain1 **>(swapChain.GetAddressOf()));
  // スワップチェインの生成に失敗したら起動しない
  assert(SUCCEEDED(hr));

#pragma endregion

#pragma region DescriptorHeapの作成

  // ディスクリプタヒープの生成
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap =
      CreateDiscriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap =
      CreateDiscriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 28,
                           true);

  /// DSVの生成
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap =
      CreateDiscriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

  // swapChainからResourceを取得する
  Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2] = {nullptr};
  hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
  assert(SUCCEEDED(hr));
  hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
  assert(SUCCEEDED(hr));

  // RenderTargetViewを生成する
  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
  rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

  // ディスクリプターの先頭を取得
  D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle =
      rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
  // RTVを2つ作るのでディスクリプターを2つ用意
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

  // 1つめを作る
  rtvHandles[0] = rtvStartHandle;
  device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc,
                                 rtvHandles[0]);
  // 2つめを作る
  rtvHandles[1].ptr =
      rtvHandles[0].ptr +
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc,
                                 rtvHandles[1]);

#pragma endregion

#pragma region DepthStencil

  D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

  depthStencilDesc.DepthEnable = true;
  depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

#pragma endregion

#pragma region FenceとEventの作成

  Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;
  uint64_t fenceValue = 0;
  hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE,
                           IID_PPV_ARGS(&fence));
  assert(SUCCEEDED(hr));

  HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  assert(fenceEvent != nullptr);
#pragma endregion

#pragma region DXCの初期化

  Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils = nullptr;
  Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler = nullptr;

  hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
  assert(SUCCEEDED(hr));
  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
  assert(SUCCEEDED(hr));

  IDxcIncludeHandler *includeHandler = nullptr;
  hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
  assert(SUCCEEDED(hr));

#pragma endregion

#pragma region ルートシグネチャーを生成する

  D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

  descriptionRootSignature.Flags =
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

#pragma region RootParameter

  D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
  descriptorRange[0].BaseShaderRegister = 0;
  descriptorRange[0].NumDescriptors = 1;
  descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  descriptorRange[0].OffsetInDescriptorsFromTableStart =
      D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  D3D12_ROOT_PARAMETER rootParameters[4] = {};

  rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  rootParameters[0].Descriptor.ShaderRegister = 0;

  rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  rootParameters[1].Descriptor.ShaderRegister = 1;

  rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
  rootParameters[2].DescriptorTable.NumDescriptorRanges =
      _countof(descriptorRange);

  rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  rootParameters[3].Descriptor.ShaderRegister = 2;

  descriptionRootSignature.pParameters = rootParameters;
  descriptionRootSignature.NumParameters = _countof(rootParameters);

#pragma endregion

#pragma region Sampler

  D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};

  staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  staticSamplers[0].MinLOD = 0.0f;
  staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
  staticSamplers[0].ShaderRegister = 0;
  staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  descriptionRootSignature.pStaticSamplers = staticSamplers;
  descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

#pragma endregion

  Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
  Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
  hr = D3D12SerializeRootSignature(&descriptionRootSignature,
                                   D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob,
                                   &errorBlob);

  if (FAILED(hr)) {
    Log(reinterpret_cast<char *>(errorBlob->GetBufferPointer()));
    assert(false);
  }

  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
  hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
                                   signatureBlob->GetBufferSize(),
                                   IID_PPV_ARGS(&rootSignature));
  assert(SUCCEEDED(hr));

#pragma endregion

#pragma region InputLayoutを生成する

  D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
  inputElementDescs[0].SemanticName = "POSITION";
  inputElementDescs[0].SemanticIndex = 0;
  inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  inputElementDescs[1].SemanticName = "TEXCOORD";
  inputElementDescs[1].SemanticIndex = 0;
  inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
  inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
  inputElementDescs[2].SemanticName = "NORMAL";
  inputElementDescs[2].SemanticIndex = 0;
  inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
  inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

  D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
  inputLayoutDesc.pInputElementDescs = inputElementDescs;
  inputLayoutDesc.NumElements = _countof(inputElementDescs);

#pragma endregion

#pragma region BlendStateを生成する

  D3D12_BLEND_DESC blendDesc{};

  blendDesc.RenderTarget[0].RenderTargetWriteMask =
      D3D12_COLOR_WRITE_ENABLE_ALL;

#pragma endregion

#pragma region RasterrizerStateを生成する

  D3D12_RASTERIZER_DESC resterizerDesc{};
  resterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
  resterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
#pragma endregion

#pragma region shaderをコンパイルする

  Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
      CompileShader(L"Object3D.VS.hlsl", L"vs_6_0", dxcUtils.Get(),
                    dxcCompiler.Get(), includeHandler);
  assert(vertexShaderBlob != nullptr);

  Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
      CompileShader(L"Object3D.PS.hlsl", L"ps_6_0", dxcUtils.Get(),
                    dxcCompiler.Get(), includeHandler);
  assert(pixelShaderBlob != nullptr);

#pragma endregion

#pragma region PSOを生成する

  D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
  graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
  graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
  graphicsPipelineStateDesc.VS = {vertexShaderBlob->GetBufferPointer(),
                                  vertexShaderBlob->GetBufferSize()};
  graphicsPipelineStateDesc.PS = {pixelShaderBlob->GetBufferPointer(),
                                  pixelShaderBlob->GetBufferSize()};
  graphicsPipelineStateDesc.BlendState = blendDesc;
  graphicsPipelineStateDesc.RasterizerState = resterizerDesc;

  // 書き込むRTVの情報
  graphicsPipelineStateDesc.NumRenderTargets = 1;
  graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

  graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
  graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

  // 利用する形状のタイプ(今回は三角形)
  graphicsPipelineStateDesc.PrimitiveTopologyType =
      D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  // どのような画面に色を打ち込むかの設定
  graphicsPipelineStateDesc.SampleDesc.Count = 1;
  graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
  // 実際に生成
  Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState = nullptr;
  hr = device->CreateGraphicsPipelineState(
      &graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
  assert(SUCCEEDED(hr));

#pragma endregion

#pragma region VertexResourceを生成する

  //// 分割数（自由に調整可能）
  // const int kLatitudeDiv = 16;  // 縦（経度）
  // const int kLongitudeDiv = 16; // 横（緯度）

  // int sphereVertexCount = kLatitudeDiv * kLongitudeDiv * 6;

  ModelData modelData = LoadObjFile("resource", "axis.obj");

  // VertexResource を生成
  Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = CreateBufferResource(
      device, sizeof(VertexData) * modelData.vertices.size());

  // Spriteの矩形
  Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite =
      CreateBufferResource(device, sizeof(VertexData) * 6);

#pragma endregion

#pragma region IndexResourceを生成する

  Microsoft::WRL::ComPtr<ID3D12Resource> indexResource = CreateBufferResource(
      device, sizeof(uint32_t) * modelData.vertices.size());

  D3D12_INDEX_BUFFER_VIEW indexBufferView{};
  indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
  indexBufferView.SizeInBytes =
      UINT(sizeof(uint32_t) * modelData.vertices.size());
  indexBufferView.Format = DXGI_FORMAT_R32_UINT;

  Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite =
      CreateBufferResource(device, sizeof(uint32_t) * 6);

  D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};

  indexBufferViewSprite.BufferLocation =
      indexResourceSprite->GetGPUVirtualAddress();
  indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
  indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

#pragma endregion

#pragma region DepthStencillTextureを生成する

  Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource =
      CreateDepthStencilResource(device, kCliantWidth, kCliantHeight);

#pragma endregion

#pragma region MaterialResource

  Microsoft::WRL::ComPtr<ID3D12Resource> materialResource =
      CreateBufferResource(device, sizeof(Material));
  Material *materialData = nullptr;

  materialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
  *materialData = Material{Vector4(1.0f, 1.0f, 1.0f, 1.0f), 1};
  materialData->enableLighting = true;
  materialData->uvTransform = MakeIdentity4x4();

  Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite =
      CreateBufferResource(device, sizeof(Material));
  Material *materialDataSprite = nullptr;
  materialResourceSprite->Map(0, nullptr,
                              reinterpret_cast<void **>(&materialDataSprite));
  *materialDataSprite = Material{Vector4(1.0f, 1.0f, 1.0f, 1.0f), 0};

  materialDataSprite->enableLighting = false;
  materialDataSprite->uvTransform = MakeIdentity4x4();

#pragma endregion

#pragma region directionalLight

  Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource =
      CreateBufferResource(device, sizeof(DirectionalLight));
  DirectionalLight *directionalLightData = nullptr;
  directionalLightResource->Map(
      0, nullptr, reinterpret_cast<void **>(&directionalLightData));

  directionalLightData->color = {1.0f, 1.0f, 1.0f, 1.0f};
  directionalLightData->direction = {0.0f, 0.0f, -1.0f};
  directionalLightData->intensity = 1.0f;

#pragma endregion

#pragma region TransformationMatrix用のResourceを作る

  Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource =
      CreateBufferResource(device, sizeof(TransformationMatrix));
  TransformationMatrix *wvpData = nullptr;

  wvpResource->Map(0, nullptr, reinterpret_cast<void **>(&wvpData));

  /// Sprite

  Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite =
      CreateBufferResource(device, sizeof(Matrix4x4));

  Matrix4x4 *transformationMatrixDataSprite = nullptr;
  transformationMatrixResourceSprite->Map(
      0, nullptr, reinterpret_cast<void **>(&transformationMatrixDataSprite));
  *transformationMatrixDataSprite = MakeIdentity4x4();
  transformationMatrixResourceSprite->Unmap(0, nullptr);

#pragma endregion

#pragma region VertexBufferViewを生成する

  D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
  vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();

  vertexBufferView.SizeInBytes =
      UINT(sizeof(VertexData) * modelData.vertices.size());

  vertexBufferView.StrideInBytes = sizeof(VertexData);

  /// Sprite
  D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
  vertexBufferViewSprite.BufferLocation =
      vertexResourceSprite->GetGPUVirtualAddress();
  vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
  vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

#pragma endregion

#pragma region DepthStencilViewを生成する

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};

  dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

  device->CreateDepthStencilView(
      depthStencilResource.Get(), &dsvDesc,
      dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

#pragma endregion

#pragma region viewportとscissor

  // Viewport
  D3D12_VIEWPORT viewport{};

  viewport.Width = kCliantWidth;
  viewport.Height = kCliantHeight;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  // Scissor
  D3D12_RECT scissorRect{};
  scissorRect.left = 0;
  scissorRect.right = kCliantWidth;
  scissorRect.top = 0;
  scissorRect.bottom = kCliantHeight;

#pragma endregion

#pragma region Textureの読み込み

  DirectX::ScratchImage mipImages = LoadTexture("resource/uvChecker.png");
  const DirectX::TexMetadata metadata = mipImages.GetMetadata();
  Microsoft::WRL::ComPtr<ID3D12Resource> textureResource =
      CreateTextureResource(device, metadata);
  Microsoft::WRL::ComPtr<ID3D12Resource> intermadiate =
      UploadTextureData(textureResource, mipImages, device, commandList);

  DirectX::ScratchImage mipImages2 =
      LoadTexture(modelData.material.textureFilePath);
  const DirectX::TexMetadata &metadata2 = mipImages2.GetMetadata();
  Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 =
      CreateTextureResource(device, metadata2);
  Microsoft::WRL::ComPtr<ID3D12Resource> intermadiate2 =
      UploadTextureData(textureResource2, mipImages2, device, commandList);

#pragma endregion

#pragma region DescriptorSizeの取得

  const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  const uint32_t descriptorSizeRTV =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  const uint32_t descriptorSizeDSV =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

#pragma endregion

#pragma region SRVを生成する

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

  srvDesc.Format = metadata.format;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

  // ２枚目
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};

  srvDesc2.Format = metadata.format;
  srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

  // SRVを作成するDescriptorHeapの場所を決める

  D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU =
      GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 0);

  D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU =
      GetGPUDscriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 0);

  D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 =
      GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);

  D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 =
      GetGPUDscriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);

  textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // SRVを生成する
  device->CreateShaderResourceView(textureResource.Get(), &srvDesc,
                                   textureSrvHandleCPU);

  // SRVを生成する
  device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2,
                                   textureSrvHandleCPU2);

#pragma endregion

#pragma region dsvHandleの取得
  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
      dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

#pragma endregion

#pragma region 頂点データの更新

  // const uint32_t kSubdivision = 16;
  // const float kLonEvery = 2.0f * PI / float(kSubdivision);
  // const float kLatEvery = PI / float(kSubdivision);
  // const float epsilon = 1e-5f;

  VertexData *vertexData = nullptr;
  vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));

  std::memcpy(vertexData, modelData.vertices.data(),
              sizeof(VertexData) * modelData.vertices.size());

  //  vertexResource->Unmap(0, nullptr);

  //// 頂点生成（(kSubdivision + 1)^2 個）
  // for (uint32_t latIndex = 0; latIndex <= kSubdivision; ++latIndex) {
  //   float lat = -PI / 2.0f + kLatEvery * latIndex;
  //   float v = 1.0f - float(latIndex) / float(kSubdivision);

  //  for (uint32_t lonIndex = 0; lonIndex <= kSubdivision; ++lonIndex) {
  //    float lon = lonIndex * kLonEvery;
  //    float u = float(lonIndex) / float(kSubdivision);
  //    if (lonIndex == kSubdivision) {
  //      u = 1.0f - epsilon; // wrap防止
  //    }

  //    Vector3 pos = {cosf(lat) * cosf(lon), sinf(lat), cosf(lat) *
  // sinf(lon)};

  //    uint32_t vertexIndex = latIndex * (kSubdivision + 1) + lonIndex;

  //    vertexData[vertexIndex].position = {pos.x, pos.y, pos.z, 1.0f};
  //    vertexData[vertexIndex].normal = pos;
  //    vertexData[vertexIndex].texcoord = {u, v};
  //  }
  //}

  // uint32_t *indexData = nullptr;
  // indexResource->Map(0, nullptr, reinterpret_cast<void **>(&indexData));

  // uint32_t index = 0;
  // for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
  //   for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
  //     uint32_t i0 = latIndex * (kSubdivision + 1) + lonIndex;
  //     uint32_t i1 = i0 + 1;
  //     uint32_t i2 = i0 + (kSubdivision + 1);
  //     uint32_t i3 = i2 + 1;

  //    // 三角形 1
  //    indexData[index++] = i0;
  //    indexData[index++] = i2;
  //    indexData[index++] = i1;

  //    // 三角形 2
  //    indexData[index++] = i1;
  //    indexData[index++] = i2;
  //    indexData[index++] = i3;
  //  }
  //}

  uint32_t *indexData = nullptr;
  indexResource->Map(0, nullptr, reinterpret_cast<void **>(&indexData));
  std::memcpy(indexData, modelData.vertices.data(),
              sizeof(uint32_t) * modelData.vertices.size());

#pragma region 画像データの頂点データ
  VertexData *vertexDataSprite = nullptr;
  vertexResourceSprite->Map(0, nullptr,
                            reinterpret_cast<void **>(&vertexDataSprite));

  // 矩形の4頂点（左上 → 左下 → 右上 → 右下）
  vertexDataSprite[0].position = {0.0f, 0.0f, 0.0f, 1.0f}; // 左上
  vertexDataSprite[0].texcoord = {0.0f, 0.0f};
  vertexDataSprite[0].normal = {0.0f, 0.0f, -1.0f};

  vertexDataSprite[1].position = {0.0f, 360.0f, 0.0f, 1.0f}; // 左下
  vertexDataSprite[1].texcoord = {0.0f, 1.0f};
  vertexDataSprite[1].normal = {0.0f, 0.0f, -1.0f};

  vertexDataSprite[2].position = {640.0f, 0.0f, 0.0f, 1.0f}; // 右上
  vertexDataSprite[2].texcoord = {1.0f, 0.0f};
  vertexDataSprite[2].normal = {0.0f, 0.0f, -1.0f};

  vertexDataSprite[3].position = {640.0f, 360.0f, 0.0f, 1.0f}; // 右下
  vertexDataSprite[3].texcoord = {1.0f, 1.0f};
  vertexDataSprite[3].normal = {0.0f, 0.0f, -1.0f};

#pragma endregion

#pragma region インデックスデータの生成

  uint32_t *indexDataSprite = nullptr;
  indexResourceSprite->Map(0, nullptr,
                           reinterpret_cast<void **>(&indexDataSprite));

  // 三角形1: 左上 → 左下 → 右上
  indexDataSprite[0] = 0;
  indexDataSprite[1] = 2;
  indexDataSprite[2] = 1;

  // 三角形2: 左下 → 右下 → 右上
  indexDataSprite[3] = 1;
  indexDataSprite[4] = 2;
  indexDataSprite[5] = 3;

#pragma endregion

#pragma endregion

#pragma region XAudio2の初期化
  HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
  result = xAudio2->CreateMasteringVoice(&masterVoice);
  assert(SUCCEEDED(result));
#pragma endregion

#pragma region DirectInputの初期化
  IDirectInput8 *directInput = nullptr;
  WindowData w = {};

  w.hInstance = hInstance;

  result =
      DirectInput8Create(w.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
                         (void **)&directInput, nullptr);
  assert(SUCCEEDED(result));
#pragma endregion

#pragma region キーボードの初期化
  IDirectInputDevice8 *keyboard = nullptr;

  result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
  assert(SUCCEEDED(result));

  result = keyboard->SetDataFormat(&c_dfDIKeyboard);
  assert(SUCCEEDED(result));

  result = keyboard->SetCooperativeLevel(
      hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
  assert(SUCCEEDED(result));

#pragma endregion

#pragma region 変数宣言
  Transform transform{
      {1.0f, 1.0f, 1.0f},
      {0.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 0.0f},

  };

  Transform transformSprite{
      {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  Transform cameraTransform{
      {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f}};

  Transform uvTransformSprite{
      {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
      0.45f, float(kCliantWidth) / float(kCliantHeight), 0.1f, 100.0f);

#pragma region ImGuiの初期化

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX12_Init(device.Get(), swapChainDesc.BufferCount, rtvDesc.Format,
                      srvDescriptorHeap.Get(),
                      srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                      srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

  // 三角形の初期値
  Vector4 triangleColor = {1.0f, 1.0f, 1.0f, 1.0f};

  Material *material = nullptr;

  bool useMonsterBall = false;

#pragma endregion

  SoundData soundData = SoundLoadWave("resource/Alarm01.wav");
  bool hasPlayed = false;

#pragma endregion
  MSG msg{};
  while (msg.message != WM_QUIT) {

    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      // ゲームの処理

      if (!hasPlayed) {
        SoundPlayWave(xAudio2.Get(), soundData);
        hasPlayed = true;
      }

#ifdef _DEBUG
      ImGui_ImplDX12_NewFrame();
      ImGui_ImplWin32_NewFrame();
      ImGui::NewFrame();

      ImGui::Begin("Settings");

      // === Triangle Color ===
      if (ImGui::CollapsingHeader("Triangle Color")) {
        ImGui::ColorEdit4("Color", reinterpret_cast<float *>(&triangleColor));
      }

      if (ImGui::CollapsingHeader("CameraTransform")) {
        ImGui::DragFloat3("CameraTranslate", &cameraTransform.translate.x,
                          0.1f);
        ImGui::SliderAngle("CameraRotateX", &cameraTransform.rotate.x);
        ImGui::SliderAngle("CameraRotateY", &cameraTransform.rotate.y);
        ImGui::SliderAngle("CameraRotateZ", &cameraTransform.rotate.z);
      }

      // === Transform (SRT Controller) ===
      if (ImGui::CollapsingHeader("SphereTransform")) {
        ImGui::SliderAngle("RotateX", &transform.rotate.x);
        ImGui::SliderAngle("RotateY", &transform.rotate.y);
        ImGui::SliderAngle("RotateZ", &transform.rotate.z);
        ImGui::Checkbox("useMonsterBall", &useMonsterBall);
        materialData->enableLighting = useMonsterBall;
      }

      // === Light Settings ===
      if (ImGui::CollapsingHeader("Light Settings")) {
        ImGui::Text("Directional Light");

        ImGui::ColorEdit4(
            "Color", reinterpret_cast<float *>(&directionalLightData->color));
        ImGui::SliderFloat3(
            "Direction",
            reinterpret_cast<float *>(&directionalLightData->direction), -1.0f,
            1.0f);

        // Normalize direction
        Vector3 &v = directionalLightData->direction;
        XMVECTOR dir = XMVectorSet(v.x, v.y, v.z, 0.0f);
        dir = XMVector3Normalize(dir);
        XMStoreFloat3(reinterpret_cast<XMFLOAT3 *>(&v), dir);

        ImGui::SliderFloat("Intensity", &directionalLightData->intensity, 0.0f,
                           10.0f);
      }

      // === Sprite Transform ===
      if (ImGui::CollapsingHeader("Sprite Transform")) {
        ImGui::DragFloat3("Scale##Sprite", &transformSprite.scale.x, 0.1f, 0.1f,
                          10.0f);
        ImGui::DragFloat3("Rotate (rad)##Sprite", &transformSprite.rotate.x,
                          0.01f, -3.14f, 3.14f);
        ImGui::DragFloat3("Translate##Sprite", &transformSprite.translate.x,
                          1.0f, -700.0f, 700.0f);
      }

      // === UV Transform Sprite ===
      if (ImGui::CollapsingHeader("UV Transform Sprite")) {
        ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f,
                          -10.0f, 10.0f);
        ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f,
                          10.0f);
        ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
      }

      ImGui::End();
      ImGui::Render();

#endif

      ///================================
      /// 更新処理
      /// ==============================

      Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);

      uvTransformMatrix = Multiply(
          uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
      uvTransformMatrix = Multiply(
          uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
      materialDataSprite->uvTransform = uvTransformMatrix;

      /// Sprite用のWorldViewProjectionMatrixを作る

      Matrix4x4 worldMatrixSprite =
          MakeAffineMatrix(transformSprite.scale, transformSprite.rotate,
                           transformSprite.translate);
      Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
      Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(
          0.0f, 0.0f, float(kCliantWidth), float(kCliantHeight), 0.0f, 100.0f);
      Matrix4x4 worldViewProjectionMatrixSprite =
          Multiply(projectionMatrixSprite,
                   Multiply(viewMatrixSprite, worldMatrixSprite));
      *transformationMatrixDataSprite = worldViewProjectionMatrixSprite;

#pragma region 三角形の回転

      Matrix4x4 worldMatrix = MakeAffineMatrix(
          transform.scale, transform.rotate, transform.translate);
      Matrix4x4 cameraMatrix =
          MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate,
                           cameraTransform.translate);
      Matrix4x4 viewMatrix = Inverse(cameraMatrix);
      Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(
          0.45f, float(kCliantWidth) / float(kCliantHeight), 0.1f, 100.0f);
      Matrix4x4 worldViewProjectionMatrix =
          Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));

      //     transform.rotate.y += 0.01f;
      *wvpData = {worldViewProjectionMatrix, worldMatrix};

#pragma endregion

      keyboard->Acquire();
      BYTE key[256] = {};
      keyboard->GetDeviceState(sizeof(key), key);
      if (key[DIK_SPACE]) {
        OutputDebugStringA("Hit space\n");
      }

      ///================================
      /// 描画処理
      ///================================

#pragma region 画面の色を変える
      // コマンドリストのリセット
      UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

#pragma region バリアを張る

      // TransitionBarrierを作る
      D3D12_RESOURCE_BARRIER barrier{};

      // 今回はTransition
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

      // Noneにしておく
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

      // バリアを張る対象のリソース。現在のバックバッファに対して行う
      barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();

      // 遷移前のResourceState
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;

      // 遷移後のResourceState
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
      // バリアを張るSubresourceIndex
      commandList.Get()->ResourceBarrier(1, &barrier);
#pragma endregion

#pragma region 実際の描画処理
      commandList.Get()->OMSetRenderTargets(1, &rtvHandles[backBufferIndex],
                                            false, &dsvHandle);
      // 指定した色で画面全体をクリアする
      float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f}; // ここで色を変える
      commandList.Get()->ClearRenderTargetView(rtvHandles[backBufferIndex],
                                               clearColor, 0, nullptr);

      commandList.Get()->ClearDepthStencilView(
          dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

      const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = {
          srvDescriptorHeap};
      commandList.Get()->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());

      commandList.Get()->RSSetViewports(1, &viewport);
      commandList.Get()->RSSetScissorRects(1, &scissorRect);
      commandList.Get()->SetGraphicsRootSignature(rootSignature.Get());
      commandList.Get()->SetPipelineState(graphicsPipelineState.Get());

      // 三角形の色を変える
      materialResource->Map(0, nullptr, reinterpret_cast<void **>(&material));
      material->color = triangleColor;
      materialResource->Unmap(0, nullptr);

      // 球の描画：Material, WVP, Light を正しくセット
      commandList.Get()->IASetVertexBuffers(0, 1, &vertexBufferView);
      commandList.Get()->IASetIndexBuffer(&indexBufferView);
      commandList.Get()->IASetPrimitiveTopology(
          D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      commandList.Get()->SetGraphicsRootConstantBufferView(
          0, materialResource->GetGPUVirtualAddress());
      commandList.Get()->SetGraphicsRootConstantBufferView(
          1, wvpResource->GetGPUVirtualAddress());
      commandList.Get()->SetGraphicsRootConstantBufferView(
          3, directionalLightResource->GetGPUVirtualAddress());
      commandList.Get()->SetGraphicsRootDescriptorTable(
          2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
      //     uint32_t indexCount = kSubdivision * kSubdivision * 6;

      commandList.Get()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0,
                                       0);

      // commandList->DrawIndexedInstanced(UINT(modelData.vertices.size()), 1,
      // 0,
      //                                   0, 0);

      commandList.Get()->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
      commandList.Get()->IASetIndexBuffer(&indexBufferViewSprite);
      commandList.Get()->IASetPrimitiveTopology(
          D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      commandList.Get()->SetGraphicsRootConstantBufferView(
          0, materialResourceSprite->GetGPUVirtualAddress());
      commandList.Get()->SetGraphicsRootConstantBufferView(
          1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
      commandList.Get()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

      //  commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

      ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

#pragma endregion

#pragma region バリアを張る

      // 今回はRenderTargetからPresentにする
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
      // バリアを張るSubresourceIndex
      commandList.Get()->ResourceBarrier(1, &barrier);

#pragma endregion

      hr = commandList.Get()->Close();
      // コマンドリストの生成に失敗したら起動しない
      assert(SUCCEEDED(hr));

      // コマンドをキックする
      const Microsoft::WRL::ComPtr<ID3D12CommandList> commandLists[] = {
          commandList};
      commandQueue.Get()->ExecuteCommandLists(1, commandLists->GetAddressOf());

      swapChain->Present(1, 0);

#pragma region フェンスの値を更新
      // フェンスの値を更新
      fenceValue++;
      // GPUがここまでたどり着いたときに,Fenceの値を指定した値に代入するようにSignalを送る
      commandQueue->Signal(fence.Get(), fenceValue);

      if (fence->GetCompletedValue() < fenceValue) {
        // GPUが指定した値にたどり着くまで待つ
        fence->SetEventOnCompletion(fenceValue, fenceEvent);

        // イベントを待つ
        WaitForSingleObject(fenceEvent, INFINITE);
      }
#pragma endregion
      hr = commandAllocator->Reset();
      assert(SUCCEEDED(hr));
      hr = commandList->Reset(commandAllocator.Get(), nullptr);
      assert(SUCCEEDED(hr));

#pragma endregion
    }
  }

#pragma region ImGuiの終了処理
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
#pragma endregion

  Log("unkillable demon king\n");

#pragma region エラー放置しない処理
#ifdef _DEBUG
  Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
  if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
    // やばいエラー時に止まる
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    // 警告時に止まる
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

    // 抑制するメッセージの設定
    D3D12_MESSAGE_ID denyIds[] = {
        D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE};

    // 抑制するレベル
    D3D12_MESSAGE_SEVERITY severrities[] = {D3D12_MESSAGE_SEVERITY_INFO};
    D3D12_INFO_QUEUE_FILTER filter{};
    filter.DenyList.NumIDs = _countof(denyIds);
    filter.DenyList.pIDList = denyIds;
    filter.DenyList.NumSeverities = _countof(severrities);
    filter.DenyList.pSeverityList = severrities;

    // 指定したメッセージの表示を抑制する
    infoQueue->PushStorageFilter(&filter);
  }

#endif
#pragma endregion

  xAudio2.Reset();
  SoundUnload(&soundData);

  CloseHandle(fenceEvent);
  CloseWindow(hwnd);

  CoUninitialize();

  return 0;
}