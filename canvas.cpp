#include "paint.h"

HBITMAP g_hbm = NULL;
static BOOL s_bDragging = FALSE;
static POINT s_ptOld;
MODE g_nMode = MODE_PENCIL;
static POINT s_pt;
static HBITMAP s_hbmFloating = NULL;
static RECT s_rcFloating;

void DoNormalizeRect(RECT *prc)
{
    if (prc->left > prc->right)
        std::swap(prc->left, prc->right);
    if (prc->top > prc->bottom)
        std::swap(prc->top, prc->bottom);
}

void DoTakeOff(void)
{
    if (!s_hbmFloating)
    {
        s_hbmFloating = DoGetSubImage(g_hbm, &s_rcFloating);
        DoPutSubImage(g_hbm, &s_rcFloating, NULL);
    }
}

void DoLanding(void)
{
    if (s_hbmFloating)
    {
        DoPutSubImage(g_hbm, &s_rcFloating, s_hbmFloating);
        DeleteBitmap(s_hbmFloating);
        s_hbmFloating = NULL;
    }
    SetRectEmpty(&s_rcFloating);
}

struct ModeSelect : IMode
{
    virtual void DoLButtonDown(HWND hwnd, POINT pt)
    {
        s_pt = s_ptOld = pt;

        if (PtInRect(&s_rcFloating, pt))
        {
            DoTakeOff();
        }
        else
        {
            DoLanding();
        }
        InvalidateRect(hwnd, NULL, TRUE);
    }

    virtual void DoMouseMove(HWND hwnd, POINT pt)
    {
        if (s_hbmFloating)
        {
            OffsetRect(&s_rcFloating, pt.x - s_ptOld.x, pt.y - s_ptOld.y);
            s_pt = s_ptOld = pt;
        }
        else
        {
            s_pt = pt;
        }
        InvalidateRect(hwnd, NULL, TRUE);
    }

    virtual void DoLButtonUp(HWND hwnd, POINT pt)
    {
        if (s_hbmFloating)
        {
            OffsetRect(&s_rcFloating, pt.x - s_ptOld.x, pt.y - s_ptOld.y);
            s_pt = s_ptOld = pt;
        }
        else
        {
            s_pt = pt;
        }
        InvalidateRect(hwnd, NULL, TRUE);
    }

    virtual void DoPostPaint(HWND hwnd, HDC hDC)
    {
        RECT rc;

        if (s_hbmFloating)
            rc = s_rcFloating;
        else
            SetRect(&rc, s_ptOld.x, s_ptOld.y, s_pt.x, s_pt.y);

        DoNormalizeRect(&rc);

        if (!IsRectEmpty(&s_rcFloating) && s_hbmFloating)
        {
            if (HDC hMemDC = CreateCompatibleDC(NULL))
            {
                HBITMAP hbmOld = SelectBitmap(hMemDC, s_hbmFloating);
                {
                    BitBlt(hDC, s_rcFloating.left, s_rcFloating.top,
                        s_rcFloating.right - s_rcFloating.left,
                        s_rcFloating.bottom - s_rcFloating.top,
                        hMemDC, 0, 0, SRCCOPY);
                }
                SelectBitmap(hMemDC, hbmOld);
                DeleteDC(hMemDC);
            }
        }

        DrawFocusRect(hDC, &rc);
        s_rcFloating = rc;
    }
};

struct ModePencil : IMode
{
    virtual void DoLButtonDown(HWND hwnd, POINT pt)
    {
        s_pt = s_ptOld = pt;
    }

    virtual void DoMouseMove(HWND hwnd, POINT pt)
    {
        if (HDC hMemDC = CreateCompatibleDC(NULL))
        {
            HPEN hPenOld = SelectPen(hMemDC, GetStockPen(WHITE_PEN));
            HBITMAP hbmOld = SelectBitmap(hMemDC, g_hbm);
            MoveToEx(hMemDC, s_ptOld.x, s_ptOld.y, NULL);
            LineTo(hMemDC, pt.x, pt.y);
            SelectBitmap(hMemDC, hbmOld);
            SelectPen(hMemDC, hPenOld);

            DeleteDC(hMemDC);

            InvalidateRect(hwnd, NULL, TRUE);
        }

        s_ptOld = pt;
    }

    virtual void DoLButtonUp(HWND hwnd, POINT pt)
    {
        if (HDC hMemDC = CreateCompatibleDC(NULL))
        {
            HPEN hPenOld = SelectPen(hMemDC, GetStockPen(WHITE_PEN));
            HBITMAP hbmOld = SelectBitmap(hMemDC, g_hbm);
            MoveToEx(hMemDC, s_ptOld.x, s_ptOld.y, NULL);
            LineTo(hMemDC, pt.x, pt.y);
            SelectBitmap(hMemDC, hbmOld);
            SelectPen(hMemDC, hPenOld);

            DeleteDC(hMemDC);

            InvalidateRect(hwnd, NULL, TRUE);
        }
    }

