// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __STOUT_WINDOWS_OS_HPP__
#define __STOUT_WINDOWS_OS_HPP__

#include <direct.h>
#include <io.h>
#include <psapi.h>

#include <sys/utime.h>

#include <list>
#include <map>
#include <set>
#include <string>

#include <stout/duration.hpp>
#include <stout/none.hpp>
#include <stout/nothing.hpp>
#include <stout/option.hpp>
#include <stout/path.hpp>
#include <stout/try.hpp>
#include <stout/windows.hpp>

#include <stout/os/raw/environment.hpp>
#include <stout/os/os.hpp>
#include <stout/os/config.hpp>

#define WNOHANG 0
#define hstrerror() ("")
#define SIGPIPE 100

namespace os {

inline Try<std::set<pid_t>> pids()
{
    // Windows does not have the concept of a process group, so we need to
    // enumerate all processes.
    //
    // The list of processes might differ between calls, so continue calling
    // `EnumProcesses` until the output buffer is large enough. The call is
    // considered to fully succeed when the function returns non-zero and the
    // number of bytes returned is less than the size of the `pids` array. If
    // that's not the case, then we need to increase the size of the `pids` array
    // and attempt the call again.
    //
    // To minimize the number of calls (at the expense
    // or memory), we choose to allocate double the amount suggested by
    // `EnumProcesses`.
    DWORD *pids = NULL;
    DWORD bytes = 1024;
    DWORD pids_size = 0;

    // Double the size of the `pids` array. Note that the magic constant `2`
    // below actually means 'double the size' and is not meant as a poor
    // substitute for 'sizeof(something)'.
    while (pids_size <= bytes) {
        pids_size = 2 * bytes;
        DWORD *reallocated_pids = (DWORD *)realloc(pids, pids_size);

        if (reallocated_pids == NULL) {
            if (pids != NULL) {
                free(pids);

            }
            return WindowsError("os::pids(): Failed to allocate memory");
        }

        pids = reallocated_pids;

        if (!::EnumProcesses(pids, pids_size, &bytes)) {
            free(pids);
            return WindowsError("os::pids(): Failed to call EnumProcesses");
        }
    }

    std::set<pid_t> result;
    for (DWORD i = 0; i < bytes / sizeof(DWORD); i++) {
        result.insert(pids[i]);
    }

    free(pids);
    return result;
}

inline Result<Process> process(pid_t pid)
{
    // Open the process
    HANDLE hProcess;
    hProcess = ::OpenProcess(
        PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
        FALSE,
        static_cast<DWORD>(pid));

    // Error out if not able to open
    if (hProcess == NULL)
    {
        return WindowsError("os::process(): Failed to call OpenProcess");
    }

    std::shared_ptr<void> hSafeProcess(hProcess, ::CloseHandle);

    // Retrieve the memory stats for the process
    PROCESS_MEMORY_COUNTERS counters;
    if (!::GetProcessMemoryInfo(
        hSafeProcess.get(),
        &counters,
        sizeof(PROCESS_MEMORY_COUNTERS)))
    {
        return WindowsError("os::process(): Failed to call GetProcessMemoryInfo");
    }

    // Retrieve the cpu usage for the process
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (!::GetProcessTimes(
        hSafeProcess.get(),
        &creationTime,
        &exitTime,
        &kernelTime,
        &userTime))
    {
        return WindowsError("os::process(): Failed to call GetProcessTimes");
    }

    LARGE_INTEGER lKernelTime, lUserTime; // in 100 nanoseconds
    lKernelTime.HighPart = kernelTime.dwHighDateTime;
    lKernelTime.LowPart = kernelTime.dwLowDateTime;
    lUserTime.HighPart = userTime.dwHighDateTime;
    lUserTime.LowPart = userTime.dwLowDateTime;

    double dKernelTime, dUserTime; // in seconds
    dKernelTime = lKernelTime.QuadPart / 10000000;
    dUserTime = lUserTime.QuadPart / 10000000;

    return Process(pid,                      // process id
        0,                                   // parent process id
        0,                                   // process group
        0,                                   // session id
        Bytes(counters.WorkingSetSize),      // working set in bytes
        Duration::create(dUserTime).get(),   // user time in seconds
        Duration::create(dKernelTime).get(), // kernel time in seconds
        "",                                  // command line
        false);                              // zombie?
}

inline Try<std::list<Process>> processes()
{
    const Try<std::set<pid_t>> pids = os::pids();

    if (pids.isError()) {
        return Error(pids.error());
    }

    std::list<Process> result;
    foreach(pid_t pid, pids.get()) {
        const Result<Process> process = os::process(pid);

        // Ignore any processes that disappear.
        if (process.isSome()) {
            result.push_back(process.get());
        }
    }
    return result;
}

inline Option<Process> process(
    pid_t pid,
    const std::list<Process>& processes)
{
  foreach(const Process& process, processes) {
    if (process.pid == pid) {
      return process;
    }
  }
  return None();
}

inline std::set<pid_t> children(
    pid_t pid,
    const std::list<Process>& processes,
    bool recursive = true)
{
  // Perform a breadth first search for descendants.
  std::set<pid_t> descendants;
  std::queue<pid_t> parents;
  parents.push(pid);
  do {
    pid_t parent = parents.front();
    parents.pop();

    // Search for children of parent.
    foreach(const Process& process, processes) {
      if (process.parent == parent) {
      // Have we seen this child yet?
        if (descendants.insert(process.pid).second) {
          parents.push(process.pid);
        }
      }
    }
  } while (recursive && !parents.empty());

  return descendants;
}

inline Try<std::set<pid_t> > children(pid_t pid, bool recursive = true)
{
  const Try<std::list<Process>> processes = os::processes();

  if (processes.isError()) {
    return Error(processes.error());
  }

  return children(pid, processes.get(), recursive);
}

inline int pagesize()
{
  SYSTEM_INFO si = {0};
  GetSystemInfo(&si);
  return si.dwPageSize;
};

inline long cpu()
{
  return 4;
};

// Sets the value associated with the specified key in the set of
// environment variables.
inline void setenv(const std::string& key,
                   const std::string& value,
                   bool overwrite = true)
{
  // Do not set the variable if already set and `overwrite` was not specified.
  if (!overwrite) {
    const DWORD bytes = ::GetEnvironmentVariable(key.c_str(), NULL, 0);
    const DWORD result = ::GetLastError();

    // Per MSDN[1], `GetEnvironmentVariable` returns 0 on error and sets the
    // error code to `ERROR_ENVVAR_NOT_FOUND` if the variable was not found.
    //
    // [1] https://msdn.microsoft.com/en-us/library/windows/desktop/ms683188(v=vs.85).aspx
    if (bytes != 0 || result != ERROR_ENVVAR_NOT_FOUND) {
      return;
    }
  }

  // `SetEnvironmentVariable` returns an error code, but we can't act on it.
  ::SetEnvironmentVariable(key.c_str(), value.c_str());
}


// Unsets the value associated with the specified key in the set of
// environment variables.
inline void unsetenv(const std::string& key)
{
  ::SetEnvironmentVariable(key.c_str(), NULL);
}

/*
// Executes a command by calling "/bin/sh -c <command>", and returns
// after the command has been completed. Returns 0 if succeeds, and
// return -1 on error (e.g., fork/exec/waitpid failed). This function
// is async signal safe. We return int instead of returning a Try
// because Try involves 'new', which is not async signal safe.
inline int system(const std::string& command)
{
  UNIMPLEMENTED;
}


// This function is a portable version of execvpe ('p' means searching
// executable from PATH and 'e' means setting environments). We add
// this function because it is not available on all systems.
//
// NOTE: This function is not thread safe. It is supposed to be used
// only after fork (when there is only one thread). This function is
// async signal safe.
inline int execvpe(const char* file, char** argv, char** envp)
{
  UNIMPLEMENTED;
}


inline Try<Nothing> chown(
    uid_t uid,
    gid_t gid,
    const std::string& path,
    bool recursive)
{
  UNIMPLEMENTED;
}


inline Try<Nothing> chmod(const std::string& path, int mode)
{
  UNIMPLEMENTED;
}


inline Try<Nothing> chroot(const std::string& directory)
{
  UNIMPLEMENTED;
}


inline Try<Nothing> mknod(
    const std::string& path,
    mode_t mode,
    dev_t dev)
{
  UNIMPLEMENTED;
}


inline Result<uid_t> getuid(const Option<std::string>& user = None())
{
  UNIMPLEMENTED;
}


inline Result<gid_t> getgid(const Option<std::string>& user = None())
{
  UNIMPLEMENTED;
}


inline Try<Nothing> su(const std::string& user)
{
  UNIMPLEMENTED;
}


inline Result<std::string> user(Option<uid_t> uid = None())
{
  UNIMPLEMENTED;
}
// Returns the list of files that match the given (shell) pattern.
inline Try<std::list<std::string>> glob(const std::string& pattern)
{
UNIMPLEMENTED;
}
*/

// Suspends execution for the given duration.
inline Try<Nothing> sleep(const Duration& duration)
{
  DWORD milliseconds = static_cast<DWORD>(duration.ms());
  ::Sleep(milliseconds);

  return Nothing();
}


// Returns the total number of cpus (cores).
inline Try<long> cpus()
{
  SYSTEM_INFO sysInfo;
  ::GetSystemInfo(&sysInfo);
  return static_cast<long>(sysInfo.dwNumberOfProcessors);
}

// Returns load struct with average system loads for the last
// 1, 5 and 15 minutes respectively.
// Load values should be interpreted as usual average loads from
// uptime(1).
inline Try<Load> loadavg()
{
  return Load();
}


// Returns the total size of main and free memory.
inline Try<Memory> memory()
{
  Memory memory;

  MEMORYSTATUSEX memory_status;
  memory_status.dwLength = sizeof(MEMORYSTATUSEX);
  if (!::GlobalMemoryStatusEx(&memory_status)) {
    return WindowsError("memory(): Could not call GlobalMemoryStatusEx");
  }

  memory.total = Bytes(memory_status.ullTotalPhys);
  memory.free = Bytes(memory_status.ullAvailPhys);
  memory.totalSwap = Bytes(memory_status.ullTotalPageFile);
  memory.freeSwap = Bytes(memory_status.ullAvailPageFile);

  return memory;
}


// Return the system information.
inline Try<UTSInfo> uname()
{
  UTSInfo info;

  OSVERSIONINFOEX os_version;
  os_version.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  if (!::GetVersionEx((LPOSVERSIONINFO)&os_version)) {
    return WindowsError("os::uname(): Failed to call GetVersionEx");
  }

  switch (os_version.wProductType) {
  case VER_NT_DOMAIN_CONTROLLER:
  case VER_NT_SERVER:
    info.sysname = "Windows Server";
    break;
  default:
    info.sysname = "Windows";
  }

  info.release = std::to_string(os_version.dwMajorVersion) + "." +
    std::to_string(os_version.dwMinorVersion);
  info.version = std::to_string(os_version.dwBuildNumber);
  if (os_version.szCSDVersion[0] != '\0') {
    info.version.append(" ");
    info.version.append(os_version.szCSDVersion);
  }

  // Get DNS name of the local computer. First, find the size of the output
  // buffer.
  DWORD size = 0;
  if (!::GetComputerNameEx(ComputerNameDnsHostname, NULL, &size) &&
    ::GetLastError() != ERROR_MORE_DATA) {
    return WindowsError("os::uname(): Failed to call GetComputerNameEx");
  }

  std::shared_ptr<char> computer_name(
    (char *)malloc((size + 1) * sizeof(char)));

  if (!::GetComputerNameEx(ComputerNameDnsHostname, computer_name.get(),
    &size)) {
    return WindowsError("os::uname(): Failed to call GetComputerNameEx");
  }

  info.nodename = computer_name.get();

  // Get OS architecture
  SYSTEM_INFO system_info;
  ::GetNativeSystemInfo(&system_info);
  switch (system_info.wProcessorArchitecture) {
  case PROCESSOR_ARCHITECTURE_AMD64:
    info.machine = "AMD64";
    break;
  case PROCESSOR_ARCHITECTURE_ARM:
    info.machine = "ARM";
    break;
  case PROCESSOR_ARCHITECTURE_IA64:
    info.machine = "IA64";
    break;
  case PROCESSOR_ARCHITECTURE_INTEL:
    info.machine = "x86";
    break;
  default:
    info.machine = "Unknown";
  }

  return info;
}


inline size_t recv(int sockfd, void *buf, size_t len, int flags) {
  return ::recv(sockfd, (char*)buf, len, flags);
}

inline int setsockopt(int socket, int level, int option_name,
       const void *option_value, socklen_t option_len) {
  return ::setsockopt(socket, level, option_name, (const char*)option_value, option_len);
}

inline int getsockopt(int socket, int level, int option_name,
  void* option_value, socklen_t* option_len) {
  return ::getsockopt(socket, level, option_name, (char*)option_value, option_len);
}

// Looks in the environment variables for the specified key and
// returns a string representation of its value. If no environment
// variable matching key is found, None() is returned.
inline Option<std::string> getenv(const std::string& key)
{
  char* value = ::getenv(key.c_str());

  if (value == NULL) {
    return None();
  }

  return std::string(value);
}

#ifdef _WINDOWS_

inline struct tm* gmtime_r(const time_t *timep, struct tm *result)
{
  if (gmtime_s(result, timep))
  {
    return result;
  }

  return NULL;
}

#else // ​_WINDOWS_

inline auto gmtime_r(const time_t *timep, struct tm *result) {
  return ::gmtime_r(timep, result);
}

#endif // ​_WINDOWS_

inline pid_t waitpid(pid_t pid, int *status, int options)
{
    return 0;
}

namespace libraries {

