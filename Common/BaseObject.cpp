#include "pch.h"
#include "BaseObject.h"
#include "Helper.h"
#include <directxtk/WICTextureLoader.h>

void CubeObject::LoadTexture2DAt(ID3D11Device* m_pDevice, const int& n, const std::wstring& path)
{
	facePaths[n] = path.c_str();
	Microsoft::WRL::ComPtr<ID3D11Resource> res;
	if (facePaths[n]) { HR_T(CreateWICTextureFromFile(m_pDevice, facePaths[n], res.GetAddressOf(), &faceSRV[n])); res.Reset(); }

}

void CubeObject::LoadTextureNormalAt(ID3D11Device* m_pDevice, const int& n, const std::wstring& path)
{
	normalPaths[n] = path.c_str();
	Microsoft::WRL::ComPtr<ID3D11Resource> res;
	if (normalPaths[n]) { HR_T(CreateWICTextureFromFile(m_pDevice, normalPaths[n], res.GetAddressOf(), &normalSRV[n])); res.Reset(); }

}

void CubeObject::LoadTextureSpecularAt(ID3D11Device* m_pDevice, const int& n, const std::wstring& path)
{
	specularPaths[n] = path.c_str();
	Microsoft::WRL::ComPtr<ID3D11Resource> res;
	if (specularPaths[n]) { HR_T(CreateWICTextureFromFile(m_pDevice, specularPaths[n], res.GetAddressOf(), &specSRV[n])); res.Reset(); }

}
