#include "SkyboxAssetValidation.h"

#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <cwctype>
#include <fstream>
#include <mutex>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace
{
	struct ExpectedAsset
	{
		const wchar_t* relativePath;
		std::uintmax_t size;
		const wchar_t* sha256;
	};

	constexpr std::uintmax_t kOfficialArchiveSize = 364836665;
	constexpr wchar_t kOfficialArchiveSha256[] =
		L"fb5df312b57fc85698fbbbf84bc934fdec32a7eced0c8aaf62a4adf8edfa6565";

	constexpr std::array<ExpectedAsset, 12> kExpectedAssets = {{
		{ L"Sample\\BakerSampleDiffuseHDR.dds", 983168, L"66cce54aed2d0a3b063d21f8ff8bbf5737f90abce569b8a2c4a5b64384788162" },
		{ L"Sample\\BakerSampleSpecularHDR.dds", 16777376, L"cc5937755c2dce160448293f0ed2dbf04636571f6a9ad6d719521ad1a2027692" },
		{ L"Sample\\BakerSampleBrdf.dds", 1310848, L"6570289a6d9aa4f1a52cebf7a30e434ec100c49e3b59c2f33ac41cb98099bf18" },
		{ L"Sample\\BakerSampleEnvHDR.dds", 268435616, L"0a84a47a63768e4cf5c3e9b8bb494f920aad056b1b234328176f0a7d148d8bc0" },
		{ L"Bridge\\bridgeDiffuseHDR.dds", 1966208, L"2e744d15be1c7711de358e22efbec2ac7df8b8b03b69e59d84e47c7170acd1e7" },
		{ L"Bridge\\bridgeSpecularHDR.dds", 33554624, L"19758a65814d3b9888e63588b0b5388b78e508ba92e22d004ebd999e325c6a77" },
		{ L"Bridge\\bridgeBrdf.dds", 1310848, L"6570289a6d9aa4f1a52cebf7a30e434ec100c49e3b59c2f33ac41cb98099bf18" },
		{ L"Bridge\\bridgeEnvHDR.dds", 33554624, L"c2c7d3bf8330d5192d0f0af3c1ce4f89e8e93b382b6201f7dce36b322ccfe80d" },
		{ L"Indoor\\indoorDiffuseHDR.dds", 1966208, L"c2e5a436ae4c569ad6061199e127fc213f146b078a91a6eb47fb6579d65ba061" },
		{ L"Indoor\\indoorSpecularHDR.dds", 33554624, L"4330b45b79e367046a2550f2489d3ac5a5559d26f72bc3db583a0ce5e793e83f" },
		{ L"Indoor\\indoorBrdf.dds", 1310848, L"6570289a6d9aa4f1a52cebf7a30e434ec100c49e3b59c2f33ac41cb98099bf18" },
		{ L"Indoor\\indoorEnvHDR.dds", 33554624, L"1c9c48225b3a48190b168cf56de277ab13a09d70f2e8d4c96c18808f5f388a96" },
	}};

	void SetReason(std::wstring* reason, std::wstring message)
	{
		if (reason)
			*reason = std::move(message);
	}

	std::wstring ToHex(const std::array<unsigned char, 32>& bytes)
	{
		constexpr wchar_t digits[] = L"0123456789abcdef";
		std::wstring result;
		result.reserve(bytes.size() * 2);
		for (const unsigned char byte : bytes)
		{
			result.push_back(digits[byte >> 4]);
			result.push_back(digits[byte & 0x0f]);
		}
		return result;
	}

	bool ComputeSha256(
		const std::filesystem::path& path,
		std::array<unsigned char, 32>& digest,
		std::wstring* reason)
	{
		BCRYPT_ALG_HANDLE algorithm = nullptr;
		BCRYPT_HASH_HANDLE hash = nullptr;
		std::vector<unsigned char> hashObject;
		bool hashingFailed = false;
		bool success = false;

		do
		{
			if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
				break;

			DWORD objectSize = 0;
			DWORD bytesRead = 0;
			if (BCryptGetProperty(
				algorithm,
				BCRYPT_OBJECT_LENGTH,
				reinterpret_cast<PUCHAR>(&objectSize),
				sizeof(objectSize),
				&bytesRead,
				0) < 0)
				break;

			hashObject.resize(objectSize);
			if (BCryptCreateHash(
				algorithm,
				&hash,
				hashObject.data(),
				static_cast<ULONG>(hashObject.size()),
				nullptr,
				0,
				0) < 0)
				break;

			std::ifstream stream(path, std::ios::binary);
			if (!stream)
				break;

			std::vector<unsigned char> buffer(1024 * 1024);
			while (stream)
			{
				stream.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
				const std::streamsize count = stream.gcount();
				if (count > 0 && BCryptHashData(
					hash,
					buffer.data(),
					static_cast<ULONG>(count),
					0) < 0)
				{
					hashingFailed = true;
					break;
				}
				if (count > 0 && stream.bad())
					break;
			}
			if (hashingFailed || stream.bad())
				break;

			if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
				break;
			success = true;
		} while (false);

		if (hash)
			BCryptDestroyHash(hash);
		if (algorithm)
			BCryptCloseAlgorithmProvider(algorithm, 0);
		if (!success)
			SetReason(reason, L"Could not compute SHA-256 for " + path.wstring());
		return success;
	}

	const ExpectedAsset* FindExpectedAsset(const std::filesystem::path& path)
	{
		const std::wstring fileName = path.filename().wstring();
		for (const ExpectedAsset& asset : kExpectedAssets)
		{
			if (_wcsicmp(std::filesystem::path(asset.relativePath).filename().c_str(), fileName.c_str()) == 0)
				return &asset;
		}
		return nullptr;
	}
}

