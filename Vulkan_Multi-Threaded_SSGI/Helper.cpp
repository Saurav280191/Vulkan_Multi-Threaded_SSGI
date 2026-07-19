#include "Helper.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Helper
{
    namespace Vulkan
    {
        int RunCommandCaptureOutput(const std::string& _cmd, std::string& _outStdout)
        {
            _outStdout.clear();

#ifdef _WIN32
            // Parse command into program + args or run via CreateProcess with command line.
            // We'll run the command line through CreateProcessW but avoid cmd.exe.
            // Convert UTF-8 std::string to wide string
            int wlen = MultiByteToWideChar(CP_UTF8, 0, _cmd.c_str(), -1, nullptr, 0);
            if (wlen == 0) return -1;
            std::vector<wchar_t> wcmd(wlen);
            MultiByteToWideChar(CP_UTF8, 0, _cmd.c_str(), -1, wcmd.data(), wlen);

            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;
            sa.lpSecurityDescriptor = nullptr;

            HANDLE hRead = nullptr;
            HANDLE hWrite = nullptr;
            if (!CreatePipe(&hRead, &hWrite, &sa, 0))
                return -1;
            // Ensure read handle is not inherited
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOW si{};
            si.cb = sizeof(si);
            si.hStdError = hWrite;
            si.hStdOutput = hWrite;
            si.dwFlags |= STARTF_USESTDHANDLES;

            PROCESS_INFORMATION pi{};

            // CreateProcessW expects a mutable wchar_t* command line
            wchar_t* cmdLine = wcmd.data();

            BOOL ok = CreateProcessW(
                nullptr,        // lpApplicationName
                cmdLine,        // lpCommandLine
                nullptr,        // lpProcessAttributes
                nullptr,        // lpThreadAttributes
                TRUE,           // bInheritHandles
                CREATE_NO_WINDOW, // dwCreationFlags
                nullptr,        // lpEnvironment
                nullptr,        // lpCurrentDirectory
                &si,
                &pi
            );

            // Close the write handle in parent after CreateProcess
            CloseHandle(hWrite);

            if (!ok)
            {
                CloseHandle(hRead);
                return -1;
            }

            // Read output
            std::array<char, 4096> buffer;
            DWORD bytesRead = 0;
            for (;;)
            {
                BOOL success = ReadFile(hRead, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr);
                if (!success || bytesRead == 0) break;
                _outStdout.append(buffer.data(), buffer.data() + bytesRead);
            }

            // Wait for process to exit
            WaitForSingleObject(pi.hProcess, INFINITE);

            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hRead);

            return static_cast<int>(exitCode);

#else
            // POSIX: use popen with stderr redirected to stdout
            std::string fullCmd = cmd + " 2>&1";
            FILE* pipe = popen(fullCmd.c_str(), "r");
            if (!pipe) return -1;

            std::array<char, 4096> buffer;
            while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
            {
                outStdout += buffer.data();
            }

            int status = pclose(pipe);
            if (status == -1) return -1;
            if (WIFEXITED(status)) return WEXITSTATUS(status);
            return -1;
#endif
        }

        namespace Shader
        {
            bool LoadShaderModule(VkDevice _device, const std::filesystem::path& _spvPath, VkShaderModule& _outModule)
            {
                if (_spvPath.extension() != ".spv")
                {
                    std::cout << "Invalid .spv file!" << std::endl;
                }

                std::ifstream file(_spvPath, std::ios::binary | std::ios::ate);
                if (!file.is_open())
                {
                    return false;
                }

                std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);

                std::vector<char> buffer(static_cast<size_t>(size));
                if (!file.read(buffer.data(), size))
                {
                    return false;
                }

                VkShaderModuleCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                createInfo.codeSize = buffer.size();
                createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

                return vkCreateShaderModule(_device, &createInfo, nullptr, &_outModule) == VK_SUCCESS;
            }
        }

    }
}