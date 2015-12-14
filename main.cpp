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
int nCharWidth = 80;

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
    nFGIntensity = 0;
    nFGColor = 7;
    nBGColor = 0;
  }
  else if ( colorcode == 1 )
  {
    nFGIntensity = 1;
  }
  else if ( colorcode == 2 )
  {
    nFGIntensity = 0;
  }
  else if ( colorcode >= 30 && colorcode <= 37 )
  {
    nFGColor = colorcode % 30;
  }
  else if ( colorcode >= 40 && colorcode <= 47 )
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
            *pnXPos = min( nCharWidth - 1, *pnXPos );
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
                rc.right = nCharWidth * sz.cx;
                rc.bottom = sz.cy * *pnYPos;
              }
              else if ( pTempBuf[0] == '2' ) // clear whole buffer
              {
                rc.top = 0;
                rc.left = 0;
                rc.right = nCharWidth * sz.cx;
                rc.bottom = sz.cy * nMaxPos;
                *pnYPos = 0;
                *pnXPos = 0;
              }
            }
            else // clear screen
            {
              rc.left = 0;
              rc.top = sz.cy * *pnYPos;
              rc.right = nCharWidth * sz.cx;
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
        case 't': // 24 bit ansi (http://picoe.ca/2014/03/07/24-bit-ansi/)
          {
            int fgbg = 0, r = 0, g = 0, b = 0;
            sscanf(pTempBuf, "%d;%d;%d;%d", &fgbg, &r, &g, &b);
            if (fgbg == 0)
              SetBkColor(hdc, RGB(r,g,b));
            else
              SetTextColor(hdc, RGB(r,g,b));

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
    while ( *pnXPos >= nCharWidth )
    {
      (*pnXPos) -= nCharWidth;
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
          StretchBlt( ps.hdc, 0, 0, zoom * sz.cx * nCharWidth, zoom * sz.cx * (nMaxPos - nPos), hdcSrc, 0, nPos * sz.cy, sz.cx * nCharWidth, sz.cx * (nMaxPos - nPos), SRCCOPY );
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
          case VK_F12: 
            {
              char szFilename[MAX_PATH];
              ZeroMemory( szFilename, MAX_PATH );

              OPENFILENAMEA opf;
              ZeroMemory( &opf, sizeof(OPENFILENAMEA) );
              opf.lStructSize = sizeof(OPENFILENAMEA);
              opf.hwndOwner = hWnd;
              opf.lpstrFilter = "Targa files (*.tga)\0*.tga\0";
              opf.nFilterIndex = 1L;
              opf.lpstrFile = szFilename;
              opf.nMaxFile = MAX_PATH;
              opf.nMaxFileTitle = 50;
              opf.lpstrTitle = "Save Image as";
              opf.lpstrDefExt = ".tga";
              opf.Flags = (OFN_OVERWRITEPROMPT | OFN_NONETWORKBUTTON) & ~OFN_ALLOWMULTISELECT;

              if(GetSaveFileNameA(&opf))
              {
                HDC saveDC = CreateCompatibleDC(hdcSrc);
                HBITMAP saveBmp = CreateCompatibleBitmap(hdcSrc, sz.cx * nCharWidth, sz.cy * nMaxPos);
                SelectObject(saveDC, saveBmp);
                BitBlt( saveDC, 0, 0, sz.cx * nCharWidth, sz.cy * nMaxPos, hdcSrc, 0, 0, SRCCOPY );

                BITMAPINFO bi;
                ZeroMemory( &bi, sizeof(BITMAPINFO) );
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biWidth = sz.cx * nCharWidth;
                bi.bmiHeader.biHeight = sz.cy * nMaxPos;
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = 0;

                unsigned char * p = new unsigned char[ sz.cx * nCharWidth * sz.cy * nMaxPos * 4 ];
                GetDIBits( saveDC, saveBmp, 0, sz.cy * nMaxPos, p, &bi, NULL );

                for (int i=0; i < sz.cx * nCharWidth * sz.cy * nMaxPos; i++)
                  p[ i * 4 + 3 ] = 0xFF;

                #pragma pack(1)
                struct _TGAHeader {
                  unsigned char id_len;
                  unsigned char map_t;
                  unsigned char img_t;
                  unsigned short map_first;
                  unsigned short map_len;
                  unsigned char map_entry;
                  unsigned short x;
                  unsigned short y;
                  unsigned short width;
                  unsigned short height;
                  unsigned char depth;
                  unsigned char alpha;
                } tga;
                #pragma pack(0)

                ZeroMemory( &tga, sizeof(_TGAHeader) );

                tga.img_t = 3; // rgba
                tga.width  = sz.cx * nCharWidth;
                tga.height = sz.cy * nMaxPos;
                tga.depth = 32;
                tga.alpha = 8;

                HANDLE hObject = CreateFileA(szFilename, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, NULL, NULL);
                DWORD bytes = 0;
                WriteFile( hObject, &tga, sizeof(_TGAHeader), &bytes, NULL );
                WriteFile( hObject, p, sz.cx * nCharWidth * sz.cy * nMaxPos * 4, &bytes, NULL );
                CloseHandle( hObject );
                delete[] p;

                DeleteObject( saveBmp );
                DeleteDC( saveDC );
              }
            } break;
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

  if (nSize > 128)
  {
#pragma pack(1)
    typedef enum 
    {
      SAUCE_DATATYPE_NONE = 0,
      SAUCE_DATATYPE_CHARACTER,
      SAUCE_DATATYPE_BITMAP,
      SAUCE_DATATYPE_VECTOR,
      SAUCE_DATATYPE_AUDIO,
      SAUCE_DATATYPE_BINARYTEXT,
      SAUCE_DATATYPE_XBIN,
      SAUCE_DATATYPE_ARCHIVE,
      SAUCE_DATATYPE_EXECUTABLE
    } SAUCE_DATATYPE;

#define SAUCE_FILETYPE_CHARACTER_ASCII      0
#define SAUCE_FILETYPE_CHARACTER_ANSI       1
#define SAUCE_FILETYPE_CHARACTER_ANSIMATION 2
#define SAUCE_FILETYPE_CHARACTER_RIPSCRIPT  3
#define SAUCE_FILETYPE_CHARACTER_PCBOARD    4
#define SAUCE_FILETYPE_CHARACTER_AVATAR     5
#define SAUCE_FILETYPE_CHARACTER_HTML       6
#define SAUCE_FILETYPE_CHARACTER_SOURCE     7
#define SAUCE_FILETYPE_CHARACTER_TUNDRADRAW 8

    typedef struct 
    {
      char szSignature[5];
      char szVersion[2];
      char szTitle[35];
      char szAuthor[20];
      char szGroup[20];
      char szDate[8];
      unsigned int nFilesize;
      unsigned char nDataType;
      unsigned char nFileType;
      unsigned short nInfo1;
      unsigned short nInfo2;
      unsigned short nInfo3;
      unsigned short nInfo4;
      unsigned char nComments;
      unsigned char nFlags;
      char pPadding[22];
    } SAUCE_HEADER;

    SAUCE_HEADER * sauceHeader = (SAUCE_HEADER *)(lpBuffer + nSize - 128);
    if (strncmp(sauceHeader->szSignature,"SAUCE",5) == 0)
    {
      if (sauceHeader->nDataType == SAUCE_DATATYPE_CHARACTER)
      {
        if (sauceHeader->nFileType == SAUCE_FILETYPE_CHARACTER_ASCII
          || sauceHeader->nFileType == SAUCE_FILETYPE_CHARACTER_ANSI
          || sauceHeader->nFileType == SAUCE_FILETYPE_CHARACTER_ANSIMATION
          || sauceHeader->nFileType == SAUCE_FILETYPE_CHARACTER_PCBOARD
          || sauceHeader->nFileType == SAUCE_FILETYPE_CHARACTER_AVATAR)
        {
          if (sauceHeader->nInfo1 > 0)
            nCharWidth = sauceHeader->nInfo1;
        }
      }
    }
#pragma pack()
  }


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
  bmp = CreateCompatibleBitmap(hdc, nCharWidth * sz.cx, sz.cy * sizeY);
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