namespace SkyboxAssets
{
	struct CancelableProcess::Impl
	{
		mutable std::mutex mutex;
		HANDLE activeJob = nullptr;
		bool cancelRequested = false;
	};

	bool VerifyFile(
		const std::filesystem::path& path,
		std::uintmax_t expectedSize,
		std::wstring_view expectedSha256,
		std::wstring* reason)
	{
		std::error_code ec;
		if (!std::filesystem::is_regular_file(path, ec))
		{
			SetReason(reason, L"Missing file: " + path.wstring());
			return false;
		}
		if (std::filesystem::file_size(path, ec) != expectedSize || ec)
		{
			SetReason(reason, L"Unexpected file size: " + path.wstring());
			return false;
		}

		std::array<unsigned char, 32> digest{};
		if (!ComputeSha256(path, digest, reason))
			return false;

		std::wstring expected(expectedSha256);
		for (wchar_t& character : expected)
			character = static_cast<wchar_t>(std::towlower(character));
		if (ToHex(digest) != expected)
		{
			SetReason(reason, L"SHA-256 mismatch: " + path.wstring());
			return false;
		}
		if (reason)
			reason->clear();
		return true;
	}

	bool VerifyOfficialArchive(const std::filesystem::path& path, std::wstring* reason)
	{
		return VerifyFile(path, kOfficialArchiveSize, kOfficialArchiveSha256, reason);
	}

	bool VerifyRequiredAsset(const std::filesystem::path& path, std::wstring* reason)
	{
		const ExpectedAsset* expected = FindExpectedAsset(path);
		if (!expected)
		{
			SetReason(reason, L"Unknown skybox asset: " + path.wstring());
			return false;
		}
		return VerifyFile(path, expected->size, expected->sha256, reason);
	}

	bool VerifyInstallRoot(const std::filesystem::path& skyboxRoot, std::wstring* reason)
	{
		for (const ExpectedAsset& asset : kExpectedAssets)
		{
			if (!VerifyFile(skyboxRoot / asset.relativePath, asset.size, asset.sha256, reason))
				return false;
		}
		if (reason)
			reason->clear();
		return true;
	}

	bool VerifySetPrefix(const std::wstring& pathPrefix, std::wstring* reason)
	{
		for (const wchar_t* suffix : { L"DiffuseHDR.dds", L"SpecularHDR.dds", L"Brdf.dds", L"EnvHDR.dds" })
		{
			if (!VerifyRequiredAsset(pathPrefix + suffix, reason))
				return false;
		}
		if (reason)
			reason->clear();
		return true;
	}

	bool HasExpectedFileSizesForSetPrefix(const std::wstring& pathPrefix)
	{
		for (const wchar_t* suffix : { L"DiffuseHDR.dds", L"SpecularHDR.dds", L"Brdf.dds", L"EnvHDR.dds" })
		{
			const std::filesystem::path path = pathPrefix + suffix;
			const ExpectedAsset* expected = FindExpectedAsset(path);
			if (!expected)
				return false;

			std::error_code ec;
			if (!std::filesystem::is_regular_file(path, ec) || ec)
				return false;
			if (std::filesystem::file_size(path, ec) != expected->size || ec)
				return false;
		}
		return true;
	}

