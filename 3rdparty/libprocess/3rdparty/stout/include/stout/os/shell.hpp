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

#ifndef __STOUT_OS_SHELL_HPP__
#define __STOUT_OS_SHELL_HPP__

// For readability, we minimize the number of #ifdef blocks in the code by
// splitting platform specifc system calls into separate directories.
#ifdef __WINDOWS__
#include <stout/os/windows/wait.hpp>
#include <stout/os/windows/shell.hpp>
#else
#include <stout/os/posix/shell.hpp>
#endif // __WINDOWS__

namespace os {

    /**
    * Runs a shell command with optional arguments.
    *
    * This assumes that a successful execution will result in the exit code
    * for the command to be `EXIT_SUCCESS`; in this case, the contents
    * of the `Try` will be the contents of `stdout`.
    *
    * If the exit code is non-zero or the process was signaled, we will
    * return an appropriate error message; but *not* `stderr`.
    *
    * If the caller needs to examine the contents of `stderr` it should
    * be redirected to `stdout` (using, e.g., "2>&1 || true" in the command
    * string).  The `|| true` is required to obtain a success exit
    * code in case of errors, and still obtain `stderr`, as piped to
    * `stdout`.
    *
    * @param fmt the formatting string that contains the command to execute
    *   in the underlying shell.
    * @param t optional arguments for `fmt`.
    *
    * @return the output from running the specified command with the shell; or
    *   an error message if the command's exit code is non-zero.
    */
    template <typename... T>
    Try<std::string> shell(const std::string& fmt, const T&... t)
    {
        const Try<std::string> command = strings::internal::format(fmt, t...);
        if (command.isError()) {
            return Error(command.error());
        }

        FILE* file;
        std::ostringstream oss;

        if ((file = popen(command.get().c_str(), "r")) == NULL) {
            return Error("Failed to run '" + command.get() + "'");
        }

        char line[1024];
        // NOTE(vinod): Ideally the if and while loops should be interchanged. But
        // we get a broken pipe error if we don't read the output and simply close.
        while (fgets(line, sizeof(line), file) != NULL) {
            oss << line;
        }

        if (ferror(file) != 0) {
            pclose(file); // Ignoring result since we already have an error.
            return Error("Error reading output of '" + command.get() + "'");
        }

        int status;
        if ((status = pclose(file)) == -1) {
            return Error("Failed to get status of '" + command.get() + "'");
        }

        if (WIFSIGNALED(status)) {
            return Error(
                "Running '" + command.get() + "' was interrupted by signal '" +
                strsignal(WTERMSIG(status)) + "'");
        }
        else if ((WEXITSTATUS(status) != EXIT_SUCCESS)) {
            LOG(ERROR) << "Command '" << command.get()
                << "' failed; this is the output:\n" << oss.str();
            return Error(
                "Failed to execute '" + command.get() + "'; the command was either "
                "not found or exited with a non-zero exit status: " +
                stringify(WEXITSTATUS(status)));
        }

        return oss.str();
    }

} // namespace os {

#endif // __STOUT_OS_SHELL_HPP__
