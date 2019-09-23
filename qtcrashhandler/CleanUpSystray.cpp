// See: https://stackoverflow.com/a/56088800/262458

#include <windows.h>

static void MoveMouseOverRect(RECT r, HWND w)
{
    for (LONG x = 0; x < r.right; x += 5) {
        for (LONG y = 0; y < r.bottom; y += 5) {
            SendMessage(w, WM_MOUSEMOVE, 0, (y << 16) + x);
        }
    }
}

#define FW(x,y) FindWindowExW(x, NULL, y, L"")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pCmdLine, int nCmdShow)
{
    HWND hNotificationArea;
    RECT r;

    // give crashed app time to close, so icon can be cleaned up
    Sleep(2000);

    // main systray
    GetClientRect(hNotificationArea = FindWindowExW(FW(FW(FW(NULL, L"Shell_TrayWnd"), L"TrayNotifyWnd"), L"SysPager"), NULL, L"ToolbarWindow32", NULL), &r);

    MoveMouseOverRect(r, hNotificationArea);

    // overflow area
    GetClientRect(hNotificationArea = FindWindowExW(FW(NULL, L"NotifyIconOverflowWindow"), NULL, L"ToolbarWindow32", NULL), &r);

    MoveMouseOverRect(r, hNotificationArea);

    return 0;
}
