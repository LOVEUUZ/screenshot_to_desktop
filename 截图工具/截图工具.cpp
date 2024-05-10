#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <Windows.h>
#include <filesystem>
#include <future>
#include <thread>

#include "shlobj.h"

// #pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup" )   //不显示控制台


HHOOK hook;


LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
std::wstring     getDesktopPath();
void             CaptureScreen(const std::wstring & filePath);
std::wstring     stringToWstring(const std::string & str);
void             task();

int main() {
  // 注册键盘钩子
  hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);

  // 消息循环
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // 卸载钩子
  UnhookWindowsHookEx(hook);
  return 0;
}


// 键盘事件处理函数
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode >= 0) {
    // 提取键盘事件信息
    KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam;

    static int keyDownCount = 0;
    if (kbdStruct->vkCode == VK_OEM_3) {
      if (wParam == WM_KEYDOWN) {
        // 处理按下事件
        keyDownCount++;

        //0.3s的中断检测
        std::thread([&]() {
          std::this_thread::sleep_for(std::chrono::milliseconds(300));
          keyDownCount = 0;
        }).detach();

        if (keyDownCount == 2) {
          task();
          keyDownCount = 0;
        }
      }
    }
    else {
      keyDownCount = 0;
    }
  }
  // 继续传递事件
  return CallNextHookEx(hook, nCode, wParam, lParam);
}

void task() {
  // 获取当前时间点
  auto now = std::chrono::system_clock::now();

  // 将时间点转换为时间结构体
  time_t    currentTime = std::chrono::system_clock::to_time_t(now);
  struct tm timeinfo;
  localtime_s(&timeinfo, &currentTime);
  // 格式化日期
  std::stringstream ss;
  ss << std::put_time(&timeinfo, "%Y_%m_%d");

  // 将日期存储到字符串中
  std::string dateString = ss.str();

  static int  i        = 1;
  std::string fileName = dateString + "_" + std::to_string(i++) + ".bmp";

  std::wstring path = getDesktopPath();
  // 将 std::string 转换为 std::wstring
  std::wstring wFileName = stringToWstring(fileName);
  path += L"\\" + wFileName;

  //判断该文件存不存在
  while (std::filesystem::exists(path)) {
    fileName  = dateString + "_" + std::to_string(i++) + ".bmp";
    wFileName = stringToWstring(fileName);
    path      = getDesktopPath() + L"\\" + wFileName;
  }
  std::wcout << path << "\n";

  CaptureScreen(path);
}

//获取桌面路径
std::wstring getDesktopPath() {
  wchar_t desktopPath[MAX_PATH];
  if (SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath) != S_OK) {
    return L"";
  }
  return desktopPath;
}

void CaptureScreen(const std::wstring & filePath) {
  // 获取屏幕尺寸
  int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
  int screenHeight = GetSystemMetrics(SM_CYSCREEN);

  // 创建设备上下文
  HDC hScreenDC = GetDC(NULL);
  HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

  // 创建位图
  HBITMAP hBitmap   = CreateCompatibleBitmap(hScreenDC, screenWidth, screenHeight);
  HGDIOBJ oldBitmap = SelectObject(hMemoryDC, hBitmap);

  // 拷贝屏幕内容到位图
  BitBlt(hMemoryDC, 0, 0, screenWidth, screenHeight, hScreenDC, 0, 0, SRCCOPY);

  // 保存位图为 BMP 文件
  BITMAPINFOHEADER bi;
  bi.biSize          = sizeof(BITMAPINFOHEADER);
  bi.biWidth         = screenWidth;
  bi.biHeight        = screenHeight;
  bi.biPlanes        = 1;
  bi.biBitCount      = 24; // 24位色深
  bi.biCompression   = BI_RGB;
  bi.biSizeImage     = 0;
  bi.biXPelsPerMeter = 0;
  bi.biYPelsPerMeter = 0;
  bi.biClrUsed       = 0;
  bi.biClrImportant  = 0;

  std::wstring desktopPath = getDesktopPath();
  if (desktopPath.empty()) {
    // sendToWindows();
    return;
  }

  // 保存位图到文件
  HANDLE hFile = CreateFile(filePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    return;
  }

  BITMAPFILEHEADER bmfh;
  DWORD            dwWritten = 0;
  bmfh.bfType                = 0x4D42; // "BM"
  bmfh.bfSize                = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + screenWidth * screenHeight * 3;
  bmfh.bfReserved1           = 0;
  bmfh.bfReserved2           = 0;
  bmfh.bfOffBits             = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  WriteFile(hFile, &bmfh, sizeof(BITMAPFILEHEADER), &dwWritten, NULL);
  WriteFile(hFile, &bi, sizeof(BITMAPINFOHEADER), &dwWritten, NULL);
  BYTE* pBuffer = new BYTE[screenWidth * screenHeight * 3];
  GetDIBits(hMemoryDC, hBitmap, 0, screenHeight, pBuffer, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
  WriteFile(hFile, pBuffer, screenWidth * screenHeight * 3, &dwWritten, NULL);
  delete[] pBuffer;
  CloseHandle(hFile);

  // 清理资源
  SelectObject(hMemoryDC, oldBitmap);
  DeleteObject(hBitmap);
  DeleteDC(hMemoryDC);
  ReleaseDC(NULL, hScreenDC);
}

std::wstring stringToWstring(const std::string & str) {
  std::wstring wstr(str.length(), L' ');
  std::copy(str.begin(), str.end(), wstr.begin());
  return wstr;
}
