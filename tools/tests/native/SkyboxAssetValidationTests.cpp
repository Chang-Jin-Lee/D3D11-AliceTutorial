#include "SkyboxAssetValidation.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace
{
    bool Expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    }

    void WriteBytes(const std::filesystem::path& path, const std::string& bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
}

int wmain()
{
    bool passed = true;
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / L"D3D11-SkyboxAssetValidation-Native";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    {
        SkyboxAssets::ScopedTemporaryDirectory cleanup(root);
        passed &= Expect(cleanup.IsReady(), "temporary directory must be created");

        const std::filesystem::path fixture = root / L"fixture.bin";
        WriteBytes(fixture, "abc");
        std::wstring reason;
        passed &= Expect(
            SkyboxAssets::VerifyFile(
                fixture,
                3,
                L"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                &reason),
            "the exact fixture must pass size and SHA-256 validation");

        WriteBytes(fixture, "abd");
        passed &= Expect(
            !SkyboxAssets::VerifyFile(
                fixture,
                3,
                L"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                &reason),
            "same-size corruption must fail SHA-256 validation");

        WriteBytes(fixture, "ab");
        passed &= Expect(
            !SkyboxAssets::VerifyFile(
                fixture,
                3,
                L"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                &reason),
            "truncation must fail size validation");

        unsigned int attempts = 0;
        passed &= Expect(
            SkyboxAssets::RunWithRetries(3, [&](unsigned int attempt)
            {
                ++attempts;
                return attempt == 3;
            }),
            "a transient operation must be allowed to succeed on its third attempt");
        passed &= Expect(attempts == 3, "retry helper must make exactly three attempts");

        attempts = 0;
        passed &= Expect(
            !SkyboxAssets::RunWithRetries(2, [&](unsigned int)
            {
                ++attempts;
                return false;
            }),
            "a permanently failing operation must report failure");
        passed &= Expect(attempts == 2, "retry helper must stop at the configured attempt limit");

        const std::filesystem::path childPidFile = root / L"child-process.pid";
        SkyboxAssets::CancelableProcess process;
        bool processResult = true;
        const auto processStart = std::chrono::steady_clock::now();
        std::thread processThread([&]
        {
            processResult = process.Run(
                L"powershell.exe -NoProfile -Command \""
                L"$child = Start-Process powershell.exe -ArgumentList "
                L"'-NoProfile','-Command','Start-Sleep -Seconds 60' -PassThru; "
                L"Set-Content -LiteralPath '" + childPidFile.wstring() +
                L"' -Value $child.Id; Wait-Process -Id $child.Id\"");
        });

        const auto processReadyDeadline = processStart + std::chrono::seconds(5);
        while ((!process.IsRunning() || !std::filesystem::exists(childPidFile)) &&
            std::chrono::steady_clock::now() < processReadyDeadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        passed &= Expect(process.IsRunning(), "the cancellable child process must start");
        passed &= Expect(
            std::filesystem::exists(childPidFile),
            "the parent process must launch its long-running child fixture");

        DWORD childProcessId = 0;
        std::wifstream childPidStream(childPidFile);
        childPidStream >> childProcessId;
        passed &= Expect(childProcessId != 0, "the child fixture must publish a process id");

        process.Cancel();
        processThread.join();
        const auto cancellationElapsed = std::chrono::steady_clock::now() - processStart;
        passed &= Expect(!processResult, "a cancelled child process must not report success");
        passed &= Expect(
            cancellationElapsed < std::chrono::seconds(5),
            "cancelling a long-running process tree must unblock its owner promptly");
        passed &= Expect(!process.IsRunning(), "the cancelled child process must be reaped");

        HANDLE childProcess = OpenProcess(SYNCHRONIZE, FALSE, childProcessId);
        const bool childExited = !childProcess || WaitForSingleObject(childProcess, 2000) == WAIT_OBJECT_0;
        if (childProcess)
            CloseHandle(childProcess);
        passed &= Expect(childExited, "cancelling the Job Object must terminate descendant processes");

        const std::filesystem::path fakeArchive = root / L"Skybox.7z";
        WriteBytes(fakeArchive, "not the official archive");
        passed &= Expect(
            !SkyboxAssets::VerifyOfficialArchive(fakeArchive, &reason),
            "an unofficial archive must fail the release size/hash contract");
    }

    passed &= Expect(!std::filesystem::exists(root), "temporary directory must be removed on scope exit");
    return passed ? 0 : 1;
}
