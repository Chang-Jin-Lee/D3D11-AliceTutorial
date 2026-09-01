#include "pch.h"
#include "SkyboxAssetManager.h"
#include "SkyboxAssetValidation.h"
#include "ReadmeCapture.h"

#include <atomic>
#include <cstdlib>
#include <cwctype>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace
{
	constexpr const wchar_t* kSkyboxReleaseUrl =
		L"https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/releases/download/Skybox_2/Skybox.7z";

	struct RequiredSkyboxAsset
	{
		const wchar_t* folder;
		const wchar_t* fileName;
	};

	const RequiredSkyboxAsset kRequiredAssets[] =
	{
		{ L"Sample", L"BakerSampleDiffuseHDR.dds" },
		{ L"Sample", L"BakerSampleSpecularHDR.dds" },
		{ L"Sample", L"BakerSampleBrdf.dds" },
		{ L"Sample", L"BakerSampleEnvHDR.dds" },
		{ L"Bridge", L"bridgeDiffuseHDR.dds" },
		{ L"Bridge", L"bridgeSpecularHDR.dds" },
		{ L"Bridge", L"bridgeBrdf.dds" },
		{ L"Bridge", L"bridgeEnvHDR.dds" },
		{ L"Indoor", L"indoorDiffuseHDR.dds" },
		{ L"Indoor", L"indoorSpecularHDR.dds" },
		{ L"Indoor", L"indoorBrdf.dds" },
		{ L"Indoor", L"indoorEnvHDR.dds" },
	};
	struct RequiredSkyboxSet
	{
		const wchar_t* folder;
		const wchar_t* prefix;
	};
	const RequiredSkyboxSet kRequiredSets[] =
	{
		{ L"Sample", L"BakerSample" },
		{ L"Bridge", L"bridge" },
		{ L"Indoor", L"indoor" },
	};

	std::atomic<int> g_state{ static_cast<int>(SkyboxAssetDownloadState::Idle) };
	std::atomic<std::uint32_t> g_completedGeneration{ 0 };
	std::mutex g_statusMutex;
	std::wstring g_statusMessage;
	std::mutex g_validationMutex;
	std::unordered_set<std::wstring> g_validatedSets;

	struct DownloadWorkerLifetime
	{
		std::mutex threadMutex;
		SkyboxAssets::CancelableProcess process;
		std::thread worker;
		std::atomic<bool> shutdownRequested{ false };

		~DownloadWorkerLifetime();
	};

	DownloadWorkerLifetime g_workerLifetime;

	bool ShutdownRequested()
	{
		return g_workerLifetime.shutdownRequested.load(std::memory_order_acquire);
	}

	std::filesystem::path SkyboxRoot()
	{
		return std::filesystem::absolute(std::filesystem::path(L"..\\Resource\\Skybox"));
	}

	std::filesystem::path FindDx11Root()
	{
		std::filesystem::path path = std::filesystem::current_path();
		for (int i = 0; i < 8 && !path.empty(); ++i)
		{
			if (std::filesystem::exists(path / L"Common" / L"Common.vcxproj") &&
				std::filesystem::exists(path / L"Resource"))
			{
				return path;
			}
			path = path.parent_path();
		}
		return {};
	}

	std::vector<std::filesystem::path> SkyboxInstallRoots()
	{
		std::vector<std::filesystem::path> roots;
		roots.push_back(SkyboxRoot());

		const std::filesystem::path dx11Root = FindDx11Root();
		if (!dx11Root.empty())
		{
			const std::filesystem::path sourceRoot = std::filesystem::absolute(dx11Root / L"Resource" / L"Skybox");
			if (sourceRoot != roots.front())
			{
				roots.push_back(sourceRoot);
			}
		}
		return roots;
	}

	std::wstring ValidationKey(const std::filesystem::path& pathPrefix)
	{
		std::error_code ec;
		std::wstring key = std::filesystem::absolute(pathPrefix, ec).lexically_normal().wstring();
		if (ec)
			key = pathPrefix.lexically_normal().wstring();
		for (wchar_t& character : key)
			character = static_cast<wchar_t>(std::towlower(character));
		return key;
	}

	void ClearValidatedSets()
	{
		std::lock_guard<std::mutex> lock(g_validationMutex);
		g_validatedSets.clear();
	}

	void MarkAllSetsValidated(const std::filesystem::path& skyboxRoot)
	{
		std::lock_guard<std::mutex> lock(g_validationMutex);
		g_validatedSets.clear();
		for (const RequiredSkyboxSet& set : kRequiredSets)
			g_validatedSets.insert(ValidationKey(skyboxRoot / set.folder / set.prefix));
	}

	bool IsSetValidated(const std::wstring& pathPrefix)
	{
		const std::wstring key = ValidationKey(pathPrefix);
		std::lock_guard<std::mutex> lock(g_validationMutex);
		return g_validatedSets.find(key) != g_validatedSets.end();
	}

	void InvalidateSet(const std::wstring& pathPrefix)
	{
		const std::wstring key = ValidationKey(pathPrefix);
		std::lock_guard<std::mutex> lock(g_validationMutex);
		g_validatedSets.erase(key);
	}

	void SetStatus(SkyboxAssetDownloadState state, std::wstring message)
	{
		{
			std::lock_guard<std::mutex> lock(g_statusMutex);
			g_statusMessage = std::move(message);
		}
		g_state.store(static_cast<int>(state), std::memory_order_release);
	}

	std::wstring QuotePowerShell(const std::filesystem::path& path)
	{
		std::wstring text = path.wstring();
		std::wstring quoted;
		quoted.reserve(text.size() + 2);
		quoted.push_back(L'\'');
		for (wchar_t ch : text)
		{
			if (ch == L'\'') quoted.append(L"''");
			else quoted.push_back(ch);
		}
		quoted.push_back(L'\'');
		return quoted;
	}

	std::wstring QuotePowerShellLiteral(const wchar_t* text)
	{
		std::wstring quoted = L"'";
		for (const wchar_t* p = text; *p; ++p)
		{
			if (*p == L'\'') quoted.append(L"''");
			else quoted.push_back(*p);
		}
		quoted.push_back(L'\'');
		return quoted;
	}

	bool RunPowerShell(const std::wstring& body)
	{
		const std::wstring command =
			L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"& { "
			+ body + L" }\"";
		return g_workerLifetime.process.Run(command);
	}

	bool DownloadArchive(const std::filesystem::path& archivePath, std::wstring& failureReason)
	{
		failureReason.clear();
		return SkyboxAssets::RunWithRetries(3, [&](unsigned int attempt)
		{
			if (ShutdownRequested())
				return false;

			SetStatus(SkyboxAssetDownloadState::Downloading,
				L"Downloading skybox assets (attempt " + std::to_wstring(attempt) + L" of 3)...");

			std::error_code ec;
			std::filesystem::remove(archivePath, ec);
			const std::wstring body =
				L"$ErrorActionPreference='Stop'; "
				L"$ProgressPreference='SilentlyContinue'; "
				L"Invoke-WebRequest -TimeoutSec 180 -Uri " + QuotePowerShellLiteral(kSkyboxReleaseUrl) +
				L" -OutFile " + QuotePowerShell(archivePath) + L";";
			if (!RunPowerShell(body))
			{
				failureReason = L"The network download command failed.";
			}
			else if (SkyboxAssets::VerifyOfficialArchive(archivePath, &failureReason))
			{
				return true;
			}

			if (attempt < 3 && !ShutdownRequested())
				Sleep(750);
			return false;
		});
	}

	bool ExtractArchive(const std::filesystem::path& archivePath, const std::filesystem::path& extractDir)
	{
		if (ShutdownRequested())
			return false;

		SetStatus(SkyboxAssetDownloadState::Downloading,
			L"Extracting skybox archive...");

		const std::wstring archive = QuotePowerShell(archivePath);
		const std::wstring extract = QuotePowerShell(extractDir);
		const std::wstring body =
			L"$ErrorActionPreference='Stop'; "
			L"if (Test-Path " + extract + L") { Remove-Item -LiteralPath " + extract + L" -Recurse -Force; } "
			L"New-Item -ItemType Directory -Force -Path " + extract + L" | Out-Null; "
			L"if (Get-Command tar.exe -ErrorAction SilentlyContinue) { "
			L"& tar.exe -xf " + archive + L" -C " + extract + L"; "
			L"if ($LASTEXITCODE -ne 0) { throw 'tar extraction failed'; } "
			L"} else { "
			L"$seven = Get-Command 7z.exe,7za.exe -ErrorAction SilentlyContinue | Select-Object -First 1; "
			L"if (-not $seven) { throw 'No extractor found for Skybox.7z'; } "
			L"& $seven.Source x " + archive + L" ('-o' + " + extract + L") -y; "
			L"if ($LASTEXITCODE -ne 0) { throw '7z extraction failed'; } "
			L"}";
		return RunPowerShell(body);
	}

	std::filesystem::path FindExtractedFile(const std::filesystem::path& root, const std::wstring& fileName)
	{
		if (!std::filesystem::exists(root)) return {};

		std::error_code ec;
		for (std::filesystem::recursive_directory_iterator it(root,
			std::filesystem::directory_options::skip_permission_denied, ec), end;
			it != end && !ec; it.increment(ec))
		{
			if (ShutdownRequested()) return {};
			if (!it->is_regular_file(ec)) continue;
			if (_wcsicmp(it->path().filename().c_str(), fileName.c_str()) == 0)
			{
				return it->path();
			}
		}
		return {};
	}

	bool InstallExtractedAssets(const std::filesystem::path& extractDir, const std::filesystem::path& skyboxDir)
	{
		SetStatus(SkyboxAssetDownloadState::Downloading,
			L"Installing skybox assets...");

		std::error_code ec;
		std::filesystem::create_directories(skyboxDir, ec);
		if (ec) return false;

		for (const RequiredSkyboxAsset& asset : kRequiredAssets)
		{
			if (ShutdownRequested()) return false;

			const std::filesystem::path targetDir = skyboxDir / asset.folder;
			const std::filesystem::path targetFile = targetDir / asset.fileName;
			if (SkyboxAssets::VerifyRequiredAsset(targetFile)) continue;

			const std::filesystem::path found = FindExtractedFile(extractDir, asset.fileName);
			if (found.empty() || !SkyboxAssets::VerifyRequiredAsset(found)) return false;

			std::filesystem::create_directories(targetDir, ec);
			if (ec) return false;

			std::filesystem::copy_file(found, targetFile,
				std::filesystem::copy_options::overwrite_existing, ec);
			if (ec || !SkyboxAssets::VerifyRequiredAsset(targetFile)) return false;
		}

		return true;
	}

	bool InstallExtractedAssetsToAllRoots(const std::filesystem::path& extractDir)
	{
		bool installedAny = false;
		for (const std::filesystem::path& root : SkyboxInstallRoots())
		{
			if (ShutdownRequested()) return false;

			if (!InstallExtractedAssets(extractDir, root))
			{
				return false;
			}
			installedAny = true;
		}
		return installedAny;
	}

	bool AllRequiredAssetsAreValid()
	{
		return SkyboxAssets::VerifyInstallRoot(SkyboxRoot());
	}

	void DownloadWorker()
	{
		try
		{
			if (ShutdownRequested())
				return;

			ClearValidatedSets();
			SetStatus(SkyboxAssetDownloadState::Downloading, L"Validating installed skybox assets...");
			if (AllRequiredAssetsAreValid())
			{
				if (ShutdownRequested())
					return;

				MarkAllSetsValidated(SkyboxRoot());
				g_completedGeneration.fetch_add(1, std::memory_order_acq_rel);
				SetStatus(SkyboxAssetDownloadState::Ready, L"Skybox assets are ready.");
				return;
			}

			const std::filesystem::path tempRoot =
				std::filesystem::temp_directory_path() /
				(L"D3D11-AliceTutorial-Skybox-" + std::to_wstring(GetCurrentProcessId()) +
					L"-" + std::to_wstring(GetTickCount64()));
			SkyboxAssets::ScopedTemporaryDirectory temporaryDirectory(tempRoot);
			const std::filesystem::path archivePath = tempRoot / L"Skybox.7z";
			const std::filesystem::path extractDir = tempRoot / L"extract";

			if (!temporaryDirectory.IsReady())
			{
				SetStatus(SkyboxAssetDownloadState::Failed, L"Failed to create temp directory for skybox download.");
				return;
			}

			std::wstring downloadFailure;
			if (!DownloadArchive(archivePath, downloadFailure))
			{
				if (ShutdownRequested())
					return;

				SetStatus(SkyboxAssetDownloadState::Failed,
					L"Failed to download and verify Skybox.7z after 3 attempts. " + downloadFailure);
				return;
			}

			if (!ExtractArchive(archivePath, extractDir))
			{
				if (ShutdownRequested())
					return;

				SetStatus(SkyboxAssetDownloadState::Failed, L"Failed to extract Skybox.7z. Install tar.exe/7z.exe or check the archive.");
				return;
			}

			if (!InstallExtractedAssetsToAllRoots(extractDir) || !AllRequiredAssetsAreValid())
			{
				if (ShutdownRequested())
					return;

				SetStatus(SkyboxAssetDownloadState::Failed, L"Skybox assets failed size or SHA-256 validation.");
				return;
			}

			if (ShutdownRequested())
				return;

			MarkAllSetsValidated(SkyboxRoot());
			g_completedGeneration.fetch_add(1, std::memory_order_acq_rel);
			SetStatus(SkyboxAssetDownloadState::Succeeded, L"Skybox assets downloaded and installed.");
		}
		catch (...)
		{
			if (!ShutdownRequested())
				SetStatus(SkyboxAssetDownloadState::Failed, L"Unexpected error while downloading skybox assets.");
		}
	}

	void StartDownload(bool forceRetry)
	{
		std::lock_guard<std::mutex> lock(g_workerLifetime.threadMutex);
		if (ShutdownRequested())
			return;

		const int current = g_state.load(std::memory_order_acquire);
		if (current == static_cast<int>(SkyboxAssetDownloadState::Downloading) ||
			(!forceRetry && current == static_cast<int>(SkyboxAssetDownloadState::Failed)))
		{
			return;
		}

		if (g_workerLifetime.worker.joinable())
			g_workerLifetime.worker.join();

		g_state.store(static_cast<int>(SkyboxAssetDownloadState::Downloading), std::memory_order_release);
		g_workerLifetime.worker = std::thread(DownloadWorker);
	}

	void ShutdownDownloadWorker()
	{
		g_workerLifetime.shutdownRequested.store(true, std::memory_order_release);
		g_workerLifetime.process.Cancel();

		std::thread worker;
		{
			std::lock_guard<std::mutex> lock(g_workerLifetime.threadMutex);
			if (g_workerLifetime.worker.joinable())
				worker = std::move(g_workerLifetime.worker);
		}
		if (worker.joinable())
			worker.join();
	}

	DownloadWorkerLifetime::~DownloadWorkerLifetime()
	{
		ShutdownDownloadWorker();
	}
}

