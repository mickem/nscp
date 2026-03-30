//
// Created by micha on 2026-03-30.
//

#include "tick_count.h"

#include <tchar.h>

typedef ULONGLONG (*tGetTickCount64)();

tGetTickCount64 pGetTickCount64 = nullptr;

ULONGLONG nscpGetTickCount64() {
  if (pGetTickCount64 == nullptr) {
    const HMODULE hMod = ::LoadLibrary(_TEXT("kernel32"));
    if (hMod == nullptr) return 0;
    pGetTickCount64 = reinterpret_cast<tGetTickCount64>(GetProcAddress(hMod, "GetTickCount64"));
    if (pGetTickCount64 == nullptr) return 0;
  }
  return pGetTickCount64();
}