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

#ifndef __STOUT_OS_WINDOWS_WAIT_HPP__
#define __STOUT_OS_WINDOWS_WAIT_HPP__

#include <process.h>

namespace os {

#define WNOHANG     1               /* dont hang in wait */
#define WUNTRACED   2               /* tell about stopped, untraced children */

#define WIFEXITED(x) true           /* whether the child terminated normally */
#define WIFSIGNALED(x) false        /* whether the child was terminated by a signal */
#define WCOREDUMP(x) false          /* whether the child produced a core dump, only be used if WIFSIGNALED is true*/
#define WIFSTOPPED(x) false         /* whether the child was stopped by delivery of a signal */

#define WEXITSTATUS(x) (x & 0xFF)	/* returns the exit status of the child, only be used if WIFEXITED is true */
#define WTERMSIG(x) 0				/* returns the number of the signals that caused the child process to terminate,
    only be used if WIFSIGNALED is true */

    // Suspends execution of the calling process until a child specified
    // by pid argument has changed state. By default, waitpid() waits only
    // for termninated children, but this behavior is modifiable via the
    // options argument
    //
    // The value of pid can be:
    // <-1: meaning wait for any child process whose process group ID is equal to the absolute value of pid.
    // -1: meaning wait for any child process.
    // 0: meaning wait for any child process whose process group ID is equal to that of the calling process.
    // >0: meaning wait for the child whose process ID is equal to the value of pid.
    //
    // The value of options is an OR of zero or more of the following constants:
    // WNOHANG: return immediately if no child has exited.
    // WUNTRACED: also return if a child has stopped (but not traced via ptrace(2)). Status for traced children
    //		which have stopped is provided even if this option is not specified.
    //
    // If status is not NULL, waitpid() stores status information in the int to which it points.
    //
    // Returns a value equal to the process ID of the child process for which status is reported. If the status
    // is not available, 0 is returned. Otherwise, -1 shall be returend and errno set to indicate the error.
    inline pid_t waitpid(pid_t pid, int *status, int options)
    {
        // For now, we only implement: (pid > 0) && (options is 0 or WNOHANG)
        if ((pid <= 0) || (options != 0 && options != WNOHANG))
        {
            // Function not implemented
            errno = ENOSYS;
            return -1;
        }

        // TODO(yisun) : check pid is one of the child processes
        // if not, set errno to ECHILD and return -1

        // Open the child process
        HANDLE hProcess;
        hProcess = ::OpenProcess(
            PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
            FALSE,
            static_cast<DWORD>(pid));

        // Error out if not able to open
        if (hProcess == NULL)
        {
            // Failed to open the child process
            errno = ECHILD;
            return -1;
        }
        std::shared_ptr<void> hSafeProcess(hProcess, ::CloseHandle);

        // Wait for child to terminate by default
        // otherwise (WNOHANG), no wait
        DWORD dwMilliseconds = (options == 0) ? INFINITE : 0;

        // Wait for the child process
        DWORD dwRes = ::WaitForSingleObject(hSafeProcess.get(), dwMilliseconds);

        // Error out if wait failed
        if ((options == 0 && dwRes != WAIT_OBJECT_0) ||
            (options == WNOHANG && dwRes != WAIT_OBJECT_0 && dwRes != WAIT_TIMEOUT))
        {
            // Failed to wait the child process
            errno = ECHILD;
            return -1;
        }

        // Child not terminated yet in the case of WNOHANG
        if (dwRes == WAIT_TIMEOUT)
        {
            return 0;
        }

        // dwRes == WAIT_OBJECT_0: retrieve the process termination status
        DWORD dwExitCode = 0;
        if (!::GetExitCodeProcess(hSafeProcess.get(), &dwExitCode))
        {
            // Failed to retrieve the status
            errno = ECHILD;
            return -1;
        }

        // Return the exit code in status
        if (status != NULL)
        {
            *status = dwExitCode;
        }

        // Return the pid of the child process for which the status is reported
        return pid;
    }

} // namespace os {

#endif // __STOUT_OS_WINDOWS_WAIT_HPP__
