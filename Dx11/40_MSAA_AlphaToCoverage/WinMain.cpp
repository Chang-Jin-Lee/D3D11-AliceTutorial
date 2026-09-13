#include "App.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int)
{
    SetProcessDPIAware();
    Coverage40::App app;
    app.SetClientSize(1600, 900);
    return app.Run(instance) ? 0 : 1;
}
