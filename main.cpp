#include <Windows.h>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <string.h>
#include <strsafe.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
  case WM_DESTROY:

    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

typedef struct Vector4 {
  float w, x, y, z;
} Vector4;

#pragma region 関数たち

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

IDxcBlob *CompileShader(
    // CompilerするShaderファイルへのパス
    const std::wstring &filePath,
    // Compilerに使用するProfile
    const wchar_t *profile,
    // 初期化で生成したもの3つ
    IDxcUtils *dxcUtils, IDxcCompiler3 *dxCompiler,
    IDxcIncludeHandler *includeHandler) {

#pragma region HLSLファイルを読み込む

  Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n",
                                filePath, profile)));

  IDxcBlobEncoding *shaderSource = nullptr;
  HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);

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
  IDxcResult *shaderResult = nullptr;
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

  IDxcBlobUtf8 *shaderError = nullptr;
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

  // もういらないので解放する
  shaderSource->Release();
  shaderResult->Release();

  // コンパイル結果を返す
  return shaderBlob;

#pragma endregion
}

#pragma endregion

#pragma endregion

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

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

  ID3D12Debug1 *debugController = nullptr;
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
  IDXGIFactory7 *dxgiFactory = nullptr;

  HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

  assert(SUCCEEDED(hr));

#pragma endregion

#pragma region 使用するGPUを決める
  // 使用するアダプターの変数、最初はnullptrを入れておく
  IDXGIAdapter4 *useAdapter = nullptr;

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
  ID3D12Device *device = nullptr;
  // 機能レベルとログ出力用の文字列
  D3D_FEATURE_LEVEL featureLevels[] = {

      D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};

  const char *featureLevelStrings[] = {"12.2", "12.1", "12.0"};

  // 高い順に生成できるか試していく
  for (size_t i = 0; i < _countof(featureLevels); i++) {
    hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));

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

  ID3D12CommandQueue *commandQueue = nullptr;
  D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
  hr = device->CreateCommandQueue(&commandQueueDesc,
                                  IID_PPV_ARGS(&commandQueue));

  // コマンドキューの生成に失敗したら起動しない
  assert(SUCCEEDED(hr));

  // コマンドアロケータの生成
  ID3D12CommandAllocator *commandAllocator = nullptr;
  hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      IID_PPV_ARGS(&commandAllocator));

  // コマンドリストを生成する
  ID3D12GraphicsCommandList *commandList = nullptr;
  hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                 commandAllocator, nullptr,
                                 IID_PPV_ARGS(&commandList));
  // コマンドリストの生成に失敗したら起動しない
  assert(SUCCEEDED(hr));

  IDXGISwapChain4 *swapChain = nullptr;
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
      commandQueue, hwnd, &swapChainDesc, nullptr, nullptr,
      reinterpret_cast<IDXGISwapChain1 **>(&swapChain));
  // スワップチェインの生成に失敗したら起動しない
  assert(SUCCEEDED(hr));

#pragma endregion

#pragma region DescriptorHeapの作成

  // ディスクリプタヒープの生成
  ID3D12DescriptorHeap *rtvDescriptorHeap = nullptr;
  D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};

  rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvDescriptorHeapDesc.NumDescriptors = 2;
  hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc,
                                    IID_PPV_ARGS(&rtvDescriptorHeap));
  // ディスクリプタヒープの生成に失敗したら起動しない
  assert(SUCCEEDED(hr));

  // swapChainからResourceを取得する
  ID3D12Resource *swapChainResources[2] = {nullptr};
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
  device->CreateRenderTargetView(swapChainResources[0], &rtvDesc,
                                 rtvHandles[0]);
  // 2つめを作る
  rtvHandles[1].ptr =
      rtvHandles[0].ptr +
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  device->CreateRenderTargetView(swapChainResources[1], &rtvDesc,
                                 rtvHandles[1]);

#pragma endregion

#pragma region FenceとEventの作成

  ID3D12Fence *fence = nullptr;
  uint64_t fenceValue = 0;
  hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE,
                           IID_PPV_ARGS(&fence));
  assert(SUCCEEDED(hr));

  HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  assert(fenceEvent != nullptr);
#pragma endregion

#pragma region DXCの初期化

  IDxcUtils *dxcUtils = nullptr;
  IDxcCompiler3 *dxcCompiler = nullptr;

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

  ID3DBlob *signatureBlob = nullptr;
  ID3DBlob *errorBlob = nullptr;
  hr = D3D12SerializeRootSignature(&descriptionRootSignature,
                                   D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob,
                                   &errorBlob);

  if (FAILED(hr)) {
    Log(reinterpret_cast<char *>(errorBlob->GetBufferPointer()));
    assert(false);
  }

  ID3D12RootSignature *rootSignature = nullptr;
  hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
                                   signatureBlob->GetBufferSize(),
                                   IID_PPV_ARGS(&rootSignature));
  assert(SUCCEEDED(hr));

#pragma endregion

