#pragma once
#include <wchar.h>
#include <d3d11.h>
#include <exception>
#include <stdio.h>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <vector>
#include <directxtk/SimpleMath.h>
using namespace DirectX::SimpleMath;

#define LOG_ERROR(...) \
{ \
    wchar_t buffer[256]; \
    swprintf_s(buffer,256, L"[ERROR] %s:%d - ", __FUNCTIONW__, __LINE__); \
    wchar_t message[256]; \
    swprintf_s(message,256, __VA_ARGS__); \
    wcscat_s(buffer, message); \
    wcscat_s(buffer, L"\n"); \
    MessageBoxW(NULL, buffer, L"LOG_ERROR", MB_OK); \
}

#define LOG_WARNING(...) \
{ \
    wchar_t buffer[256]; \
    swprintf_s(buffer,256, L"[WARNING] %s:%d - ", __FUNCTIONW__, __LINE__); \
    wchar_t message[256]; \
    swprintf_s(message,256, __VA_ARGS__); \
    wcscat_s(buffer, message); \
    wcscat_s(buffer, L"\n"); \
    OutputDebugStringW(buffer); \
}

#define LOG_MESSAGE(...) \
{ \
    wchar_t buffer[256]; \
    swprintf_s(buffer,256, L"[MESSAGE] %s:%d - ", __FUNCTIONW__, __LINE__); \
    wchar_t message[256]; \
    swprintf_s(message,256, __VA_ARGS__); \
    wcscat_s(buffer, message); \
    wcscat_s(buffer, L"\n"); \
    OutputDebugStringW(buffer); \
}

#define LOG_ERRORA(...) \
{ \
    char buffer[256]; \
    sprintf_s(buffer,256, "[ERROR] %s:%d - ", __FUNCTION__, __LINE__); \
    char message[256]; \
    sprintf_s(message,256, __VA_ARGS__); \
    strcat_s(buffer, message); \
    strcat_s(buffer, "\n"); \
    MessageBoxA(NULL, buffer, "LOG_ERROR", MB_OK); \
}

#define LOG_WARNINGA(...) \
{ \
    char buffer[256]; \
    sprintf_s(buffer,256, "[WARNING] %s:%d - ", __FUNCTION__, __LINE__); \
    char message[256]; \
    sprintf_s(message,256, __VA_ARGS__); \
    strcat_s(buffer, message); \
    strcat_s(buffer, "\n"); \
    OutputDebugStringW(buffer); \
}

#define LOG_MESSAGEA(...) \
{ \
    char buffer[256]; \
    sprintf_s(buffer, 256, "[MESSAGE] %s:%d - ", __FUNCTION__, __LINE__); \
    char message[256]; \
    sprintf_s(message, 256, __VA_ARGS__); \
    strcat_s(buffer, message); \
    strcat_s(buffer, "\n"); \
    OutputDebugStringA(buffer); \
}

template <typename T>
void SAFE_RELEASE(T* p)
{
	if (p)
	{
		p->Release();
		p = nullptr;
	}
}

template <typename T>
void SAFE_DELETE(T* p)
{
	if (p)
	{
		delete p;
		p = nullptr;
	}
}


LPCWSTR GetComErrorString(HRESULT hr); 
std::string GetComErrorStringA(HRESULT hr);

void CheckDXGIDebug();



// Helper class for COM exceptions
class com_exception : public std::exception {
	HRESULT m_hr;
	const char* m_file;
	int m_line;
	const char* m_func;
	std::string m_msg;

public:
	com_exception(HRESULT hr, const char* file, int line, const char* func)
		: m_hr(hr), m_file(file), m_line(line), m_func(func)
	{
		char buf[512];
		sprintf_s(buf, "COM call failed. hr=0x%08X\n%s\nFile: %s\nLine: %d\nFunc: %s",
			hr, GetComErrorStringA(hr).c_str(), file, line, func);
		m_msg = buf;
	}

	HRESULT hr()   const noexcept { return m_hr; }
	const char* file() const noexcept { return m_file; }
	int line()    const noexcept { return m_line; }
	const char* func() const noexcept { return m_func; }

	const char* what() const noexcept override {
		return m_msg.c_str();
	}
};

// Helper utility converts D3D API failures into exceptions.
inline void HR_T_Impl(HRESULT hr, const char* file, int line, const char* func)
{
	if (FAILED(hr)) {
		throw com_exception(hr, file, line, func);
	}
}
#define HR_T(hr) HR_T_Impl((hr), __FILE__, __LINE__, __func__)


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);

HRESULT CreateTextureFromFile(ID3D11Device* d3dDevice, const wchar_t* szFileName, ID3D11ShaderResourceView** textureView);