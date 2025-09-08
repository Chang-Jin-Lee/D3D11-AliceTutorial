#include "pch.h"
#include "GameApp.h"
#include "Helper.h"

#include <dbghelp.h>
#include <minidumpapiset.h>

#pragma comment(lib, "Dbghelp.lib")

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
		fileName = L"unknown_project.dmp"; // 예외 상황 처리
	}
	else
	{
		fileName = std::wstring(moduleFileName);
		size_t pos = fileName.find_last_of(L"\\/");
		if (pos != std::wstring::npos) {
			fileName = fileName.substr(pos + 1); // 파일 이름 추출
		}

		pos = fileName.find_last_of(L'.');
		if (pos != std::wstring::npos) {
			fileName = fileName.substr(0, pos); // 확장자 제거
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

	// 등록
	RegisterClassExW(&m_wcex);

	// 원하는 크기가 조정되어 리턴
	RECT rcClient = { 0, 0, (LONG)m_ClientWidth, (LONG)m_ClientHeight };
	AdjustWindowRect(&rcClient, WS_OVERLAPPEDWINDOW, FALSE);

	//생성
	m_hWnd = CreateWindowW(m_szWindowClass, m_szTitle, WS_OVERLAPPEDWINDOW,
		100, 100,	// 시작 위치
		rcClient.right - rcClient.left, rcClient.bottom - rcClient.top,
		nullptr, nullptr, m_hInstance, nullptr);

	if (!m_hWnd)
	{
		return false;
	}

	// 윈도우 보이기
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

	m_Timer.Reset();

	// PeekMessage 메세지가 있으면 true,없으면 false
	try
	{
		if (!Initialize())
			return false;

		// PeekMessage 메세지가 있으면 true,없으면 false
		while (TRUE)
		{
			if (PeekMessage(&m_msg, NULL, 0, 0, PM_REMOVE))
			{
				if (m_msg.message == WM_QUIT)
					break;

				//윈도우 메시지 처리 
				TranslateMessage(&m_msg); // 키입력관련 메시지 변환  WM_KEYDOWN -> WM_CHAR
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

//
//  함수: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  용도: 주 창의 메시지를 처리합니다.
//  WM_DESTROY  - 종료 메시지를 게시하고 반환합니다.
//
//
LRESULT CALLBACK GameApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

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
