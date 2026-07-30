#pragma once
#include "Main.h"

void MenuThread(Main* main);
void unhookAll();
void imguiInit();

inline BOOL activelyHooked = false;
