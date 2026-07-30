#pragma once

class Logger
{
public:
	static void Initialize(const char* fileName);
	static void LogMessage(const char* message, ...);
	static void Shutdown();
};
