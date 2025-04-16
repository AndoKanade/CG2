#include <Windows.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <string.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
  case WM_DESTROY:

    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

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

void Log(std::ostream &os, const std::string &message) {
  os << message << std::endl;
  OutputDebugStringA(message.c_str());
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
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

  std::filesystem::create_directory("logs");

  MSG msg{};
  while (msg.message != WM_QUIT) {

    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      // ゲームの処理
    }
  }

  OutputDebugStringA("Hello,DirectX!\n");
  return 0;
}
