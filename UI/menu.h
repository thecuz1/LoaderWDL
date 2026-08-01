#pragma once

#include "Main.h"

#include <minwindef.h>

DWORD MenuThread(Main* main);
void unhookAll();
void imguiInit();

inline BOOL activelyHooked = false;
