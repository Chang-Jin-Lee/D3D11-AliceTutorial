#include "pch.h"
#include "BaseObject.h"
#include "Helper.h"
#include <directxtk/WICTextureLoader.h>
#include <filesystem>

namespace
{
	bool LoadOptionalWICTexture(
		ID3D11Device* device,
		const std::wstring& path,
		ID3D11ShaderResourceView** outSRV)
	{
		if (!device || path.empty() || !outSRV || !std::filesystem::exists(path))
		{
			return false;
		}

		Microsoft::WRL::ComPtr<ID3D11Resource> res;
		ID3D11ShaderResourceView* nextSRV = nullptr;
		if (SUCCEEDED(CreateWICTextureFromFile(device, path.c_str(), res.GetAddressOf(), &nextSRV)))
		{
			if (*outSRV) (*outSRV)->Release();
			*outSRV = nextSRV;
			return true;
		}

		return false;
	}
}

void CubeObject::LoadTexture2DAt(ID3D11Device* m_pDevice, const int& n, const std::wstring& path)
{
	if (n < 0 || n >= 6) return;
	facePathStorage[n] = path;
	facePaths[n] = nullptr;
	if (LoadOptionalWICTexture(m_pDevice, facePathStorage[n], &faceSRV[n]))
	{
		facePaths[n] = facePathStorage[n].c_str();
	}
}

void CubeObject::LoadTextureNormalAt(ID3D11Device* m_pDevice, const int& n, const std::wstring& path)
{
	if (n < 0 || n >= 6) return;
	normalPathStorage[n] = path;
	normalPaths[n] = nullptr;
	if (LoadOptionalWICTexture(m_pDevice, normalPathStorage[n], &normalSRV[n]))
	{
		normalPaths[n] = normalPathStorage[n].c_str();
	}
}

void CubeObject::LoadTextureSpecularAt(ID3D11Device* m_pDevice, const int& n, const std::wstring& path)
{
	if (n < 0 || n >= 6) return;
	specularPathStorage[n] = path;
	specularPaths[n] = nullptr;
	if (LoadOptionalWICTexture(m_pDevice, specularPathStorage[n], &specSRV[n]))
	{
		specularPaths[n] = specularPathStorage[n].c_str();
	}
}
