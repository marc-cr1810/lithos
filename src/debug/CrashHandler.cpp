#ifdef _WIN32
#include <windows.h>
#else
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

#endif

#include "CrashHandler.h"
#include "Logger.h"
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

void CrashHandler::Init() {
#ifdef _WIN32
  SetUnhandledExceptionFilter(WindowsExceptionFilter);
#else
  struct sigaction sa;
  sa.sa_handler = LinuxSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
#endif
  LOG_INFO("Crash Handler Initialized.");
}

void CrashHandler::WriteCrashReport(const std::string &report) {
  std::time_t now = std::time(nullptr);
  char filename[64];
  std::strftime(filename, sizeof(filename), "crash_report_%Y%m%d_%H%M%S.txt",
                std::localtime(&now));

  std::ofstream file(filename);
  if (file.is_open()) {
    file << "CRASH REPORT\n";
    file << "============\n";
    char timeStr[64];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S",
                  std::localtime(&now));
    file << "Time: " << timeStr << "\n\n";
    file << report << "\n";
    file.close();

    LOG_ERROR("Crash Report written to {}", filename);
  } else {
    LOG_ERROR("Failed to write crash report.");
  }
}

#ifdef _WIN32
long __stdcall CrashHandler::WindowsExceptionFilter(
    struct _EXCEPTION_POINTERS *exceptionInfo) {
  std::stringstream report;
  report << "Exception Code: 0x" << std::hex
         << exceptionInfo->ExceptionRecord->ExceptionCode << "\n";
  report << "Exception Address: 0x"
         << exceptionInfo->ExceptionRecord->ExceptionAddress << "\n";

  report << "\nStack trace generation disabled due to persistent compilation "
            "conflicts with DbgHelp.h.\n";
  report << "The Exception Address can be used with a debugger or map file to "
            "locate the crash.\n";

  WriteCrashReport(report.str());

  return EXCEPTION_EXECUTE_HANDLER; // Terminate mechanism
}

#else
void CrashHandler::LinuxSignalHandler(int signal) {
  std::stringstream report;
  report << "Signal Received: " << signal << "\n";

  void *array[20];
  int size = backtrace(array, 20);
  char **messages = backtrace_symbols(array, size);

  report << "\nStack Trace:\n";
  for (int i = 0; i < size && messages != NULL; ++i) {
    report << messages[i] << "\n";
  }
  free(messages);

  WriteCrashReport(report.str());

  exit(1);
}
#endif
