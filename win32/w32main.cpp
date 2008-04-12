#include <windows.h>
#include <commctrl.h>
#include <gl/gl.h> 
#include <gl/glu.h> 
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "solvespace.h"

#define FREEZE_SUBKEY "SolveSpace"
#include "freeze.h"

#define MIN_COLS    42
#define TEXT_HEIGHT 19
#define TEXT_WIDTH  10

HINSTANCE Instance;

HWND TextWnd;
HWND TextWndScrollBar;
int TextWndScrollPos;
int TextWndRows;

HWND GraphicsWnd;
HMENU SubMenus[100];
struct {
    int x, y;
} LastMousePos;

int ClientIsSmallerBy;

HFONT FixedFont, LinkFont;

void dbp(char *str, ...)
{
    va_list f;
    char buf[1024];
    va_start(f, str);
    vsprintf(buf, str, f);
    OutputDebugString(buf);
    OutputDebugString("\n");
}

static void PaintTextWnd(HDC hdc)
{
    RECT rect;
    GetClientRect(TextWnd, &rect);

    // Set up the back-buffer
    HDC backDc = CreateCompatibleDC(hdc);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    HBITMAP backBitmap = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(backDc, backBitmap);

    HBRUSH hbr = CreateSolidBrush(SS.TW.COLOR_BG_DEFAULT);
    FillRect(backDc, &rect, hbr);

    SelectObject(backDc, FixedFont);
    SetBkColor(backDc, SS.TW.COLOR_BG_DEFAULT);

    int rows = height / TEXT_HEIGHT;
    TextWndRows = rows;

    TextWndScrollPos = min(TextWndScrollPos, SS.TW.rows - rows);
    TextWndScrollPos = max(TextWndScrollPos, 0);

    // Let's set up the scroll bar first
    SCROLLINFO si;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_DISABLENOSCROLL | SIF_ALL;
    si.nMin = 0;
    si.nMax = SS.TW.rows - 1;
    si.nPos = TextWndScrollPos;
    si.nPage = rows;
    SetScrollInfo(TextWndScrollBar, SB_CTL, &si, TRUE);

    int r, c;
    for(r = TextWndScrollPos; r < (TextWndScrollPos+rows); r++) {
        if(r < 0) continue;
        if(r >= SS.TW.MAX_ROWS) continue;

        for(c = 0; c < SS.TW.MAX_COLS; c++) {
            char v = '0' + (c % 10);
            int color = SS.TW.meta[r][c].color;
            SetTextColor(backDc, SS.TW.colors[color].fg);
            SetBkColor(backDc, SS.TW.colors[color].bg);

            if(SS.TW.meta[r][c].link) {
                SelectObject(backDc, LinkFont);
            } else {
                SelectObject(backDc, FixedFont);
            }
            TextOut(backDc, 4 + c*TEXT_WIDTH,
                (r-TextWndScrollPos)*TEXT_HEIGHT + 1,
                (char *)&(SS.TW.text[r][c]), 1);
        }
    }

    // And commit the back buffer
    BitBlt(hdc, 0, 0, width, height, backDc, 0, 0, SRCCOPY);
    DeleteObject(backBitmap);
    DeleteObject(hbr);
    DeleteDC(backDc);
}

void HandleTextWindowScrollBar(WPARAM wParam, LPARAM lParam)
{
    int prevPos = TextWndScrollPos;
    switch(LOWORD(wParam)) {
        case SB_LINEUP:         TextWndScrollPos--; break;
        case SB_PAGEUP:         TextWndScrollPos -= 4; break;

        case SB_LINEDOWN:       TextWndScrollPos++; break;
        case SB_PAGEDOWN:       TextWndScrollPos += 4; break;

        case SB_TOP:            TextWndScrollPos = 0; break;

        case SB_BOTTOM:         TextWndScrollPos = SS.TW.rows; break;

        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:  TextWndScrollPos = HIWORD(wParam); break;
    }
    TextWndScrollPos = max(0, TextWndScrollPos);
    TextWndScrollPos = min(SS.TW.rows - TextWndRows, TextWndScrollPos);
    if(prevPos != TextWndScrollPos) {
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        si.nPos = TextWndScrollPos;
        SetScrollInfo(TextWndScrollBar, SB_CTL, &si, TRUE);

        InvalidateRect(TextWnd, NULL, FALSE);
    }
}

