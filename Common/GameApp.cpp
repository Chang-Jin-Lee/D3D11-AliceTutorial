#include "pch.h"
#include "GameApp.h"
#include "Helper.h"

#include <dbghelp.h>
#include <minidumpapiset.h>

#pragma comment(lib, "Dbghelp.lib")

// ���� �� �׸��� ����ϱ� ���� ���
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

GameApp* GameApp::m_pInstance = nullptr;
HWND GameApp::m_hWnd;

LRESULT CALLBACK DefaultWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return  GameApp::m_pInstance->WndProc(hWnd,message,wParam,lParam);
}

void CreateDump(EXCEPTION_POINTERS* pExceptionPointers)
{
	wchar_t moduleFileName[MAX_PATH]={0,};
	std::wstring fileName(moduleFileName);
	if (GetModuleFileName(NULL, moduleFileName, MAX_PATH) == 0) {
		fileName = L"unknown_project.dmp"; // ���� ��Ȳ ó��
	}
	else
	{
		fileName = std::wstring(moduleFileName);
		size_t pos = fileName.find_last_of(L"\\/");
		if (pos != std::wstring::npos) {
			fileName = fileName.substr(pos + 1); // ���� �̸� ����
		}

		pos = fileName.find_last_of(L'.');
		if (pos != std::wstring::npos) {
			fileName = fileName.substr(0, pos); // Ȯ���� ����
		}
		fileName += L".dmp";
	}

	HANDLE hFile = CreateFile(fileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return;

	MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
	dumpInfo.ThreadId = GetCurrentThreadId();
	dumpInfo.ExceptionPointers = pExceptionPointers;
	dumpInfo.ClientPointers = TRUE;

	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &dumpInfo, NULL, NULL);

	CloseHandle(hFile);
}

LONG WINAPI CustomExceptionHandler(EXCEPTION_POINTERS* pExceptionPointers)
{
	int msgResult = MessageBox(
		NULL,
		L"Should Create Dump ?",
		L"Exception",
		MB_YESNO | MB_ICONQUESTION
	);
	
	if (msgResult == IDYES) {
		CreateDump(pExceptionPointers);
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

GameApp::GameApp()
	: m_szWindowClass(L"DefaultWindowCalss"), m_szTitle(L"GameApp"), m_ClientWidth(1024), m_ClientHeight(768)
{
	GameApp::m_pInstance = this;
}

GameApp::~GameApp()
{

}


bool GameApp::Initialize()
{
	SetUnhandledExceptionFilter(CustomExceptionHandler);

	// ���
	RegisterClassExW(&m_wcex);

	// ���ϴ� ũ�Ⱑ �����Ǿ� ����
	RECT rcClient = { 0, 0, (LONG)m_ClientWidth, (LONG)m_ClientHeight };
	AdjustWindowRect(&rcClient, WS_OVERLAPPEDWINDOW, FALSE);

	//����
	m_hWnd = CreateWindowW(m_szWindowClass, m_szTitle, WS_OVERLAPPEDWINDOW,
		100, 100,	// ���� ��ġ
		rcClient.right - rcClient.left, rcClient.bottom - rcClient.top,
		nullptr, nullptr, m_hInstance, nullptr);

	if (!m_hWnd)
	{
		return false;
	}

	// ��ũ ��� ����
	EnableDarkTitleBar(m_hWnd, true);

	// ������ ���̱�
	ShowWindow(m_hWnd,SW_SHOW);
	UpdateWindow(m_hWnd);

	m_currentTime = m_previousTime = (float)GetTickCount64() / 1000.0f;
	m_Input.Initialize(m_hWnd,this);

	if (!OnInitialize())
		return false;

	return true;
}

void GameApp::Uninitialize()
{
	OnUninitialize();
}

bool GameApp::Run(HINSTANCE hInstance)
{
	m_wcex.hInstance = hInstance;
	m_wcex.cbSize = sizeof(WNDCLASSEX);
	m_wcex.style = CS_HREDRAW | CS_VREDRAW;
	m_wcex.lpfnWndProc = DefaultWndProc;
	m_wcex.cbClsExtra = 0;
	m_wcex.cbWndExtra = 0;
	m_wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	m_wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	m_wcex.lpszClassName = m_szWindowClass;

	// ������ ����
	m_wcex.hIcon = (HICON)LoadImageW(
		NULL,
		L"..\\Resource\\Icon\\Alice.ico",
		IMAGE_ICON,
		128, 128,           // ������ ũ��
		LR_LOADFROMFILE
	);

	m_Timer.Reset();

	// PeekMessage �޼����� ������ true,������ false
	try
	{
		if (!Initialize())
			return false;

		// PeekMessage �޼����� ������ true,������ false
		while (TRUE)
		{
			if (PeekMessage(&m_msg, NULL, 0, 0, PM_REMOVE))
			{
				if (m_msg.message == WM_QUIT)
					break;

				//������ �޽��� ó�� 
				TranslateMessage(&m_msg); // Ű�Է°��� �޽��� ��ȯ  WM_KEYDOWN -> WM_CHAR
				DispatchMessage(&m_msg);
			}

			Update();
			Render();

		}
	}
	catch (const std::exception& e)
	{
		::MessageBoxA(NULL, e.what(), "Exception", MB_OK);
	}
	Uninitialize();
	return true;
}


void GameApp::Update()
{
	m_Timer.Tick();
	m_Input.Update(m_Timer.DeltaTime());
	m_Camera.Update(m_Timer.DeltaTime());

	OnUpdate(m_Timer.DeltaTime());
}

void GameApp::Render()
{
	OnRender();
}

void GameApp::OnInputProcess(const Keyboard::State& KeyState, const Keyboard::KeyboardStateTracker& KeyTracker, const Mouse::State& MouseState, const Mouse::ButtonStateTracker& MouseTracker)
{
	m_Camera.OnInputProcess(KeyState,KeyTracker,MouseState, MouseTracker);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//
//  �Լ�: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  �뵵: �� â�� �޽����� ó���մϴ�.
//  WM_DESTROY  - ���� �޽����� �Խ��ϰ� ��ȯ�մϴ�.
//
//
LRESULT CALLBACK GameApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_ACTIVATEAPP:
		DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
		DirectX::Mouse::ProcessMessage(message, wParam, lParam);
		break;
	case WM_INPUT:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEHOVER:
		Mouse::ProcessMessage(message, wParam, lParam);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		Keyboard::ProcessMessage(message, wParam, lParam);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

float GameApp::AspectRatio() const
{
	return static_cast<float>(m_ClientWidth) / m_ClientHeight;
}

void GameApp::EnableDarkTitleBar(HWND hWnd, bool enable)
{
	const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20; // Win10 2004+/Win11
	const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_OLD = 19; // Win10 1809~1903
	const DWORD DWMWA_CAPTION_COLOR = 35; // Win11
	const DWORD DWMWA_TEXT_COLOR = 36; // Win11

	BOOL on = enable ? TRUE : FALSE;

	// ��ũ Ÿ��Ʋ�� ����
	if (FAILED(DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &on, sizeof(on))))
		DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &on, sizeof(on));

	// Win11�̸� ĸ��/�ؽ�Ʈ ������ ����(���� ����/���)
	COLORREF caption = RGB(0, 0, 0);
	COLORREF text = RGB(255, 255, 255);
	DwmSetWindowAttribute(hWnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
	DwmSetWindowAttribute(hWnd, DWMWA_TEXT_COLOR, &text, sizeof(text));
}