bool SkyboxAssetManager::HasIBLAssetSet(const std::wstring& pathPrefix)
{
	if (ReadmeCapture::IsEnabled())
		return SkyboxAssets::HasExpectedFileSizesForSetPrefix(pathPrefix);
	if (!IsSetValidated(pathPrefix))
		return false;
	if (SkyboxAssets::HasExpectedFileSizesForSetPrefix(pathPrefix))
		return true;
	InvalidateSet(pathPrefix);
	return false;
}

void SkyboxAssetManager::EnsureSkyboxAssetsAsync()
{
	if (ReadmeCapture::IsEnabled())
	{
		return;
	}

	StartDownload(false);
}

void SkyboxAssetManager::RetrySkyboxAssetsAsync()
{
	StartDownload(true);
}

void SkyboxAssetManager::Shutdown()
{
	ShutdownDownloadWorker();
}

SkyboxAssetDownloadState SkyboxAssetManager::GetState()
{
	return static_cast<SkyboxAssetDownloadState>(g_state.load(std::memory_order_acquire));
}

std::uint32_t SkyboxAssetManager::GetCompletedGeneration()
{
	return g_completedGeneration.load(std::memory_order_acquire);
}

std::wstring SkyboxAssetManager::GetStatusMessage()
{
	std::lock_guard<std::mutex> lock(g_statusMutex);
	return g_statusMessage;
}

