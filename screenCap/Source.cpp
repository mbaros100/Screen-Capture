#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <string>
#include <dwmapi.h>

#include "DwmThumbnail.h"
using namespace std;

// Helper function to retrieve current position of file pointer:
inline int GetFilePointer(HANDLE FileHandle) {
	return SetFilePointer(FileHandle, 0, 0, FILE_CURRENT);
}

extern bool SaveBMPFile(const char *filename, HDC bitmapDC, int width, int height) {
	bool Success = false;
	HDC SurfDC = NULL;        // GDI-compatible device context for the surface
	HBITMAP OffscrBmp = NULL; // bitmap that is converted to a DIB
	HDC OffscrDC = NULL;      // offscreen DC that we can select OffscrBmp into
	LPBITMAPINFO lpbi = NULL; // bitmap format info; used by GetDIBits
	LPVOID lpvBits = NULL;    // pointer to bitmap bits array
	HANDLE BmpFile = INVALID_HANDLE_VALUE;    // destination .bmp file
	BITMAPFILEHEADER bmfh;  // .bmp file header

							// We need an HBITMAP to convert it to a DIB:
	if ((OffscrBmp = CreateCompatibleBitmap(bitmapDC, width, height)) == NULL)
		return false;

	// The bitmap is empty, so let's copy the contents of the surface to it.
	// For that we need to select it into a device context. We create one.
	if ((OffscrDC = CreateCompatibleDC(bitmapDC)) == NULL)
		return false;

	// Select OffscrBmp into OffscrDC:
	HBITMAP OldBmp = (HBITMAP)SelectObject(OffscrDC, OffscrBmp);

	// Now we can copy the contents of the surface to the offscreen bitmap:
	BitBlt(OffscrDC, 0, 0, width, height, bitmapDC, 0, 0, SRCCOPY);

	// GetDIBits requires format info about the bitmap. We can have GetDIBits
	// fill a structure with that info if we pass a NULL pointer for lpvBits:
	// Reserve memory for bitmap info (BITMAPINFOHEADER + largest possible
	// palette):
	if ((lpbi = (LPBITMAPINFO)(new char[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)])) == NULL)
		return false;


	ZeroMemory(&lpbi->bmiHeader, sizeof(BITMAPINFOHEADER));
	lpbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	// Get info but first de-select OffscrBmp because GetDIBits requires it:
	SelectObject(OffscrDC, OldBmp);
	if (!GetDIBits(OffscrDC, OffscrBmp, 0, height, NULL, lpbi, DIB_RGB_COLORS))
		return false;

	// Reserve memory for bitmap bits:
	if ((lpvBits = new char[lpbi->bmiHeader.biSizeImage]) == NULL)
		return false;

	// Have GetDIBits convert OffscrBmp to a DIB (device-independent bitmap):
	if (!GetDIBits(OffscrDC, OffscrBmp, 0, height, lpvBits, lpbi, DIB_RGB_COLORS))
		return false;

	// Create a file to save the DIB to:
	if ((BmpFile = CreateFile(filename,
		GENERIC_WRITE,
		0, NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL)) == INVALID_HANDLE_VALUE)

		return false;

	DWORD Written;    // number of bytes written by WriteFile

					  // Write a file header to the file:
	bmfh.bfType = 19778;        // 'BM'
								// bmfh.bfSize = ???        // we'll write that later
	bmfh.bfReserved1 = bmfh.bfReserved2 = 0;
	// bmfh.bfOffBits = ???     // we'll write that later
	if (!WriteFile(BmpFile, &bmfh, sizeof(bmfh), &Written, NULL))
		return false;

	if (Written < sizeof(bmfh))
		return false;

	// Write BITMAPINFOHEADER to the file:
	if (!WriteFile(BmpFile, &lpbi->bmiHeader, sizeof(BITMAPINFOHEADER), &Written, NULL))
		return false;

	if (Written < sizeof(BITMAPINFOHEADER))
		return false;

	// Calculate size of palette:
	int PalEntries;
	// 16-bit or 32-bit bitmaps require bit masks:
	if (lpbi->bmiHeader.biCompression == BI_BITFIELDS)
		PalEntries = 3;
	else
		// bitmap is palettized?
		PalEntries = (lpbi->bmiHeader.biBitCount <= 8) ?
		// 2^biBitCount palette entries max.:
		(int)(1 << lpbi->bmiHeader.biBitCount)
		// bitmap is TrueColor -> no palette:
		: 0;
	// If biClrUsed use only biClrUsed palette entries:
	if (lpbi->bmiHeader.biClrUsed)
		PalEntries = lpbi->bmiHeader.biClrUsed;

	// Write palette to the file:
	if (PalEntries) {
		if (!WriteFile(BmpFile, &lpbi->bmiColors, PalEntries * sizeof(RGBQUAD), &Written, NULL))
			return false;

		if (Written < PalEntries * sizeof(RGBQUAD))
			return false;
	}

	// The current position in the file (at the beginning of the bitmap bits)
	// will be saved to the BITMAPFILEHEADER:
	bmfh.bfOffBits = GetFilePointer(BmpFile);

	// Write bitmap bits to the file:
	if (!WriteFile(BmpFile, lpvBits, lpbi->bmiHeader.biSizeImage, &Written, NULL))
		return false;

	// get the data here.

	if (Written < lpbi->bmiHeader.biSizeImage)
		return false;

	// The current pos. in the file is the final file size and will be saved:
	bmfh.bfSize = GetFilePointer(BmpFile);

	// We have all the info for the file header. Save the updated version:
	SetFilePointer(BmpFile, 0, 0, FILE_BEGIN);
	if (!WriteFile(BmpFile, &bmfh, sizeof(bmfh), &Written, NULL))
		return false;

	if (Written < sizeof(bmfh))
		return false;

	return true;
}

