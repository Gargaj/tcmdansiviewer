#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <shlwapi.h>

#ifdef __cplusplus
extern "C" {  
#endif

#include "listplug.h"

HWND hWnd = NULL;
HDC hdc = NULL;
HDC hdcSrc = NULL;
COLORREF color[16];
HBITMAP bmp = NULL;
char szINIFilename[MAX_PATH];
HINSTANCE hInstance = NULL;
int nPos = 0;
int nMaxPos = 0;
SIZE sz = { 0, 0 };
int zoom = 1;
int nFontSize = 0;
char szFontName[200] = "Fixedsys";

BOOL APIENTRY DllMain( HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved )
{
  switch (ul_reason_for_call)
  {
    case DLL_PROCESS_ATTACH:
      hInstance = (HINSTANCE)hModule;
      break;
    case DLL_PROCESS_DETACH:
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
  }
  return TRUE;
}


int nFGIntensity = 0;
int nFGColor = 7;
int nBGColor = 0;

void SetANSIColor( HDC hdc, int colorcode )
{
  if ( colorcode == 0 )
  {
    SetTextColor(hdc, 0xA8A8A8);
    SetBkColor(hdc, 0);
    nFGIntensity = 0;
    nFGColor = 7;
    nBGColor = 0;
    return;
  }
  if ( colorcode == 1 )
  {
    nFGIntensity = 1;
  }
  if ( colorcode == 2 )
  {
    nFGIntensity = 0;
  }
  if ( colorcode >= 30 && colorcode <= 37 )
  {
    nFGColor = colorcode % 30;
  }
  if ( colorcode >= 40 && colorcode <= 47 )
  {
    nBGColor = colorcode % 40;
  }
  SetTextColor(hdc, color[(nFGIntensity << 3) | (nFGColor & 7)]);
  SetBkColor(hdc, color[nBGColor & 7]);
}

void sRenderImage(HDC hdc, unsigned char *pBuffer, int nBufferCount, int *pnXPos, int *pnYPos)
{
  int pushedX = 0;
  int pushedY = 0;

  *pnXPos = 0;
  *pnYPos = 0;
  for ( int i = 0; i < nBufferCount; i++ )
  {
    if ( pBuffer[i] == 0x1B )
    {
      char pTempBuf[20];
      ZeroMemory(pTempBuf, 20);
      i += 2;
      int j = i;
      for ( j = i; !isalpha(pBuffer[j]); j++ ) { }
      CopyMemory(pTempBuf, &pBuffer[i], j - i);
      switch ( pBuffer[j] ) // https://en.wikipedia.org/wiki/ANSI_escape_code#CSI_codes
      {
        case 'A': // move up X lines
          {
            int lines = 1;
            if ( strlen(pTempBuf) )
              sscanf(pTempBuf, "%d", &lines);
            *pnYPos -= lines;
          } break;
        case 'B': // move down X lines
          {
            int lines = 1;
            if ( strlen(pTempBuf) )
              sscanf(pTempBuf, "%d", &lines);
            *pnYPos += lines;
          } break;
        case 'C': // move right X lines
          {
            int lines = 1;
            if ( strlen(pTempBuf) )
              sscanf(pTempBuf, "%d", &lines);
            *pnXPos += lines;
          } break;
        case 'D': // move left X lines
          {
            int lines = 1;
            if ( strlen(pTempBuf) )
              sscanf(pTempBuf, "%d", &lines);
            *pnXPos -= lines;
          } break;
        case 'H': // go to XY
          {
            if ( strlen(pTempBuf) )
            {
              if ( strstr(pTempBuf, ";") )
              {
                int y = 0, x = 0;
                sscanf(pTempBuf, "%d;%d", &y, &x);
                *pnXPos = x - 1;
                *pnYPos = y - 1;
              }
              else
              {
                int y = 0;
                sscanf(pTempBuf, "%d", &y);
                *pnXPos = 0;
                *pnYPos = y - 1;
              }
            }
            else
            {
              *pnYPos = 0;
              *pnXPos = 0;
            }
          } break;
        case 'J': // clear
          {
            RECT rc;
            if ( strlen(pTempBuf) )
            {
              if ( pTempBuf[0] == '1' ) // clear screen only
              {
                rc.left = 0;
                rc.top = 0;
                rc.right = 80 * sz.cx;
                rc.bottom = sz.cy * *pnYPos;
              }
              else if ( pTempBuf[0] == '2' ) // clear whole buffer
              {
                rc.top = 0;
                rc.left = 0;
                rc.right = 80 * sz.cx;
                rc.bottom = sz.cy * nMaxPos;
                *pnYPos = 0;
                *pnXPos = 0;
              }
            }
            else // clear screen
            {
              rc.left = 0;
              rc.top = sz.cy * *pnYPos;
              rc.right = 80 * sz.cx;
              rc.bottom = sz.cy * nMaxPos;
            }
            if ( hdc )
            {
              FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            }
          } break;
        case 'm': // set color
          if ( hdc )
          {
            for ( char * k = pTempBuf; ; k++ )
            {
              int color = 0;
              sscanf(k, "%d", &color);
              SetANSIColor(hdc, color);
              k = strchr(k, ';');
              if ( !k )
                break;
            }
          } break;
        case 's': // push position
          {
            pushedX = *pnXPos;
            pushedY = *pnYPos;
          } break;
        case 'u': // pop position
          {
            *pnXPos = pushedX;
            *pnYPos = pushedY;
          } break;
        default:
          {
            assert(false);
          } break;
      }
      i = j;
    }
    else
    {
      if ( pBuffer[i] < 32 )
      {
        if ( pBuffer[i] == 10 )
        {
          *pnXPos = 0;
        }
        else
        {
          if ( pBuffer[i] == 13 )
            (*pnYPos)++;
          else
            (*pnXPos)++;
        }
      }
      else
      {
        if ( hdc )
        {
          char s[2] = { pBuffer[i], 0 };
          TextOutA(hdc, sz.cx * *pnXPos, sz.cy * *pnYPos, s, 1);
        }
        (*pnXPos)++;
      }
    }
    while ( *pnXPos >= 80 )
    {
      (*pnXPos) -= 80;
      (*pnYPos)++;
    }
  }
}

void __stdcall ListGetDetectString(char* DetectString, int maxlen)
{
  strncpy(DetectString,"EXT=\"ANS\"",maxlen);
}

void __stdcall ListSetDefaultParams(ListDefaultParamStruct* dps)
{
  strncpy( szINIFilename, dps->DefaultIniName, MAX_PATH - 1 );
}

void __stdcall ListCloseWindow(HWND hWnd)
{
  DeleteDC(hdcSrc);
  DeleteObject(bmp);
  DestroyWindow(hWnd);
  UnregisterClassA("ansiviewwnd", hInstance);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch(uMsg)
  {
    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        BeginPaint( hWnd, &ps );
        FillRect( ps.hdc, &ps.rcPaint, NULL );
        if (zoom <= 1)
        {
          BitBlt( ps.hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, hdcSrc, 0, nPos * sz.cy, SRCCOPY );
        }
        else
        {
          StretchBlt( ps.hdc, 0, 0, zoom * sz.cx * 80, zoom * sz.cx * (nMaxPos - nPos), hdcSrc, 0, nPos * sz.cy, sz.cx * 80, sz.cx * (nMaxPos - nPos), SRCCOPY );
        }
        EndPaint( hWnd, &ps );
      } break;
    case WM_KEYFIRST:
      {
        switch(wParam)
        {
          case VK_DOWN: { PostMessage( hWnd, WM_VSCROLL, SB_LINEDOWN, NULL); break; };
          case VK_UP: { PostMessage( hWnd, WM_VSCROLL, SB_LINEUP, NULL); break; };
          case VK_NEXT: { PostMessage( hWnd, WM_VSCROLL, SB_PAGEDOWN, NULL); break; };
          case VK_PRIOR: { PostMessage( hWnd, WM_VSCROLL, SB_PAGEUP, NULL); break; };
          case VK_ADD: { zoom++; InvalidateRect( hWnd, NULL, FALSE ); break; };
          case VK_SUBTRACT: { zoom = max(zoom-1,0); InvalidateRect( hWnd, NULL, FALSE ); break; };
        }
      } break;
    case WM_VSCROLL:
      {
        switch(LOWORD(wParam))
        {
          case SB_LINEDOWN: { nPos = min( nPos + 1, nMaxPos ); break; };
          case SB_LINEUP: { nPos = max( nPos - 1, 0 ); break; };
          case SB_PAGEDOWN: { nPos = min( nPos + 10, nMaxPos ); break; };
          case SB_PAGEUP: { nPos = max( nPos - 10, 0 ); break; };
          case SB_THUMBTRACK: { nPos = HIWORD(wParam); break; };
        }
        SetScrollPos( hWnd, SB_VERT, nPos, TRUE );
        InvalidateRect( hWnd, NULL, FALSE );
      } break;
    case WM_MOUSEWHEEL:
      {
        nPos -= GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 10;
        nPos = min( nPos, nMaxPos );
        nPos = max( nPos, 0 );
        InvalidateRect( hWnd, NULL, FALSE );
      } break;
  }
  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void ReadFromIni()
{
  char sz[200];
  char szPath[MAX_PATH];
  ExpandEnvironmentStringsA( szINIFilename, szPath, MAX_PATH );

  PathRemoveFileSpecA( szPath );
  PathAppendA( szPath, "wincmd.ini" );

  GetPrivateProfileStringA( "Lister", "Font2", "Fixedsys,0", sz, 200, szPath );
  sscanf( sz, "%[^,],%d", szFontName, &nFontSize );
}

HWND __stdcall ListLoad(HWND hWndParent, char * lpFileName, int ShowFlags)
{
  HANDLE hObject = CreateFileA(lpFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
  if ( hObject == INVALID_HANDLE_VALUE )
  {
    return NULL;
  }

  int nSize = GetFileSize(hObject, 0);
  if ( nSize == -1 )
  {
    CloseHandle(hObject);
    return NULL;
  }

  unsigned char * lpBuffer = new unsigned char[nSize];
  DWORD NumberOfBytesRead = 0;
  ReadFile(hObject, lpBuffer, nSize, &NumberOfBytesRead, 0);
  CloseHandle(hObject);

  hObject = NULL;

  WNDCLASSA WndClass;
  WndClass.style = 32;
  WndClass.lpfnWndProc = WndProc;
  WndClass.cbClsExtra = 0;
  WndClass.cbWndExtra = 0;
  WndClass.hInstance = hInstance;
  WndClass.hIcon = 0;
  WndClass.hCursor = NULL;
  WndClass.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
  WndClass.lpszMenuName = 0;
  WndClass.lpszClassName = "ansiviewwnd";
  RegisterClassA(&WndClass);

  RECT Rect;
  GetClientRect(hWndParent, &Rect);
  HWND hWnd = CreateWindowExA(
    NULL,
    "ansiviewwnd",
    "ANSI Viewer",
    WS_CHILD | WS_VSCROLL,
    Rect.left,
    Rect.top,
    Rect.right - Rect.left,
    Rect.bottom - Rect.top,
    hWndParent,
    NULL,
    hInstance,
    NULL);

  hdc = GetDC(hWnd);
  hdcSrc = CreateCompatibleDC(hdc);
  
  ReadFromIni();

  HFONT h = CreateFontA(nFontSize, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, szFontName);
  SelectObject(hdc, h);
  
  char c[2] = { 'A', 0 };
  GetTextExtentPointA(hdc, c, 1, &sz);
  int sizeX = 0, sizeY = 0;
  sRenderImage(0, lpBuffer, nSize, &sizeX, &sizeY);
  nPos = 0;
  nMaxPos = sizeY;
  SetScrollRange(hWnd, 1, 0, sizeY, 1);
  bmp = CreateCompatibleBitmap(hdc, 80 * sz.cx, sz.cy * sizeY);
  SelectObject(hdcSrc, (HGDIOBJ)bmp);
  SelectObject(hdcSrc, h);
  for ( int i = 0; i < 16; i++ )
  {
    COLORREF v40 = 0;
    if ( i & 1 )  v40 |=     0xA8;
    if ( i & 2 )  v40 |=   0xA800;
    if ( i & 4 )  v40 |= 0xA80000;
    if ( i & 8 )  v40 += 0x545454;
    if ( i == 3 ) v40  = 0x0054A8;
    color[i] = v40;
  }
  
  SetTextColor(hdcSrc, 0xFFFFFF);
  SetBkColor(hdcSrc, 0);

  sRenderImage(hdcSrc, (unsigned __int8 *)lpBuffer, nSize, &sizeX, &sizeY);
  DeleteObject(h);

  delete[] lpBuffer;

  InvalidateRect(hWnd, NULL, FALSE);
  ShowWindow(hWnd, SW_SHOW);

  return hWnd;
}

#ifdef __cplusplus
}
#endif
