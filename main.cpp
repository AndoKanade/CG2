#include <Windows.h>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxgi1_6.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <string.h>
#include <strsafe.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
  case WM_DESTROY:

    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

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
void Log(std::ostream &os, const std::string &message) {
  os << message << std::endl;
  OutputDebugStringA(message.c_str());
}

void log(const std::string &message) { OutputDebugStringA(message.c_str()); }
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
  log("Complete create D3D12Device!!\n");

#pragma endregion

  MSG msg{};
  while (msg.message != WM_QUIT) {

    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      // ゲームの処理

    }
  }

  log("Hello,DirectX!\n");
  return 0;
}