BOOL aisWindowVisible(HWND hwnd) {
	RECT r1;
	GetWindowRect(hwnd, &r1);
	HRGN x = CreateRectRgnIndirect(&r1);

	HWND s = GetTopWindow(0);

	do {
		if (IsWindowVisible(s)) {
			RECT r2;
			GetWindowRect(s, &r2);
			HRGN y = CreateRectRgnIndirect(&r2);

			int res = CombineRgn(x, x, y, RGN_DIFF);
			DeleteObject(y);
			if (res == NULLREGION) {
				DeleteObject(x);
				return FALSE;
			}
		}
	} while ((s = GetNextWindow(s, GW_HWNDNEXT)) && s != hwnd);

	DeleteObject(x);
	return TRUE;
}

void GetScreenShot()
{
	int x1, y1, x2, y2, w, h;

	// get screen dimensions
	x1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
	y1 = GetSystemMetrics(SM_YVIRTUALSCREEN);
	x2 = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	y2 = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	w = x2 - x1;
	h = y2 - y1;

	int x[2];
	int y[2];
	x[0] = x1;  x[1] = x2;
	y[0] = y1; y[0] = y2;

	HWND hWnd = ::FindWindow(0, _T("Calculator"));
	if (hWnd == nullptr) {
		cout << "null window\n";
	}

	///
	HDC		hScreen = nullptr;
	HDC     hDC = nullptr;
	HBITMAP hBitmap = nullptr;
	HGDIOBJ old_obj = nullptr;
	///

	int index = 0;
	while (1)
	{
		if (hWnd == nullptr) {
			cout << "null window\n";
			break;
		}
		if (aisWindowVisible(hWnd))
		{
			index++;
			RECT r;
			GetWindowRect(hWnd, &r);
			x[0] = r.top;
			x[1] = r.bottom;
			y[0] = r.left;
			y[1] = r.right;

			cout << "visible\n";
			HDC		hScreen = GetDC(NULL);
			hDC = CreateCompatibleDC(hScreen);
			hBitmap = CreateCompatibleBitmap(hScreen, y[1] - y[0], x[1] - x[0]);
			old_obj = SelectObject(hDC, hBitmap);
			BitBlt(hDC, 0, 0, y[1] - y[0], x[1] - x[0], hScreen, y[0], x[0], SRCCOPY);

			// save my bitmap
			bool ret = SaveBMPFile(("screenshot" + to_string(index) + ".bmp").c_str(), hDC, y[1] - y[0], x[1] - x[0]);
		}
	}

	// clean up
	SelectObject(hDC, old_obj);
	DeleteDC(hDC);
	ReleaseDC(NULL, hScreen);
	DeleteObject(hBitmap);
}

int main()
{
	GetScreenShot();
}