#pragma region InputLayoutを生成する

  D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
  inputElementDescs[0].SemanticName = "POSITION";
  inputElementDescs[0].SemanticIndex = 0;
  inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
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

  IDxcBlob *vertexShaderBlob = CompileShader(
      L"Object3D.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
  assert(vertexShaderBlob != nullptr);

  IDxcBlob *pixelShaderBlob = CompileShader(
      L"Object3D.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
  assert(pixelShaderBlob != nullptr);

#pragma endregion

#pragma region PSOを生成する

  D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
  graphicsPipelineStateDesc.pRootSignature = rootSignature;
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

  // 利用する形状のタイプ(今回は三角形)
  graphicsPipelineStateDesc.PrimitiveTopologyType =
      D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  // どのような画面に色を打ち込むかの設定
  graphicsPipelineStateDesc.SampleDesc.Count = 1;
  graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
  // 実際に生成
  ID3D12PipelineState *graphicsPipelineState = nullptr;
  hr = device->CreateGraphicsPipelineState(
      &graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
  assert(SUCCEEDED(hr));

#pragma endregion

#pragma region VertexResourceを生成する

  D3D12_HEAP_PROPERTIES uploadHeapProperties{};
  uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC vertexResourceDesc{};

  vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  vertexResourceDesc.Width = sizeof(Vector4) * 3;
  vertexResourceDesc.Height = 1;
  vertexResourceDesc.DepthOrArraySize = 1;
  vertexResourceDesc.MipLevels = 1;
  vertexResourceDesc.SampleDesc.Count = 1;

  vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  ID3D12Resource *vertexResource = nullptr;
  hr = device->CreateCommittedResource(
      &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&vertexResource));
  assert(SUCCEEDED(hr));

#pragma endregion
#pragma region VertexBufferViewを生成する

  D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
  vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();

  vertexBufferView.SizeInBytes = sizeof(Vector4) * 3;

  vertexBufferView.StrideInBytes = sizeof(Vector4);

#pragma endregion

#pragma region resourceにデータを書き込む
  Vector4 *vertexDate = nullptr;

  vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexDate));

  vertexDate[0] = {-0.5f, -0.5f, 0.0f, 1.0f};
  vertexDate[1] = {0.0f, 0.5f, 0.0f, 1.0f};
  vertexDate[2] = {0.5f, -0.5f, 0.0f, 1.0f};

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

  MSG msg{};
  while (msg.message != WM_QUIT) {

    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      // ゲームの処理

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
      barrier.Transition.pResource = swapChainResources[backBufferIndex];

      // 遷移前のResourceState
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;

      // 遷移後のResourceState
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
      // バリアを張るSubresourceIndex
      commandList->ResourceBarrier(1, &barrier);
#pragma endregion

      commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false,
                                      nullptr);
      // 指定した色で画面全体をクリアする
      float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f}; // ここで色を変える
      commandList->ClearRenderTargetView(rtvHandles[backBufferIndex],
                                         clearColor, 0, nullptr);

      commandList->RSSetViewports(1, &viewport);
      commandList->RSSetScissorRects(1, &scissorRect);
      commandList->SetGraphicsRootSignature(rootSignature);
      commandList->SetPipelineState(graphicsPipelineState);
      commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
      commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      commandList->DrawInstanced(3, 1, 0, 0);

#pragma region バリアを張る

      // 今回はRenderTargetからPresentにする
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
      // バリアを張るSubresourceIndex
      commandList->ResourceBarrier(1, &barrier);

#pragma endregion

      hr = commandList->Close();
      // コマンドリストの生成に失敗したら起動しない
      assert(SUCCEEDED(hr));

      // コマンドをキックする
      ID3D12CommandList *commandLists[] = {commandList};
      commandQueue->ExecuteCommandLists(1, commandLists);

      swapChain->Present(1, 0);

#pragma region フェンスの値を更新
      // フェンスの値を更新
      fenceValue++;
      // GPUがここまでたどり着いたときに,Fenceの値を指定した値に代入するようにSignalを送る
      commandQueue->Signal(fence, fenceValue);

      if (fence->GetCompletedValue() < fenceValue) {
        // GPUが指定した値にたどり着くまで待つ
        fence->SetEventOnCompletion(fenceValue, fenceEvent);

        // イベントを待つ
        WaitForSingleObject(fenceEvent, INFINITE);
      }
#pragma endregion

      hr = commandAllocator->Reset();
      assert(SUCCEEDED(hr));
      hr = commandList->Reset(commandAllocator, nullptr);
      assert(SUCCEEDED(hr));
#pragma endregion
    }
  }

  Log("Hello,DirectX!\n");

#pragma region エラー放置しない処理
#ifdef _DEBUG
  ID3D12InfoQueue *infoQueue = nullptr;
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

    // 解放
    infoQueue->Release();
  }

#endif
#pragma endregion

#pragma region 解放処理

  // 生成と逆の順番で解放する

  vertexResource->Release();
  graphicsPipelineState->Release();
  signatureBlob->Release();
  if (errorBlob) {
    errorBlob->Release();
  }
  rootSignature->Release();
  pixelShaderBlob->Release();
  vertexShaderBlob->Release();
  CloseHandle(fenceEvent);
  fence->Release();
  rtvDescriptorHeap->Release();
  swapChainResources[0]->Release();
  swapChainResources[1]->Release();
  swapChain->Release();
  commandList->Release();
  commandAllocator->Release();
  commandQueue->Release();
  device->Release();
  useAdapter->Release();
  dxgiFactory->Release();

#ifdef _DEBUG

  debugController->Release();
#endif
  CloseWindow(hwnd);

#pragma endregion

#pragma region 解放しているかの確認
  IDXGIDebug1 *debug;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
    debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
    debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
    debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
    debug->Release();
  }
#pragma endregion

  return 0;
}
