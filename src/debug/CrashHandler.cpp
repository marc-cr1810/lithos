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
#include <cstdio> // for snprintf (risky but better than stringstream)
#include <cstring>
#include <fcntl.h>

void CrashHandler::LinuxSignalHandler(int signal) {
  time_t now = time(nullptr);
  struct tm tstruct;
  localtime_r(&now, &tstruct);

  char filename[64];
  strftime(filename, sizeof(filename), "crash_report_%Y%m%d_%H%M%S.txt",
           &tstruct);

  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd >= 0) {
    const char *header = "CRASH REPORT\n============\nTime: ";
    write(fd, header, strlen(header));

    char timeBuf[64];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S\n", &tstruct);
    write(fd, timeBuf, strlen(timeBuf));

    const char *sigMsg = "Signal Received: ";
    write(fd, sigMsg, strlen(sigMsg));

    char sigNum[16];
    // Simple integer to string conversion
    int n = signal;
    int i = 0;
    if (n == 0)
      sigNum[i++] = '0';
    else {
      char temp[16];
      int j = 0;
      while (n > 0) {
        temp[j++] = (n % 10) + '0';
        n /= 10;
      }
      while (j > 0)
        sigNum[i++] = temp[--j];
    }
    sigNum[i++] = '\n';
    write(fd, sigNum, i);

    const char *stHeader = "\nStack Trace:\n";
    write(fd, stHeader, strlen(stHeader));

    void *array[50];
    int size = backtrace(array, 50);
    backtrace_symbols_fd(array, size, fd);

    close(fd);
  }

  // Use Logger as requested. Caution: Not async-signal-safe, risking deadlock
  // if crash was in logging/allocator.
  LOG_CRITICAL("Crash detected! Report written to {}", filename);

  _exit(1);
}
#endif
