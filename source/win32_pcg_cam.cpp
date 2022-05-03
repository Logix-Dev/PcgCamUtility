/*
    ==========================================================================
    File: win32_pcg_cam.cpp
    Date: 02/05/2022
    Creator: Logix
    Version: 1.1
    ==========================================================================

    This is a simple utility program to select a region of the screen with your cursor
    and receive the edge-relative coordinates of the drawn rectangle, for the purpose
    of assisting with setting up cameras in OBS Studio / Streamlabs OBS Desktop.

    CHANGELOG:
    v1.0:
        - Implemented the program!
    v1.1:
        - Fixed the coordinates being offset by the monitor position.
        - Switched to using the work area instead of the entire screen, since OBS wants window-relative
            coordinates, not absolute screen coordinates, and the taskbar should be excluded.
        - As a positive side-effect of previously mentioned fixes, the lines are now drawn in the correct
            positions on non-primary monitors.

    TODO
      - [✓] Prevent flickering
      - [✓] Documentation pass
      - [ ] Remove as many globalvars as possible
      - [ ] Remove all localpersists
*/

// TODO: https://stackoverflow.com/questions/62252362/winapi-how-to-draw-opaque-text-on-a-transparent-window-background

#ifdef UNICODE
#undef UNICODE
#endif

#include <Windows.h>
#include <windowsx.h>
#include <stdint.h>
#include <string>
#include <gdiplus.h>
#include <uxtheme.h>

#define globalvar static
#define internal static
#define localpersist static

typedef uint32_t  u32;
typedef int32_t   i32;
typedef uint32_t  b32;
typedef float     r32;

struct rect2i
{
    i32 X;
    i32 Y;
};

struct pcg_cam_result
{
    b32 IsValid;
    i32 Left;
    i32 Top;
    i32 Right;
    i32 Bottom;
};

#define Min(A, B) ((A) < (B) ? (A) : (B))
#define Max(A, B) ((A) > (B) ? (A) : (B))

/// The minimum size for a selection box (not currently DPI-friendly).
const i32 MinSize = 32;

globalvar WINDOWPLACEMENT G_WindowPosition = { sizeof(G_WindowPosition) };
globalvar b32 G_Running;
globalvar b32 G_HasDrawnSelection;
globalvar b32 G_IsDrawingSelection;
globalvar POINT G_SelectionStart;
globalvar POINT G_SelectionEnd;
globalvar b32 G_SelectionIsValid;
globalvar HMONITOR G_WindowMonitor;
globalvar r32 G_WorkAreaW;
globalvar r32 G_WorkAreaH;

/// Returns whether the two given points have different X -or- Y coordinates.
internal b32 ArePointsDifferent(POINT A, POINT B)
{
    return A.x != B.x || A.y != B.y;
}

