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

#include <stout/os/config.hpp>

namespace os {

  inline int pagesize()
  {
    SYSTEM_INFO si = {0};
    GetSystemInfo(&si);
    return si.dwPageSize;
  };

/*
// Sets the value associated with the specified key in the set of
// environment variables.
inline void setenv(const std::string& key,
                   const std::string& value,
                   bool overwrite = true)
{
  UNIMPLEMENTED;
}


// Unsets the value associated with the specified key in the set of
// environment variables.
inline void unsetenv(const std::string& key)
{
  UNIMPLEMENTED;
}


// Executes a command by calling "/bin/sh -c <command>", and returns
// after the command has been completed. Returns 0 if succeeds, and
// return -1 on error (e.g., fork/exec/waitpid failed). This function
// is async signal safe. We return int instead of returning a Try
// because Try involves 'new', which is not async signal safe.
inline int system(const std::string& command)
{
  UNIMPLEMENTED;
}
*/

// This function is a portable version of execvpe ('p' means searching
// executable from PATH and 'e' means setting environments). We add
// this function because it is not available on all systems.
//
// NOTE: This function is not thread safe. It is supposed to be used
// only after fork (when there is only one thread). This function is
// async signal safe.
inline int execvpe(const char* file, char** argv, char** envp)
{
	throw WindowsError(ERROR_NOT_SUPPORTED);
}

// Wait for a child matching PID to die.
// If PID is greater than 0, match any process whose process ID is PID.
// If PID is (pid_t) -1, match any process.
// If PID is (pid_t) 0, match any process with the
// same process group as the current process.
// If PID is less than -1, match any process whose
// process group is the absolute value of PID.
// If the WNOHANG bit is set in OPTIONS, and that child
// is not already dead, return (pid_t) 0.  If successful,
// return PID and store the dead child's status in STAT_LOC.
// Return (pid_t) -1 for errors.  If the WUNTRACED bit is
// set in OPTIONS, return status for stopped children; otherwise don't.
// 
// This function is a cancellation point and therefore not marked with
// __THROW.
pid_t waitpid(pid_t pid, int *status, int options)
{
	throw WindowsError(ERROR_NOT_SUPPORTED);
}

/* Bits in the third argument to `waitpid'.  */
#define	WNOHANG		1	/* Don't block waiting.  */
#define	WUNTRACED	2	/* Report status of stopped children.  */

// Clone the calling process, creating an exact copy.
// Return -1 for errors, 0 to the new process,
// and the process ID of the new process to the old process.
pid_t fork(void)
{
	throw WindowsError(ERROR_NOT_SUPPORTED);
}

// Create a one-way communication channel (pipe).
// If successful, two file descriptors are stored in PIPEDES;
// bytes written on PIPEDES[1] can be read from PIPEDES[0].
// Returns 0 if successful, -1 if not. 
int pipe(int __pipedes[2])
{
	throw WindowsError(ERROR_NOT_SUPPORTED);
}

/*
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


// Suspends execution for the given duration.
inline Try<Nothing> sleep(const Duration& duration)
{
  UNIMPLEMENTED;
}


// Returns the list of files that match the given (shell) pattern.
inline Try<std::list<std::string>> glob(const std::string& pattern)
{
  UNIMPLEMENTED;
}


// Returns the total number of cpus (cores).
inline Try<long> cpus()
{
  UNIMPLEMENTED;
}


// Returns load struct with average system loads for the last
// 1, 5 and 15 minutes respectively.
// Load values should be interpreted as usual average loads from
// uptime(1).
inline Try<Load> loadavg()
{
  UNIMPLEMENTED;
}


// Returns the total size of main and free memory.
inline Try<Memory> memory()
{
  UNIMPLEMENTED;
}


// Return the system information.
inline Try<UTSInfo> uname()
{
  UNIMPLEMENTED;
}


inline Try<std::list<Process>> processes()
{
  UNIMPLEMENTED;
}


// Overload of os::pids for filtering by groups and sessions.
// A group / session id of 0 will fitler on the group / session ID
// of the calling process.
inline Try<std::set<pid_t>> pids(Option<pid_t> group, Option<pid_t> session)
{
  UNIMPLEMENTED;
}
*/
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

} // namespace os {


#endif // __STOUT_WINDOWS_OS_HPP__
