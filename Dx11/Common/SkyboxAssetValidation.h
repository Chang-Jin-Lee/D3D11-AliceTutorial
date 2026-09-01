#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace SkyboxAssets
{
	bool VerifyFile(
		const std::filesystem::path& path,
		std::uintmax_t expectedSize,
		std::wstring_view expectedSha256,
		std::wstring* reason = nullptr);

	bool VerifyOfficialArchive(
		const std::filesystem::path& path,
		std::wstring* reason = nullptr);

	bool VerifyRequiredAsset(
		const std::filesystem::path& path,
		std::wstring* reason = nullptr);

	bool VerifyInstallRoot(
		const std::filesystem::path& skyboxRoot,
		std::wstring* reason = nullptr);

	bool VerifySetPrefix(
		const std::wstring& pathPrefix,
		std::wstring* reason = nullptr);

	bool HasExpectedFileSizesForSetPrefix(const std::wstring& pathPrefix);

	bool RunWithRetries(
		unsigned int maxAttempts,
		const std::function<bool(unsigned int)>& operation);

	class CancelableProcess
	{
	public:
		CancelableProcess();
		~CancelableProcess();

		CancelableProcess(const CancelableProcess&) = delete;
		CancelableProcess& operator=(const CancelableProcess&) = delete;

		bool Run(const std::wstring& commandLine);
		void Cancel() noexcept;
		bool IsRunning() const noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

	class ScopedTemporaryDirectory
	{
	public:
		explicit ScopedTemporaryDirectory(std::filesystem::path path);
		~ScopedTemporaryDirectory();

		ScopedTemporaryDirectory(const ScopedTemporaryDirectory&) = delete;
		ScopedTemporaryDirectory& operator=(const ScopedTemporaryDirectory&) = delete;

		bool IsReady() const noexcept;
		const std::filesystem::path& Path() const noexcept;

	private:
		std::filesystem::path m_path;
		bool m_ready = false;
	};
}
