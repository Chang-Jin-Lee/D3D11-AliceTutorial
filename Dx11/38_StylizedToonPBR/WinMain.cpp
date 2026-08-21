#include "App.h"

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR commandLine,
    _In_ int showCommand)
{
    (void)hPrevInstance;
    (void)commandLine;
    (void)showCommand;

    App app;
    app.SetClientSize(1600, 900);
    return app.Run(hInstance) ? 0 : 1;
}
