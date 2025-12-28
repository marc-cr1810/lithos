#pragma once

#include <string>

class CrashHandler {
public:
  static void Init();

private:
  static void WriteCrashReport(const std::string &report);

#ifdef _WIN32
  static long __stdcall
  WindowsExceptionFilter(struct _EXCEPTION_POINTERS *exceptionInfo);
#else
  static void LinuxSignalHandler(int signal);
#endif
};
