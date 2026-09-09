#include "App.h"
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int)
{
    // Keep requested client pixels and the D3D backbuffer at native resolution on scaled desktops.
    SetProcessDPIAware();
    App app;
    app.SetClientSize(1600, 900);
    return app.Run(instance) ? 0 : 1;
}