LRESULT CALLBACK TextWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintTextWnd(hdc);
            EndPaint(hwnd, &ps);
            break;
        }
        
        case WM_SIZING: {
            RECT *r = (RECT *)lParam;
            int hc = (r->bottom - r->top) - ClientIsSmallerBy;
            int extra = hc % TEXT_HEIGHT;
            switch(wParam) {
                case WMSZ_BOTTOM:
                case WMSZ_BOTTOMLEFT:
                case WMSZ_BOTTOMRIGHT:
                    r->bottom -= extra;
                    break;

                case WMSZ_TOP:
                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                    r->top += extra;
                    break;
            }
            int tooNarrow = (MIN_COLS*TEXT_WIDTH) - (r->right - r->left);
            if(tooNarrow >= 0) {
                switch(wParam) {
                    case WMSZ_RIGHT:
                    case WMSZ_BOTTOMRIGHT:
                    case WMSZ_TOPRIGHT:
                        r->right += tooNarrow;
                        break;

                    case WMSZ_LEFT:
                    case WMSZ_BOTTOMLEFT:
                    case WMSZ_TOPLEFT:
                        r->left -= tooNarrow;
                        break;
                }
            }
            break;
        }

        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // Find the corresponding character in the text buffer
            int r = (y / TEXT_HEIGHT);
            int c = (x / TEXT_WIDTH);
            if(msg == WM_MOUSEMOVE && r >= TextWndRows) {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                break;
            }
            r += TextWndScrollPos;

            if(msg == WM_MOUSEMOVE) {
                if(SS.TW.meta[r][c].link) {
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                } else {
                    SetCursor(LoadCursor(NULL, IDC_ARROW));
                }
            } else {
                if(SS.TW.meta[r][c].link && SS.TW.meta[r][c].f) {
                    (SS.TW.meta[r][c].f)(
                        SS.TW.meta[r][c].link,
                        SS.TW.meta[r][c].data
                    );
                }
            }
            break;
        }
        
        case WM_SIZE: {
            RECT r;
            GetWindowRect(TextWndScrollBar, &r);
            int sw = r.right - r.left;
            GetClientRect(hwnd, &r);
            MoveWindow(TextWndScrollBar, r.right - sw, r.top, sw,
                (r.bottom - r.top), TRUE);
            InvalidateRect(TextWnd, NULL, FALSE);
            break;
        }

        case WM_VSCROLL:
            HandleTextWindowScrollBar(wParam, lParam);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 1;
}

static HGLRC CreateGlContext(HDC hdc)
{
    PIXELFORMATDESCRIPTOR pfd;
    int pixelFormat; 

    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR); 
    pfd.nVersion = 1; 
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |  
                        PFD_DOUBLEBUFFER; 
    pfd.dwLayerMask = PFD_MAIN_PLANE; 
    pfd.iPixelType = PFD_TYPE_RGBA; 
    pfd.cColorBits = 8; 
    pfd.cDepthBits = 16; 
    pfd.cAccumBits = 0; 
    pfd.cStencilBits = 0; 
 
    pixelFormat = ChoosePixelFormat(hdc, &pfd); 
    if(!pixelFormat) oops();
 
    if(!SetPixelFormat(hdc, pixelFormat, &pfd)) oops();

    HGLRC hgrc = wglCreateContext(hdc); 
    wglMakeCurrent(hdc, hgrc); 

    return hgrc;
}

void InvalidateGraphics(void)
{
    InvalidateRect(GraphicsWnd, NULL, FALSE);
}
void InvalidateText(void)
{
    InvalidateRect(TextWnd, NULL, FALSE);
}