namespace
{
	std::string NarrowAscii(const std::wstring& text)
	{
		std::string result;
		result.reserve(text.size());
		for (wchar_t ch : text)
		{
			result.push_back((ch >= 0 && ch < 128) ? static_cast<char>(ch) : '?');
		}
		return result;
	}
}

void SkyboxAssetManager::RenderStatusUI()
{
	if (ReadmeCapture::IsEnabled())
	{
		return;
	}

	const SkyboxAssetDownloadState state = GetState();
	if (state == SkyboxAssetDownloadState::Idle || state == SkyboxAssetDownloadState::Ready)
	{
		return;
	}

	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 windowSize(420.0f, 112.0f);
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - windowSize.x - 16.0f, 16.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("Skybox Assets", nullptr, flags))
	{
		const std::wstring messageW = GetStatusMessage();
		const std::string message = NarrowAscii(messageW);

		if (state == SkyboxAssetDownloadState::Downloading)
		{
			ImGui::TextUnformatted("Skybox assets are being prepared...");
			ImGui::TextUnformatted(message.c_str());
			ImGui::ProgressBar(-1.0f, ImVec2(-1.0f, 0.0f));
		}
		else if (state == SkyboxAssetDownloadState::Succeeded)
		{
			ImGui::TextUnformatted("Skybox assets are ready.");
			ImGui::TextUnformatted("Reloading the current IBL selection.");
			if (ImGui::Button("Hide"))
			{
				g_state.store(static_cast<int>(SkyboxAssetDownloadState::Ready), std::memory_order_release);
			}
		}
		else if (state == SkyboxAssetDownloadState::Failed)
		{
			ImGui::TextUnformatted("Skybox asset download failed.");
			ImGui::TextWrapped("%s", message.c_str());
			if (ImGui::Button("Retry"))
			{
				RetrySkyboxAssetsAsync();
			}
		}
	}
	ImGui::End();
}