/// Makes the given window cover the entire screen (including the TaskBar).
internal void ToggleWindowFullScreen(HWND Window)
{
    // NOTE: https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
    DWORD Style = GetWindowLongA(Window, GWL_STYLE);
    if (Style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO MonitorInfo = { sizeof(MONITORINFO) };
        if (GetWindowPlacement(Window, &G_WindowPosition) &&
            GetMonitorInfoA(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
        {
            // NOTE: Make the window cover the entire screen
            SetWindowLongA(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP,
                         MonitorInfo.rcWork.left, MonitorInfo.rcWork.top,
                         MonitorInfo.rcWork.right - MonitorInfo.rcWork.left,
                         MonitorInfo.rcWork.bottom - MonitorInfo.rcWork.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
}

/// Converts the current selection into a RECT structure.
internal RECT GetSelectionRect()
{
    RECT SelectionRect;

    SelectionRect.left = G_SelectionStart.x;
    SelectionRect.top = G_SelectionStart.y;
    SelectionRect.right = G_SelectionEnd.x;
    SelectionRect.bottom = G_SelectionEnd.y;

    return SelectionRect;
}

/// Draws the selection box and additional on-screen information using GDI and GDI+.
internal void PaintSelection(HDC DeviceContext, rect2i Start, rect2i End)
{
    Gdiplus::Graphics Graphics(DeviceContext);

    // NOTE: Setup the dashed line pen
    Gdiplus::Pen DashedPen(G_SelectionIsValid ? Gdiplus::Color(255, 79, 223, 78) : Gdiplus::Color(255, 223, 78, 79));
    DashedPen.SetWidth(3.0f);
    DashedPen.SetDashStyle(Gdiplus::DashStyle::DashStyleDash);
    DashedPen.SetDashOffset(32.0f);
    DashedPen.SetDashCap(Gdiplus::DashCap::DashCapRound);

    if (G_IsDrawingSelection || G_HasDrawnSelection)
    {
        // NOTE: Fill selection rectangle
        HBRUSH Brush = CreateSolidBrush(RGB(50, 50, 50));
        RECT SelectionRect = GetSelectionRect();
        FillRect(DeviceContext, &SelectionRect, Brush);

        // NOTE: Selection rectangle dashed outline
        Graphics.DrawLine(&DashedPen, Start.X, Start.Y, End.X, Start.Y); // NOTE: Top
        Graphics.DrawLine(&DashedPen, Start.X, Start.Y, Start.X, End.Y); // NOTE: Left
        Graphics.DrawLine(&DashedPen, Start.X, End.Y, End.X, End.Y); // NOTE: Bottom
        Graphics.DrawLine(&DashedPen, End.X, Start.Y, End.X, End.Y); // NOTE: Right
    }

    // NOTE: TextBox properties
    // TODO: Move to global constants
    localpersist r32 TextBoxW = 116.0f;
    localpersist r32 TextBoxH = 32.0f;
    localpersist r32 HalfTextBoxW = TextBoxW / 2.0f;
    localpersist r32 HalfTextBoxH = TextBoxH / 2.0f;

    // NOTE: Font setup
    // TODO: Find a way to check this font is installed on the system, fallback to 'Times New Roman' if not
    Gdiplus::FontFamily FontFamily(L"Ubuntu");
    Gdiplus::Font Font = Gdiplus::Font(&FontFamily, 24, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush TextBrush(Gdiplus::Color(255, 255, 255, 255));
    Gdiplus::SolidBrush EvilTextBrush(Gdiplus::Color(255, 223, 78, 79));
    Gdiplus::SolidBrush HintTextBrush(Gdiplus::Color(255, 236, 206, 91));
    Gdiplus::StringFormat BottomRightAligned;
    BottomRightAligned.SetAlignment(Gdiplus::StringAlignmentFar);
    BottomRightAligned.SetLineAlignment(Gdiplus::StringAlignmentFar);
    Gdiplus::StringFormat CenterAligned;
    CenterAligned.SetAlignment(Gdiplus::StringAlignmentCenter);
    CenterAligned.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    if (G_IsDrawingSelection)
    {
        if (G_SelectionIsValid)
        {
            #if PCG_INTERNAL && 0  // TOGGLE: Enable start/end coordinates for debugging
            // NOTE: Draw start coordinates
            {
                Gdiplus::RectF StartRect((r32)G_SelectionStart.x - TextBoxW, (r32)G_SelectionStart.y - TextBoxH, TextBoxW, TextBoxH);
                std::wstring StartCoordinates = std::to_wstring(G_SelectionStart.x) + L", " + std::to_wstring(G_SelectionStart.y);
                Graphics.DrawString(StartCoordinates.c_str(), -1, &Font, StartRect, &BottomRightAligned, &TextBrush);
            }

            // NOTE: Draw end coordinates
            {
                Gdiplus::RectF EndRect((r32)G_SelectionEnd.x, (r32)G_SelectionEnd.y, TextBoxW, TextBoxH);
                std::wstring EndCoordinates = std::to_wstring(G_SelectionEnd.x) + L", " + std::to_wstring(G_SelectionEnd.y);
                Graphics.DrawString(EndCoordinates.c_str(), -1, &Font, EndRect, 0, &TextBrush);
            }
            #endif

            i32 LinePadding = 8;
            DashedPen.SetColor(Gdiplus::Color(255, 255, 255, 255));

            // NOTE: X and Y are relative to the center of the TextBox

            // NOTE: Draw distance to left screen edge
            {
                i32 DistanceValue = G_SelectionStart.x;
                r32 X = (r32)(G_SelectionStart.x / 2);
                r32 Y = (r32)(G_SelectionEnd.y - ((G_SelectionEnd.y - G_SelectionStart.y) / 2));

                // NOTE: TextBox
                Gdiplus::RectF Rect(X - HalfTextBoxW, Y - HalfTextBoxH, TextBoxW, TextBoxH);
                std::wstring DistanceString = std::to_wstring(DistanceValue) + L" px";
                Graphics.DrawString(DistanceString.c_str(), -1, &Font, Rect, &CenterAligned, &TextBrush);

                // NOTE: Left dashed line
                Graphics.DrawLine(&DashedPen, LinePadding, (i32)Y, (G_SelectionStart.x / 2) - (i32)HalfTextBoxW + LinePadding, (i32)Y);
                // NOTE: Right dashed line
                Graphics.DrawLine(&DashedPen, (G_SelectionStart.x / 2) + (i32)HalfTextBoxW + LinePadding, (i32)Y, G_SelectionStart.x - LinePadding, (i32)Y);
            }

            // NOTE: Draw distance to right screen edge
            {
                i32 DistanceValue = (i32)G_WorkAreaW - G_SelectionEnd.x;
                r32 X = (r32)(G_SelectionEnd.x + (DistanceValue / 2));
                r32 Y = (r32)(G_SelectionEnd.y - ((G_SelectionEnd.y - G_SelectionStart.y) / 2));

                // NOTE: TextBox
                Gdiplus::RectF Rect(X - HalfTextBoxW, Y - HalfTextBoxH, TextBoxW, TextBoxH);
                std::wstring DistanceString = std::to_wstring(DistanceValue) + L" px";
                Graphics.DrawString(DistanceString.c_str(), -1, &Font, Rect, &CenterAligned, &TextBrush);

                // NOTE: Left dashed line
                Graphics.DrawLine(&DashedPen, G_SelectionEnd.x + LinePadding, (i32)Y, G_SelectionEnd.x + (DistanceValue / 2) - (i32)HalfTextBoxW - LinePadding, (i32)Y);
                // NOTE: Right dashed line
                Graphics.DrawLine(&DashedPen, (i32)X + (i32)HalfTextBoxW + LinePadding, (i32)Y, (i32)G_WorkAreaW - LinePadding, (i32)Y);
            }

            // NOTE: Draw distance to top screen edge
            {
                i32 DistanceValue = G_SelectionStart.y;
                r32 X = (r32)(G_SelectionStart.x + ((G_SelectionEnd.x - G_SelectionStart.x) / 2));
                r32 Y = (r32)(G_SelectionStart.y / 2);

                // NOTE: TextBox
                Gdiplus::RectF Rect(X - HalfTextBoxW, Y - HalfTextBoxH, TextBoxW, TextBoxH);
                std::wstring DistanceString = std::to_wstring(DistanceValue) + L" px";
                Graphics.DrawString(DistanceString.c_str(), -1, &Font, Rect, &CenterAligned, &TextBrush);

                // NOTE: Top dashed line
                Graphics.DrawLine(&DashedPen, (i32)X, LinePadding, (i32)X, (DistanceValue / 2) - LinePadding - (i32)HalfTextBoxH);
                // NOTE: Bottom dashed line
                Graphics.DrawLine(&DashedPen, (i32)X, (DistanceValue / 2) + LinePadding + (i32)HalfTextBoxH, (i32)X, DistanceValue - LinePadding);
            }

            // NOTE: Draw distance to bottom screen edge
            {
                i32 DistanceValue = (i32)G_WorkAreaH - G_SelectionEnd.y;
                r32 X = (r32)(G_SelectionStart.x + ((G_SelectionEnd.x - G_SelectionStart.x) / 2));
                r32 Y = (r32)(G_SelectionEnd.y + ((G_WorkAreaH - G_SelectionEnd.y) / 2));

                // NOTE: TextBox
                Gdiplus::RectF Rect(X - HalfTextBoxW, Y - HalfTextBoxH, TextBoxW, TextBoxH);
                std::wstring DistanceString = std::to_wstring(DistanceValue) + L" px";
                Graphics.DrawString(DistanceString.c_str(), -1, &Font, Rect, &CenterAligned, &TextBrush);

                // NOTE: Top dashed line
                Graphics.DrawLine(&DashedPen, (i32)X, G_SelectionEnd.y + LinePadding, (i32)X, (i32)G_WorkAreaH - (DistanceValue / 2) - (i32)HalfTextBoxH - LinePadding);
                // NOTE: Bottom dashed line
                Graphics.DrawLine(&DashedPen, (i32)X, (i32)G_WorkAreaH - (DistanceValue / 2) + (i32)HalfTextBoxH + LinePadding, (i32)X, (i32)G_WorkAreaH - LinePadding);
            }
        }
        else
        {
            Gdiplus::StringFormat CenterBottomAligned;
            CenterBottomAligned.SetAlignment(Gdiplus::StringAlignmentCenter);
            CenterBottomAligned.SetLineAlignment(Gdiplus::StringAlignmentFar);

            Gdiplus::RectF InvalidRectRect(0.0f, 0.0f, G_WorkAreaW, G_WorkAreaH - 80.0f);
            std::wstring InvalidRectText = L"Invalid Rectangle! Must be larger than " + std::to_wstring(MinSize) + L" x " + std::to_wstring(MinSize) + L" pixels";
            Graphics.DrawString(InvalidRectText.c_str(), -1, &Font, InvalidRectRect, &CenterBottomAligned, &EvilTextBrush);
        }
    }
    else
    {
        Gdiplus::StringFormat CenterBottomAligned;
        CenterBottomAligned.SetAlignment(Gdiplus::StringAlignmentCenter);
        CenterBottomAligned.SetLineAlignment(Gdiplus::StringAlignmentFar);

        Gdiplus::RectF InvalidRectRect(0.0f, 0.0f, G_WorkAreaW, G_WorkAreaH - 80.0f);
        std::wstring InvalidRectText = L"Click and drag to draw a selection, or press [Escape] or [Right Mouse Button] to cancel";
        Graphics.DrawString(InvalidRectText.c_str(), -1, &Font, InvalidRectRect, &CenterBottomAligned, &HintTextBrush);
    }
}

internal void Paint(HDC &DeviceContext, RECT ClientRect)
{
    // NOTE: Draw the translucent window background
    localpersist HBRUSH WindowBGBrush = CreateSolidBrush(RGB(20, 20, 20));
    FillRect(DeviceContext, &ClientRect, WindowBGBrush);

    if (G_IsDrawingSelection)
    {
        // NOTE: Draw the selection rectangle outline
        rect2i Start;
        Start.X = Min(G_SelectionStart.x, G_SelectionEnd.x);
        Start.Y = Min(G_SelectionStart.y, G_SelectionEnd.y);

        rect2i End;
        End.X = Max(G_SelectionStart.x, G_SelectionEnd.x);
        End.Y = Max(G_SelectionStart.y, G_SelectionEnd.y);

        PaintSelection(DeviceContext, Start, End);
    }
    else
    {
        rect2i Start = { };
        rect2i End = { };
        PaintSelection(DeviceContext, Start, End);
    }
}

internal void Repaint(HWND Window)
{
    InvalidateRect(Window, 0, TRUE);
}

// NOTE: Suppress 'unreferenced formal parameter' warning for Window; it is used for Repaint()
#pragma warning ( push )
#pragma warning ( disable: 4100 )
/// Updates the selection rectangle and issues a redraw request if it has changed.
internal void UpdateSelection(HWND Window)
#pragma warning ( pop )
{
    POINT PreviousStart = G_SelectionStart;
    POINT PreviousEnd = G_SelectionEnd;

    POINT PointA = G_SelectionStart;
    POINT PointB;
    GetCursorPos(&PointB);
    ScreenToClient(Window, &PointB);

    G_SelectionStart = PointA;
    G_SelectionEnd = PointB;

    if (ArePointsDifferent(PreviousStart, G_SelectionStart) || ArePointsDifferent(PreviousEnd, G_SelectionEnd))
    {
        G_SelectionIsValid = ((G_SelectionEnd.x - G_SelectionStart.x) >= MinSize &&
                              (G_SelectionEnd.y - G_SelectionStart.y) >= MinSize);

        #if PCG_ATTEMPT_VSYNC == 0
        // NOTE: Invalidate the entire client area, to force a redraw
        Repaint(Window);
        #endif
    }
}

internal void UpdateMonitorStats(HWND Window)
{
    G_WindowMonitor = MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO MonitorInfo = { sizeof(MONITORINFO) };
    if (GetMonitorInfoA(G_WindowMonitor, &MonitorInfo))
    {
        G_WorkAreaW = (r32)(MonitorInfo.rcWork.right - MonitorInfo.rcWork.left);
        G_WorkAreaH = (r32)(MonitorInfo.rcWork.bottom - MonitorInfo.rcWork.top);

        #if PCG_INTERNAL
        std::string MonitorStats = "Monitor size: " + std::to_string((i32)G_WorkAreaW) + " x " + std::to_string((i32)G_WorkAreaH) + " px\n";
        OutputDebugStringA(MonitorStats.c_str());
        #endif
    }
    else
    {
        #if PCG_INTERNAL
        OutputDebugStringA("ERROR: Failed to update the monitor stats!\n");
        #endif
    }
}

/// Updates the window position (for when the window should move to the monitor the cursor is on).
internal void UpdateWindowPosition(HWND Window)
{
    MONITORINFO MonitorInfo = { sizeof(MONITORINFO) };
    POINT Cursor;
    GetCursorPos(&Cursor);
    // NOTE: Get the monitor the cursor is on (or the closest one)
    HMONITOR Monitor = MonitorFromPoint(Cursor, MONITOR_DEFAULTTONEAREST);
    if (G_WindowMonitor != Monitor)
    {
        if (GetMonitorInfoA(Monitor, &MonitorInfo))
        {
            // NOTE: Move the window onto the montior the cursor moved to
            SetWindowPos(Window, HWND_TOP,
                         MonitorInfo.rcWork.left, MonitorInfo.rcWork.top,
                         MonitorInfo.rcWork.right - MonitorInfo.rcWork.left,
                         MonitorInfo.rcWork.bottom - MonitorInfo.rcWork.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            G_WindowMonitor = Monitor;
            UpdateMonitorStats(Window);
        }
        else
        {
            #if PCG_INTERNAL
            OutputDebugStringA("ERROR: Failed to get the monitor info!\n");
            #endif
        }
    }
}

LRESULT CALLBACK PcgCamUtilityProcedure(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    localpersist BOOL TrackingMouse = FALSE;

    switch (Message)
    {
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            const u32 VKCode = (u32)WParam;
            const b32 WasDown = ((LParam & (1 << 30)) != 0);
            const b32 IsDown = ((LParam & (1 << 31)) == 0);
            const b32 AltModifier = ((LParam & (1 << 29)) != 0);

            if (IsDown != WasDown)
            {
                // NOTE: Alt-F4 functionality
                if (VKCode == VK_F4 && AltModifier)
                {
                    PostQuitMessage(0);
                    G_Running = false;
                }

                // NOTE: Cancel and close on ESCAPE pressed
                if (VKCode == VK_ESCAPE)
                {
                    PostQuitMessage(0);
                    G_Running = false;
                }
            }
        }
        break;
        case WM_CLOSE:
        case WM_DESTROY:
        {
            G_Running = false;
            PostQuitMessage(0);
        }
        break;
        case WM_TIMER:
        {
            Repaint(Window);
        }
        break;
        case WM_LBUTTONDOWN:
        case WM_NCLBUTTONDOWN:
        {
            if (!G_IsDrawingSelection)
            {
                GetCursorPos(&G_SelectionStart);
                ScreenToClient(Window, &G_SelectionStart);
                G_IsDrawingSelection = true;
                UpdateSelection(Window);
            }
        }
        break;
        case WM_LBUTTONUP:
        case WM_NCLBUTTONUP:
        {
            if (G_IsDrawingSelection)
            {
                G_IsDrawingSelection = false;
                G_HasDrawnSelection = true; // TODO: Remove this?
                UpdateSelection(Window); // NOTE: Needed to ensure G_SelectionIsValid is accurate

                if (G_HasDrawnSelection && G_SelectionIsValid)
                {
                    #if PCG_INTERNAL
                    OutputDebugStringA("Selection is valid\n");
                    #endif

                    i32 Left = G_SelectionStart.x;
                    i32 Top = G_SelectionStart.y;
                    i32 Right = (i32)G_WorkAreaW - G_SelectionEnd.x;
                    i32 Bottom = (i32)G_WorkAreaH - G_SelectionEnd.y;

                    std::string ResultMessage =
                        "Left:\t  " + std::to_string(Left) +
                        "\nTop:\t  " + std::to_string(Top) +
                        "\nRight:\t  " + std::to_string(Right) +
                        "\nBottom:\t  " + std::to_string(Bottom) +
                        "                                          "; // NOTE: Widen the box a little

                    // NOTE: Make the window invisible
                    SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 0, LWA_ALPHA);

                    // NOTE: Show the data to the user
                    // TODO: Find a way to make this wider?
                    MessageBox(Window, ResultMessage.c_str(), "PCG Cam Utility Results", MB_OK | MB_TOPMOST);

                    // NOTE: Close the app
                    G_Running = false;
                }
                else
                {
                    #if PCG_INTERNAL
                    OutputDebugStringA("Selection is invalid\n");
                    #endif
                    G_HasDrawnSelection = false;
                    G_SelectionStart = { };
                    G_SelectionEnd = { };
                    Repaint(Window);
                }
            }
        }
        break;
        case WM_RBUTTONUP:
        case WM_NCRBUTTONUP:
        {
            G_Running = false;
        }
        break;
        case WM_MOUSELEAVE:
        {
            UpdateWindowPosition(Window);

            // NOTE: We need to start tracking the mouse again-- apparently this is a one-shot deal
            TrackingMouse = false;
        }
        break;
        case WM_MOUSEMOVE:
        {
            // NOTE: Track when the cursor leaves the window - this is how we move the window to the correct monitor
            if (!TrackingMouse)
            {
                TRACKMOUSEEVENT _TrackMouseEvent = { };
                _TrackMouseEvent.cbSize = sizeof(TRACKMOUSEEVENT);
                _TrackMouseEvent.dwFlags = TME_LEAVE;
                _TrackMouseEvent.hwndTrack = Window;

                TrackMouseEvent(&_TrackMouseEvent);

                TrackingMouse = true;
            }

            if (G_IsDrawingSelection)
            {
                UpdateSelection(Window);
            }
        }
        break;
        case WM_PAINT:
        {
            PAINTSTRUCT PaintStruct;
            HDC DeviceContext = BeginPaint(Window, &PaintStruct);

            RECT ClientRect;
            GetClientRect(Window, &ClientRect);

            // NOTE: Thanks to https://stackoverflow.com/a/51330038/11878570 for this solution (removes the flickering with GDI+)
            HDC MemDC;
            HPAINTBUFFER Buffer = BeginBufferedPaint(DeviceContext, &ClientRect, BPBF_COMPATIBLEBITMAP, NULL, &MemDC);
            Paint(MemDC, ClientRect);
            EndBufferedPaint(Buffer, TRUE);

            EndPaint(Window, &PaintStruct);
        }
        break;
        default:
        {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        }
        break;
    }

    return(Result);
}

i32 WinMain(HINSTANCE Instance, [[maybe_unused]] HINSTANCE PrevInstance, [[maybe_unused]] LPSTR CommandLine, [[maybe_unused]] int ShowCommand)
{
    // NOTE: Register the window class
    WNDCLASSA WindowClass = { };
    WindowClass.lpfnWndProc = PcgCamUtilityProcedure;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(0, IDC_CROSS);
    WindowClass.lpszClassName = "PcgCameraUtility";

    if (!RegisterClassA(&WindowClass))
    {
        OutputDebugStringA("Failed to register the window class!\n");
        return 1;
    }

    // NOTE: Create the window
    // NOTE: WS_EX_LAYERED allows the window to be translucent
    // NOTE: WS_EX_TOOLWINDOW hides the window from the taskbar
    HWND Window = CreateWindowExA(WS_EX_LAYERED | WS_EX_TOOLWINDOW,
                              WindowClass.lpszClassName,
                              "PCG Camera Utility",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                              NULL, NULL, Instance, NULL);
    if (!Window)
    {
        OutputDebugStringA("Failed to create the window!\n");
        return 1;
    }

    // NOTE: Initialize GDI+
    Gdiplus::GdiplusStartupInput GdiPlusStartupInput;
    ULONG_PTR GdiPlusToken;
    Gdiplus::GdiplusStartup(&GdiPlusToken, &GdiPlusStartupInput, NULL);

    // NOTE: Make window fullscreen and make it visible
    SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 128, LWA_ALPHA | LWA_COLORKEY);
    ToggleWindowFullScreen(Window);

    // NOTE: Get monitor info
    UpdateMonitorStats(Window);

    #if PCG_ATTEMPT_VSYNC
    // NOTE: Attempt to make the program refresh in time with V-Sync (this is not *ACTUALLY* V-Sync, only an approximation!)
    i32 MonitorRefreshHz = 60;
    HDC RefreshDC = GetDC(Window);
    i32 Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
    ReleaseDC(Window, RefreshDC);
    if (Win32RefreshRate > 1)
    {
        MonitorRefreshHz = Win32RefreshRate;
    }

    SetTimer(Window, 999, 1000 / MonitorRefreshHz, NULL);
    #endif

    // NOTE: Program loop
    G_Running = true;
    MSG Message;
    while (G_Running && GetMessageA(&Message, Window, 0, 0))
    {
        if (Message.message == WM_QUIT)
        {
            G_Running = false;
        }

        TranslateMessage(&Message);
        DispatchMessageA(&Message);
    }

    // NOTE: Shut down GDI+
    Gdiplus::GdiplusShutdown(GdiPlusToken);

    // NOTE: Exit
    return 0;
}