#pragma once

#include <cstdint>
#include <string>

enum class SkyboxAssetDownloadState
{
	Idle = 0,
	Ready,
	Downloading,
	Succeeded,
	Failed
};

class SkyboxAssetManager
{
public:
	static bool HasIBLAssetSet(const std::wstring& pathPrefix);
	static void EnsureSkyboxAssetsAsync();
	static void RetrySkyboxAssetsAsync();
	static void Shutdown();

	static SkyboxAssetDownloadState GetState();
	static std::uint32_t GetCompletedGeneration();
	static std::wstring GetStatusMessage();

	static void RenderStatusUI();
};