    virtual void DoPostPaint(HWND hwnd, HDC hDC)
    {
    }
};

static IMode *s_pMode = new ModePencil();

void DoSetMode(MODE nMode)
{
    delete s_pMode;
    switch (nMode)
    {
    case MODE_SELECT:
        s_pMode = new ModeSelect();
        break;
    case MODE_PENCIL:
        s_pMode = new ModePencil();
        break;
    }
    g_nMode = nMode;
}

static void OnDelete(HWND hwnd)
{
    if (g_nMode != MODE_SELECT)
        return;

    RECT rc;
    SetRect(&rc, s_ptOld.x, s_ptOld.y, s_pt.x, s_pt.y);
    DoNormalizeRect(&rc);

    DoPutSubImage(g_hbm, &rc, NULL);
    InvalidateRect(hwnd, NULL, TRUE);
}

static void OnCopy(HWND hwnd)
{
    RECT rc;
    if (g_nMode == MODE_SELECT)
    {
        SetRect(&rc, s_ptOld.x, s_ptOld.y, s_pt.x, s_pt.y);
        DoNormalizeRect(&rc);
    }
    else
    {
        BITMAP bm;
        GetObject(g_hbm, sizeof(bm), &bm);
        SetRect(&rc, 0, 0, bm.bmWidth, bm.bmHeight);
    }

    HBITMAP hbmNew = DoGetSubImage(g_hbm, &rc);
    if (!hbmNew)
        return;

    HGLOBAL hDIB = DIBFromBitmap(hbmNew);

    if (OpenClipboard(hwnd))
    {
        EmptyClipboard();
        SetClipboardData(CF_DIB, hDIB);
        CloseClipboard();
    }

    DeleteBitmap(hbmNew);
}

static void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case ID_CUT:
        OnCopy(hwnd);
        OnDelete(hwnd);
        break;
    case ID_COPY:
        OnCopy(hwnd);
        break;
    case ID_PASTE:
        // TODO:
        break;
    case ID_DELETE:
        OnDelete(hwnd);
        break;
    case ID_SELECT_ALL:
        // TODO:
        break;
    case ID_SELECT:
        DoSetMode(MODE_SELECT);
        break;
    case ID_PENCIL:
        DoSetMode(MODE_PENCIL);
        break;
    }
}

static BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
    g_hbm = DoCreate24BppBitmap(320, 120);
    return TRUE;
}

static void OnDestroy(HWND hwnd)
{
    DeleteObject(g_hbm);
    g_hbm = NULL;
}

static void OnPaint(HWND hwnd)
{
    BITMAP bm;
    GetObject(g_hbm, sizeof(bm), &bm);

    PAINTSTRUCT ps;
    if (HDC hDC = BeginPaint(hwnd, &ps))
    {
        if (HDC hMemDC = CreateCompatibleDC(NULL))
        {
            HBITMAP hbmOld = SelectBitmap(hMemDC, g_hbm);
            BitBlt(hDC, 0, 0, bm.bmWidth, bm.bmHeight,
                   hMemDC, 0, 0, SRCCOPY);
            SelectBitmap(hMemDC, hbmOld);

            s_pMode->DoPostPaint(hwnd, hDC);

            DeleteDC(hMemDC);
        }
        EndPaint(hwnd, &ps);
    }
}

static void OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    if (fDoubleClick || s_bDragging)
        return;

    s_bDragging = TRUE;
    SetCapture(hwnd);

    POINT pt = { x, y };
    s_pMode->DoLButtonDown(hwnd, pt);
}

static void OnMouseMove(HWND hwnd, int x, int y, UINT keyFlags)
{
    if (!s_bDragging)
        return;

    POINT pt = { x, y };
    s_pMode->DoMouseMove(hwnd, pt);
}

static void OnLButtonUp(HWND hwnd, int x, int y, UINT keyFlags)
{
    if (!s_bDragging)
        return;

    POINT pt = { x, y };
    s_pMode->DoLButtonUp(hwnd, pt);

    ReleaseCapture();
    s_bDragging = FALSE;
}

LRESULT CALLBACK
CanvasWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
        HANDLE_MSG(hwnd, WM_LBUTTONDOWN, OnLButtonDown);
        HANDLE_MSG(hwnd, WM_MOUSEMOVE, OnMouseMove);
        HANDLE_MSG(hwnd, WM_LBUTTONUP, OnLButtonUp);
        case WM_CAPTURECHANGED:
            s_bDragging = FALSE;
            break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}
