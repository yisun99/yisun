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

#ifndef __STOUT_OS_POSIX_SHELL_HPP__
#define __STOUT_OS_POSIX_SHELL_HPP__

#include <stdarg.h> // For va_list, va_start, etc.
#include <stdio.h> // For ferror, fgets, FILE, pclose, popen.

#include <sys/wait.h> // For waitpid.

#include <ostream>
#include <string>

#include <glog/logging.h>

#include <stout/error.hpp>
#include <stout/format.hpp>
#include <stout/try.hpp>


namespace os {

    // Canonical constants used as platform-dependent args to `exec` calls.
    // name() is the command name, arg0() is the first argument received
    // by the callee, usualy the command name and arg1() is the second
    // command argument received by the callee.
    struct shell_const
    {
        static const char* name()
        {
            return "sh";
        }
        static const char* arg0()
        {
            return "sh";
        }
        static const char* arg1()
        {
            return "-c";
        }
    };

    // Executes a command by calling "/bin/sh -c <command>", and returns
    // after the command has been completed. Returns 0 if succeeds, and
    // return -1 on error (e.g., fork/exec/waitpid failed). This function
    // is async signal safe. We return int instead of returning a Try
    // because Try involves 'new', which is not async signal safe.
    inline int system(const std::string& command)
    {
        pid_t pid = ::fork();

        if (pid == -1) {
            return -1;
        }
        else if (pid == 0) {
            // In child process.
            ::execlp(shell_const::name(), shell_const::arg0(), shell_const::arg1(), command.c_str(), (char*)NULL);
            ::exit(127);
        }
        else {
            // In parent process.
            int status;
            while (::waitpid(pid, &status, 0) == -1) {
                if (errno != EINTR) {
                    return -1;
                }
            }

            return status;
        }
    }

} // namespace os {

#endif // __STOUT_OS_POSIX_SHELL_HPP__