LRESULT CALLBACK GraphicsWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                            LPARAM lParam)
{
    switch (msg) {
        case WM_ERASEBKGND:
            break;

        case WM_SIZE:
            InvalidateRect(GraphicsWnd, NULL, FALSE);
            break;

        case WM_PAINT: {
            InvalidateRect(GraphicsWnd, NULL, FALSE);
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            HGLRC hgrc = CreateGlContext(hdc);

            RECT r;
            GetClientRect(GraphicsWnd, &r);
            int w = r.right - r.left;
            int h = r.bottom - r.top;

            SS.GW.Paint(w, h);
    
            SwapBuffers(hdc);

            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(hgrc);

            EndPaint(hwnd, &ps);
            break;
        }

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            RECT r;
            GetClientRect(GraphicsWnd, &r);
            x = x - (r.right - r.left)/2;
            y = (r.bottom - r.top)/2 - y;

            LastMousePos.x = x;
            LastMousePos.y = y;

            if(msg == WM_LBUTTONDOWN) {
                SS.GW.MouseLeftDown(x, y);
            } else if(msg == WM_MBUTTONDOWN) {
                SS.GW.MouseMiddleDown(x, y);
            } else if(msg == WM_MOUSEMOVE) {
                SS.GW.MouseMoved(x, y,
                    !!(wParam & MK_LBUTTON),
                    !!(wParam & MK_MBUTTON),
                    !!(wParam & MK_RBUTTON),
                    !!(wParam & MK_SHIFT),
                    !!(wParam & MK_CONTROL));
            } else {
                oops();
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            SS.GW.MouseScroll(LastMousePos.x, LastMousePos.y, delta);
            break;
        }

        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            return 1;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 1;
}

HMENU CreateGraphicsWindowMenus(void)
{
    HMENU top = CreateMenu();
    HMENU m;

    int i;
    int subMenu = 0;
    
    for(i = 0; SS.GW.menu[i].level >= 0; i++) {
        if(SS.GW.menu[i].level == 0) {
            m = CreateMenu();
            AppendMenu(top, MF_STRING | MF_POPUP, (UINT_PTR)m, 
                                                        SS.GW.menu[i].label);

            if(subMenu >= arraylen(SubMenus)) oops();
            SubMenus[subMenu] = m;
            subMenu++;
        } else {
            if(SS.GW.menu[i].label) {
                AppendMenu(m, MF_STRING, SS.GW.menu[i].id, SS.GW.menu[i].label);
            } else {
                AppendMenu(m, MF_SEPARATOR, SS.GW.menu[i].id, "");
            }
        }
    }

    return top;
}

static void CreateMainWindows(void)
{
    WNDCLASSEX wc;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);

    // The graphics window, where the sketch is drawn and shown.
    wc.style            = CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_OWNDC |
                          CS_DBLCLKS;
    wc.lpfnWndProc      = (WNDPROC)GraphicsWndProc;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1); 
    wc.lpszClassName    = "GraphicsWnd";
    wc.lpszMenuName     = NULL;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon            = NULL;
    wc.hIconSm          = NULL;
    if(!RegisterClassEx(&wc)) oops();

    HMENU top = CreateGraphicsWindowMenus();
    GraphicsWnd = CreateWindowEx(0, "GraphicsWnd",
        "SolveSpace (Graphics Window)",
        WS_OVERLAPPED | WS_THICKFRAME | WS_CLIPCHILDREN | WS_MAXIMIZEBOX |
        WS_MINIMIZEBOX | WS_SYSMENU | WS_SIZEBOX | WS_CLIPSIBLINGS,
        600, 300, 200, 200, NULL, top, Instance, NULL);
    if(!GraphicsWnd) oops();


    // The text window, with a comand line and some textual information
    // about the sketch.
    wc.style           &= ~CS_DBLCLKS;
    wc.lpfnWndProc      = (WNDPROC)TextWndProc;
    wc.hbrBackground    = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName    = "TextWnd";
    wc.hCursor          = NULL;
    if(!RegisterClassEx(&wc)) oops();

    // We get the desired Alt+Tab behaviour by specifying that the text
    // window is a child of the graphics window.
    TextWnd = CreateWindowEx(0, 
        "TextWnd", "SolveSpace (Text Window)",
        WS_THICKFRAME | WS_CLIPCHILDREN,
        10, 10, 600, 300, GraphicsWnd, (HMENU)NULL, Instance, NULL);
    if(!TextWnd) oops();

    TextWndScrollBar = CreateWindowEx(0, WC_SCROLLBAR, "", WS_CHILD |
        SBS_VERT | SBS_LEFTALIGN | WS_VISIBLE | WS_CLIPSIBLINGS, 
        200, 100, 100, 100, TextWnd, NULL, Instance, NULL);
    // Force the scrollbar to get resized to the window,
    TextWndProc(TextWnd, WM_SIZE, 0, 0);


    RECT r, rc;
    GetWindowRect(TextWnd, &r);
    GetClientRect(TextWnd, &rc);
    ClientIsSmallerBy = (r.bottom - r.top) - (rc.bottom - rc.top);
}

//-----------------------------------------------------------------------------
// Entry point into the program.
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, INT nCmdShow)
{
    Instance = hInstance;

    // Create the root windows: one for control, with text, and one for
    // the graphics
    CreateMainWindows();

    ThawWindowPos(TextWnd);
    ThawWindowPos(GraphicsWnd);

    // A monospaced font
    FixedFont = CreateFont(TEXT_HEIGHT-2, TEXT_WIDTH, 0, 0, FW_REGULAR, FALSE,
        FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FF_DONTCARE, "Lucida Console");
    LinkFont = CreateFont(TEXT_HEIGHT-2, TEXT_WIDTH, 0, 0, FW_REGULAR, FALSE,
        TRUE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FF_DONTCARE, "Lucida Console");
    if(!FixedFont)
        FixedFont = (HFONT)GetStockObject(SYSTEM_FONT);
    if(!LinkFont)
        LinkFont = (HFONT)GetStockObject(SYSTEM_FONT);

    // Call in to the platform-independent code, and let them do their init
    SS.Init();

    ShowWindow(TextWnd, SW_SHOWNOACTIVATE);
    ShowWindow(GraphicsWnd, SW_SHOW);
   
    // And now it's the message loop. All calls in to the rest of the code
    // will be from the wndprocs.
    MSG msg;
    DWORD ret;
    while(ret = GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    FreezeWindowPos(TextWnd);
    FreezeWindowPos(GraphicsWnd);

    return 0;
}