	bool RunWithRetries(
		unsigned int maxAttempts,
		const std::function<bool(unsigned int)>& operation)
	{
		for (unsigned int attempt = 1; attempt <= maxAttempts; ++attempt)
		{
			if (operation(attempt))
				return true;
		}
		return false;
	}

	CancelableProcess::CancelableProcess()
		: m_impl(std::make_unique<Impl>())
	{
	}

	CancelableProcess::~CancelableProcess()
	{
		Cancel();
	}

	bool CancelableProcess::Run(const std::wstring& commandLine)
	{
		if (commandLine.empty())
			return false;

		HANDLE job = CreateJobObjectW(nullptr, nullptr);
		if (!job)
			return false;

		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
		limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (!SetInformationJobObject(
			job,
			JobObjectExtendedLimitInformation,
			&limits,
			static_cast<DWORD>(sizeof(limits))))
		{
			CloseHandle(job);
			return false;
		}

		std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
		mutableCommand.push_back(L'\0');

		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};
		if (!CreateProcessW(
			nullptr,
			mutableCommand.data(),
			nullptr,
			nullptr,
			FALSE,
			CREATE_NO_WINDOW | CREATE_SUSPENDED,
			nullptr,
			nullptr,
			&startup,
			&process))
		{
			CloseHandle(job);
			return false;
		}

		if (!AssignProcessToJobObject(job, process.hProcess))
		{
			TerminateProcess(process.hProcess, ERROR_CANCELLED);
			CloseHandle(process.hThread);
			CloseHandle(process.hProcess);
			CloseHandle(job);
			return false;
		}

		bool cancelBeforeResume = false;
		bool anotherProcessIsRunning = false;
		{
			std::lock_guard<std::mutex> lock(m_impl->mutex);
			anotherProcessIsRunning = m_impl->activeJob != nullptr;
			if (!anotherProcessIsRunning)
			{
				m_impl->activeJob = job;
				cancelBeforeResume = m_impl->cancelRequested;
			}
		}

		if (anotherProcessIsRunning)
		{
			TerminateJobObject(job, ERROR_BUSY);
			CloseHandle(process.hThread);
			CloseHandle(process.hProcess);
			CloseHandle(job);
			return false;
		}

		if (cancelBeforeResume || ResumeThread(process.hThread) == static_cast<DWORD>(-1))
			TerminateJobObject(job, ERROR_CANCELLED);

		const DWORD waitResult = WaitForSingleObject(process.hProcess, INFINITE);
		DWORD exitCode = ERROR_CANCELLED;
		if (waitResult == WAIT_OBJECT_0)
			GetExitCodeProcess(process.hProcess, &exitCode);

		{
			std::lock_guard<std::mutex> lock(m_impl->mutex);
			if (m_impl->activeJob == job)
				m_impl->activeJob = nullptr;
		}

		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		CloseHandle(job);
		return waitResult == WAIT_OBJECT_0 && exitCode == 0;
	}

	void CancelableProcess::Cancel() noexcept
	{
		std::lock_guard<std::mutex> lock(m_impl->mutex);
		m_impl->cancelRequested = true;
		if (m_impl->activeJob)
			TerminateJobObject(m_impl->activeJob, ERROR_CANCELLED);
	}

	bool CancelableProcess::IsRunning() const noexcept
	{
		std::lock_guard<std::mutex> lock(m_impl->mutex);
		return m_impl->activeJob != nullptr;
	}

	ScopedTemporaryDirectory::ScopedTemporaryDirectory(std::filesystem::path path)
		: m_path(std::move(path))
	{
		std::error_code ec;
		m_ready = std::filesystem::create_directories(m_path, ec) && !ec;
	}

	ScopedTemporaryDirectory::~ScopedTemporaryDirectory()
	{
		if (!m_ready)
			return;
		std::error_code ec;
		std::filesystem::remove_all(m_path, ec);
	}

	bool ScopedTemporaryDirectory::IsReady() const noexcept
	{
		return m_ready;
	}

	const std::filesystem::path& ScopedTemporaryDirectory::Path() const noexcept
	{
		return m_path;
	}
}
