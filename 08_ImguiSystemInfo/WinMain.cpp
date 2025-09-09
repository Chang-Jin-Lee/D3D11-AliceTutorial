#include "App.h"


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	App App;  // 생성자에서 아이콘,윈도우 이름만 바꾼다
	return App.Run(hInstance);
}