  // Returns the full library name by adding prefix and extension to
  // library name.
  inline std::string expandName(const std::string& libraryName)
  {
    const char* prefix = "lib";
    const char* extension =
#ifdef __APPLE__
      ".dylib";
#else
      ".so";
#endif

    return prefix + libraryName + extension;
  }


  // Returns the current value of LD_LIBRARY_PATH environment variable.
  inline std::string paths()
  {
    const char* environmentVariable =
#ifdef __APPLE__
      "DYLD_LIBRARY_PATH";
#else
      "LD_LIBRARY_PATH";
#endif
    const Option<std::string> path = getenv(environmentVariable);
    return path.isSome() ? path.get() : std::string();
  }


  // Updates the value of LD_LIBRARY_PATH environment variable.
  inline void setPaths(const std::string& newPaths)
  {
    const char* environmentVariable =
#ifdef __APPLE__
      "DYLD_LIBRARY_PATH";
#else
      "LD_LIBRARY_PATH";
#endif
    os::setenv(environmentVariable, newPaths);
  }


  // Append newPath to the current value of LD_LIBRARY_PATH environment
  // variable.
  inline void appendPaths(const std::string& newPaths)
  {
    if (paths().empty()) {
      setPaths(newPaths);
    }
    else {
      setPaths(paths() + ":" + newPaths);
    }
  }

} // namespace libraries {

inline auto access(const std::string& fileName, int accessMode) ->
decltype(_access(fileName.c_str(), accessMode))
{
  return _access(fileName.c_str(), accessMode);
}

} // namespace os {


#endif // __STOUT_WINDOWS_OS_HPP__
