#include <Windows.h>
#include <cmath>
#include <math.h>
#include <vector>
#include <random>

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 8000
#define SECONDS 20
#define BUFFER_SIZE (SAMPLE_RATE * SECONDS)

#define FASTER_SAMPLE_RATE 11025
#define NEW_BUFFER_SIZE (FASTER_SAMPLE_RATE * SECONDS)

struct Point3D
{
	double x;
	double y;
	double z;
};

struct Point2D
{
	int x, y;
};

inline VOID Reflect2D(int* x, int* y, int w, int h)
{
	*x = abs(*x);
	*y = abs(*y);

	int rx = *x / w;
	int ry = *y / h;

	*x = (rx % 2 == 0) ? (*x % w) : (w - 1 - (*x % w));
	*y = (ry % 2 == 0) ? (*y % h) : (h - 1 - (*y % h));
}

typedef enum
{
	REDRAW = RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN,
	MEM_ALLOC_TYPES = MEM_COMMIT | MEM_RESERVE,
} MalwareDefs;

void generateSierpinski(Point3D p1, Point3D p2, Point3D p3, int depth, std::vector<Point3D>& points)
{
	if (depth == 0)
	{
		points.push_back(p1);
		points.push_back(p2);
		points.push_back(p3);
		return;
	}

	Point3D p12 = { (p1.x + p2.x) / 2.0f, (p1.y + p2.y) / 2.0f, (p1.z + p2.z) / 2.0f };
	Point3D p23 = { (p2.x + p3.x) / 2.0f, (p2.y + p3.y) / 2.0f, (p2.z + p3.z) / 2.0f };
	Point3D p31 = { (p3.x + p1.x) / 2.0f, (p3.y + p1.y) / 2.0f, (p3.z + p1.z) / 2.0f };

	generateSierpinski(p1, p12, p31, depth - 1, points);
	generateSierpinski(p12, p2, p23, depth - 1, points);
	generateSierpinski(p31, p23, p3, depth - 1, points);
}

typedef union
{
	COLORREF rgb;

	struct
	{
		BYTE r;
		BYTE g;
		BYTE b;

		BYTE reserved;
	};
} RGBQUAD_t;

typedef RGBQUAD_t* PRGBQUAD_t;

typedef struct
{
	FLOAT h;
	FLOAT s;
	FLOAT l;
} HSL;

static float HueToRGB(float p, float q, float t)
{
	if (t < 0.0f) t += 1.0f;
	if (t > 1.0f) t -= 1.0f;
	if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
	if (t < 1.0f / 2.0f) return q;
	if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
	return p;
}

RGBQUAD_t HSLtoRGB(HSL hsl)
{
	RGBQUAD_t rgb;
	rgb.reserved = 0;

	if (hsl.s == 0.0f)
	{
		BYTE val = (BYTE)(hsl.l * 255.0f);
		rgb.r = val;
		rgb.g = val;
		rgb.b = val;
	}

	else
	{
		float q =
			(hsl.l < 0.5f) ? (hsl.l * (1.0f + hsl.s)) :
			(hsl.l + hsl.s - hsl.l * hsl.s);

		float p = 2.0f * hsl.l - q;

		rgb.r = (BYTE)(HueToRGB(p, q, hsl.h + 1.0f / 3.0f) * 255.0f);
		rgb.g = (BYTE)(HueToRGB(p, q, hsl.h) * 255.0f);
		rgb.b = (BYTE)(HueToRGB(p, q, hsl.h - 1.0f / 3.0f) * 255.0f);
	}

	return rgb;
}

HSL RGBtoHSL(RGBQUAD_t rgb)
{
	float r = rgb.r / 255.0f;
	float g = rgb.g / 255.0f;
	float b = rgb.b / 255.0f;
	float max = fmaxf(r, fmaxf(g, b));
	float min = fminf(r, fminf(g, b));
	float h, s, l = (max + min) / 2.0f;

	if (max == min)
	{
		h = s = 0;
	}

	else
	{
		float d = max - min;
		s = l > 0.5f ? d / (2 - max - min) : d / (max + min);
		if (max == r) h = (g - b) / d + (g < b ? 6 : 0);
		else if (max == g) h = (b - r) / d + 2;
		else h = (r - g) / d + 4;
		h /= 6;
	}

	return { h, s, l };
}

int r = 0;
int g = 0;
int b = 0;
int state = 0;

COLORREF CubeHSLtoRGB(double h, double s, double l)
{
	double c = (1.0 - std::abs(2.0 * l - 1.0)) * s;
	double x = c * (1.0 - std::abs(fmod(h / 60.0, 2.0) - 1.0));
	double m = l - c / 2.0;

	double r = 0, g = 0, b = 0;
	if (0 <= h && h < 60) { r = c; g = x; b = 0; }
	else if (60 <= h && h < 120) { r = x; g = c; b = 0; }
	else if (120 <= h && h < 180) { r = 0; g = c; b = x; }
	else if (180 <= h && h < 240) { r = 0; g = x; b = c; }
	else if (240 <= h && h < 300) { r = x; g = 0; b = c; }
	else if (300 <= h && h < 360) { r = c; g = 0; b = x; }

	int red = static_cast<int>((r + m) * 255);
	int green = static_cast<int>((g + m) * 255);
	int blue = static_cast<int>((b + m) * 255);

	return RGB(red, green, blue);
}

int w = GetSystemMetrics(0);
int h = GetSystemMetrics(1);

namespace Bytebeats
{
	DWORD WINAPI Sound1(LPVOID lpvd)
	{
		HWAVEOUT hWaveOut;
		WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

		if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
			return 1;

		std::vector<unsigned char> buffer(BUFFER_SIZE);

		for (int t = 0; t < BUFFER_SIZE; ++t)
		{
			buffer[t] =
			(unsigned char)
			(
				((((((t * t)) + 2) / ((t >> 10) + 2))) / 40.75) +
				((((t * (t >> 5 & t >> 6)) >> (7 & t >> 13)) * 100) - t)
			);
		}

		WAVEHDR header = { (LPSTR)buffer.data(), BUFFER_SIZE, 0, 0, 0, 0, 0, 0 };

		waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

		while (!(header.dwFlags & WHDR_DONE))
		{
			Sleep(100);
		}

		waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		return 0;
	}

	DWORD WINAPI Sound2(LPVOID lpvd)
	{
		HWAVEOUT hWaveOut;
		WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

		if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
			return 1;

		std::vector<unsigned char> buffer(BUFFER_SIZE);

		for (int t = 0; t < BUFFER_SIZE; ++t)
		{
			buffer[t] =
			(unsigned char)
			(
				(((t & (t >> 8) - t & t >> 7) * t))
			);
		}

		WAVEHDR header = { (LPSTR)buffer.data(), BUFFER_SIZE, 0, 0, 0, 0, 0, 0 };

		waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

		while (!(header.dwFlags & WHDR_DONE))
		{
			Sleep(100);
		}

		waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		return 0;
	}

	DWORD WINAPI Sound3(LPVOID lpParam)
	{
		HWAVEOUT hWaveOut;
		WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, FASTER_SAMPLE_RATE, FASTER_SAMPLE_RATE, 1, 8, 0 };

		if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
		{
			return 1;
		}

		std::vector<unsigned char> buffer(NEW_BUFFER_SIZE);

		for (int t = 0; t < NEW_BUFFER_SIZE; ++t)
		{
			buffer[t] = (unsigned char)(((t * (t >> 10 | t << 1)) / 40.75) + (t % 255 | t % 257));
		}

		WAVEHDR header = { (LPSTR)buffer.data(), (DWORD)buffer.size(), 0, 0, 0, 0, 0, 0 };

		waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

		while (!(header.dwFlags & WHDR_DONE))
		{
			Sleep(100);
		}

		waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);

		return 0;
	}

	DWORD WINAPI Sound4(LPVOID lpParam)
	{
		HWAVEOUT hWaveOut;
		WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, FASTER_SAMPLE_RATE, FASTER_SAMPLE_RATE, 1, 8, 0 };

		if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
		{
			return 1;
		}

		std::vector<unsigned char> buffer(NEW_BUFFER_SIZE);

		for (int t = 0; t < NEW_BUFFER_SIZE; ++t)
		{
			buffer[t] = (unsigned char)(((2 * t & (t & 256 ? -t : t) * 1) >> 10) + (t >> 1) & 128 + (t >> t));
		}

		WAVEHDR header = { (LPSTR)buffer.data(), (DWORD)buffer.size(), 0, 0, 0, 0, 0, 0 };

		waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

		while (!(header.dwFlags & WHDR_DONE))
		{
			Sleep(100);
		}

		waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);

		return 0;
	}

	DWORD WINAPI Sound5(LPVOID lpvd)
	{
		HWAVEOUT hWaveOut = 0;
		WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 22050, 22050, 1, 8, 0 };

		if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
		{
			return 1;
		}

		DWORD bufferSize = 22050 * 20;
		char* buffer = new char[bufferSize];

		for (DWORD t = 0; t < bufferSize; ++t)
		{
			buffer[t] = static_cast<char>((t * (t >> 9 | t >> 13) & 16) + (t * (42 & t >> 10)));
		}

		WAVEHDR header = { 0 };
		header.lpData = buffer;
		header.dwBufferLength = bufferSize;

		waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

		while (!(header.dwFlags & WHDR_DONE))
		{
			Sleep(100);
		}

		waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);

		delete[] buffer;

		return 0;
	}

	DWORD WINAPI Sound6(LPVOID lpParam)
	{
		HWAVEOUT hWaveOut;
		WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

		if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
			return 1;

		std::vector<unsigned char> buffer(BUFFER_SIZE);

		for (int t = 0; t < BUFFER_SIZE; ++t)
		{
			buffer[t] =
			(unsigned char)
			(
				(17 * t | (t >> 2) + (8 * (2 >> (t & 32768) ? 13 : 14) * t | t >> 3 | t >> 5) * t >> 16) + 128
			);
		}

		WAVEHDR header = { (LPSTR)buffer.data(), BUFFER_SIZE, 0, 0, 0, 0, 0, 0 };

		waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

		while (!(header.dwFlags & WHDR_DONE))
		{
			Sleep(100);
		}

		waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		return 0;
	}

	DWORD WINAPI Sound7(LPVOID lpParam)
	{
		HWAVEOUT hWaveOut;
		WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

		if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
			return 1;

		std::vector<unsigned char> buffer(BUFFER_SIZE);

		for (int t = 0; t < BUFFER_SIZE; ++t)
		{
			buffer[t] =
			(unsigned char)
			(
				(
					(t >> (t >> 12) % 4) + t * (1 + (1 + (t >> 16) % 6) * (t >> 10) *
					(t >> 11) % 8) ^ t >> 13 ^ t >> 6
				) + (t & t >> 8)
			);
		}

		WAVEHDR header = { (LPSTR)buffer.data(), BUFFER_SIZE, 0, 0, 0, 0, 0, 0 };

		waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

		while (!(header.dwFlags & WHDR_DONE))
		{
			Sleep(100);
		}

		waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		return 0;
	}

	DWORD WINAPI Sound8(LPVOID lpParam)
	{
		HWAVEOUT hWaveOut;
		WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

		if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
			return 1;

		std::vector<unsigned char> buffer(BUFFER_SIZE);

		for (int t = 0; t < BUFFER_SIZE; ++t)
		{
			buffer[t] =
			(unsigned char)
			(
				(t * t >> (t / 256)) + (t + (t & 1) + (t >> 5) * (t >> 1) | t >> 4 | t >> 8)
			);
		}

		WAVEHDR header = { (LPSTR)buffer.data(), BUFFER_SIZE, 0, 0, 0, 0, 0, 0 };

		waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

		while (!(header.dwFlags & WHDR_DONE))
		{
			Sleep(100);
		}

		waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		return 0;
	}

	DWORD WINAPI Sound9(LPVOID lpParam)
	{
		HWAVEOUT hWaveOut;
		WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, FASTER_SAMPLE_RATE, FASTER_SAMPLE_RATE, 1, 8, 0 };

		if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
		{
			return 1;
		}

		std::vector<unsigned char> buffer(NEW_BUFFER_SIZE);

		for (int t = 0; t < NEW_BUFFER_SIZE; ++t)
		{
			buffer[t] = (unsigned char)
			(
				(((t / 512 | 0) ^ (t / 500 | 0) - 2) % 13 * t / 2 & 127) + (t >> 12 | t >> 8)
			);
		}

		WAVEHDR header = { (LPSTR)buffer.data(), (DWORD)buffer.size(), 0, 0, 0, 0, 0, 0 };

		waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

		while (!(header.dwFlags & WHDR_DONE))
		{
			Sleep(100);
		}

		waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);

		return 0;
	}

	DWORD WINAPI Sound10(LPVOID lpParam)
	{
		HWAVEOUT hWaveOut;
		WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

		if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
			return 1;

		std::vector<unsigned char> buffer(BUFFER_SIZE);

		for (int t = 0; t < BUFFER_SIZE; ++t)
		{
			buffer[t] =
			(unsigned char)
			(
				(((t / 500 | 0) - 2) % 16 * t / 2 & 127) + (((t / 91 & t ^ t / 90 & t) - 1))
			);
		}

		WAVEHDR header = { (LPSTR)buffer.data(), BUFFER_SIZE, 0, 0, 0, 0, 0, 0 };

		waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

		while (!(header.dwFlags & WHDR_DONE))
		{
			Sleep(100);
		}

		waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		return 0;
	}

	DWORD WINAPI Sound11(LPVOID lpParam)
	{
		HWAVEOUT hWaveOut;
		WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

		if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
			return 1;

		std::vector<unsigned char> buffer(BUFFER_SIZE);

		for (int t = 0; t < BUFFER_SIZE; ++t)
		{
			buffer[t] =
			(unsigned char)
			(
				t * ((t >> 16 | t >> 10) & 23 & t >> 1)
			);
		}

		WAVEHDR header = { (LPSTR)buffer.data(), BUFFER_SIZE, 0, 0, 0, 0, 0, 0 };

		waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

		while (!(header.dwFlags & WHDR_DONE))
		{
			Sleep(100);
		}

		waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		return 0;
	}
}

namespace GDIPayloads
{
	DWORD WINAPI GDIShader1(LPVOID lpParam)
	{
		HDC hdc = GetDC(0);
		HDC dcCopy = CreateCompatibleDC(hdc);

		BITMAPINFO bmpi = { 0 };
		bmpi.bmiHeader.biSize = sizeof(bmpi);
		bmpi.bmiHeader.biWidth = w;
		bmpi.bmiHeader.biHeight = -h;
		bmpi.bmiHeader.biPlanes = 1;
		bmpi.bmiHeader.biBitCount = 32;

		PRGBQUAD_t prgbSrc =
		(PRGBQUAD_t)VirtualAlloc(NULL, w * h * sizeof(RGBQUAD_t), MalwareDefs::MEM_ALLOC_TYPES, PAGE_READWRITE);

		PRGBQUAD_t prgbDst = nullptr;

		HBITMAP hbmDst = CreateDIBSection(hdc, &bmpi, DIB_RGB_COLORS, (void**)&prgbDst, NULL, 0);
		SelectObject(dcCopy, hbmDst);

		FLOAT f = 0.f;

		while (true)
		{
			BitBlt(dcCopy, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
			memcpy(prgbSrc, prgbDst, w * h * sizeof(RGBQUAD_t));

			RGBQUAD_t rgbDst;
			RGBQUAD_t rgbSrc;

			int t = 0;

			for (INT y = 0; y < h; y++)
			{
				FLOAT a = sin(t + y) * 2.f;

				for (INT x = 0; x < w; x++)
				{
					UINT u = x + (INT)a;
					UINT v = y;

					u %= w;
					v %= h;


					Reflect2D((PINT)&u, (PINT)&v, w, h);

					COLORREF rgbSrcRgb = 0;
					COLORREF rgbDstRgb = 0;

					rgbDst = prgbSrc[v * w + u];
					rgbSrc = prgbSrc[y * w + x];

					if (!rgbSrcRgb)
					{
						rgbSrcRgb = 1;
					}

					rgbDstRgb &= rgbDstRgb % ((rgbSrcRgb << 8) + 1);
					FLOAT _r = (FLOAT)rgbDst.r * f + (FLOAT)rgbSrc.r * (1.f - f);
					FLOAT _g = (FLOAT)rgbDst.g * f + (FLOAT)rgbSrc.g * (1.f - f);
					FLOAT _b = (FLOAT)rgbDst.b * f + (FLOAT)rgbSrc.b * (1.f - f);
					rgbDstRgb = ((BYTE)_b | ((BYTE)_g << 8) | ((BYTE)_r << 16));

					HSL hsl = RGBtoHSL(rgbDst);
					hsl.h = (FLOAT)fmod((DOUBLE)hsl.h + (DOUBLE)(x + y) / 100000.0 + 0.05, 1.0);
					hsl.s = 1.f;

					if (hsl.l < .2f)
					{
						hsl.l += .2f;
					}

					rgbDst = HSLtoRGB(hsl);
					prgbDst[y * w + x] = rgbDst;
				}
			}

			BitBlt(hdc, 0, 0, w, h, dcCopy, 0, 0, SRCCOPY);
			Sleep(10);
		}

		VirtualFree(prgbSrc, 0, MEM_RELEASE);
		DeleteObject(hbmDst);
		DeleteDC(dcCopy);
		ReleaseDC(NULL, hdc);

		return 0;
	}

	DWORD WINAPI GDIShader2(LPVOID lpParam)
	{
		HDC hdc = GetDC(NULL);
		HDC hdcCopy = CreateCompatibleDC(hdc);
		BITMAPINFO bmpi = { 0 };
		HBITMAP bmp;

		bmpi.bmiHeader.biSize = sizeof(bmpi);
		bmpi.bmiHeader.biWidth = w;
		bmpi.bmiHeader.biHeight = h;
		bmpi.bmiHeader.biPlanes = 1;
		bmpi.bmiHeader.biBitCount = 32;
		bmpi.bmiHeader.biCompression = BI_RGB;


		PRGBQUAD_t rgbquad = NULL;
		HSL hslcolor;

		bmp = CreateDIBSection(hdc, &bmpi, DIB_RGB_COLORS, (void**)&rgbquad, NULL, 0);
		SelectObject(hdcCopy, bmp);

		INT i = 0;

		while (1)
		{
			hdc = GetDC(NULL);
			StretchBlt(hdcCopy, 0, 0, w, h, hdc, 0, 0, w, h, SRCCOPY);

			RGBQUAD_t rgbquadCopy;

			for (int x = 0; x < w; x++)
			{
				for (int y = 0; y < h; y++)
				{
					int index = y * w + x;

					int cx = x - (w / 2);
					int cy = y - (h / 2);

					if (rand() % 25 == 24)
					{
						cy = y + (h / 2);
					}

					int fx = i * (cbrt((cx * cx) + (cy * cy)));

					rgbquadCopy = rgbquad[index];

					hslcolor = RGBtoHSL(rgbquadCopy);
					hslcolor.h = fmod(fx / 400.f + y / h * .2f, 1.f);
					rgbquad[index] = HSLtoRGB(hslcolor);
				}
			}

			i++;

			StretchBlt(hdc, 0, 0, w, h, hdcCopy, 0, 0, w, h, SRCCOPY);
			ReleaseDC(NULL, hdc);
			DeleteDC(hdc);
		}
		
		return 0;
	}

	DWORD WINAPI Pats(LPVOID lpParam)
	{
		while (1)
		{
			HDC hdc = GetDC(0);
			HDC dcCopy = CreateCompatibleDC(hdc);

			HBRUSH brush = CreateHatchBrush(rand() % 4, rand() % 255);

			while (true)
			{
				SelectObject(hdc, brush);
				SetBkColor(hdc, RGB(rand() % 255, rand() % 255, rand() % 255));
				PatBlt(hdc, 0, 0, w, h, PATINVERT);
				Sleep(1);
			}

			DeleteObject(brush);
			DeleteDC(hdc);
			return 0;
		}
	}

	DWORD WINAPI Cube(LPVOID lpParam)
	{
		HDC hdc = GetDC(NULL);

		Point3D cubeVertices[8] =
		{
			{-100, -100, -100}, {100, -100, -100},
			{100,  100, -100}, {-100,  100, -100},
			{-100, -100,  100}, {100, -100,  100},
			{100,  100,  100}, {-100,  100,  100}
		};

		int edges[12][2] =
		{
			{0,1}, {1,2}, {2,3}, {3,0},
			{4,5}, {5,6}, {6,7}, {7,4},
			{0,4}, {1,5}, {2,6}, {3,7}
		};

		double angleX = 0.0;
		double angleY = 0.0;
		double hue = 0.0;

		while (true)
		{
			angleX += 0.03;
			angleY += 0.02;
			hue += 2.0;
			if (hue >= 360.0) hue -= 360.0;

			Point2D projectedPoints[8];

			for (int i = 0; i < 8; ++i)
			{
				double y1 = cubeVertices[i].y * cos(angleX) - cubeVertices[i].z * sin(angleX);
				double z1 = cubeVertices[i].y * sin(angleX) + cubeVertices[i].z * cos(angleX);
				double x1 = cubeVertices[i].x;

				double x2 = x1 * cos(angleY) + z1 * sin(angleY);
				double z2 = -x1 * sin(angleY) + z1 * cos(angleY);
				double y2 = y1;

				double fov = 400.0;
				double distance = 500.0;
				double zProj = z2 + distance;

				projectedPoints[i].x = static_cast<int>(w / 2 + (x2 * fov) / zProj);
				projectedPoints[i].y = static_cast<int>(h / 2 + (y2 * fov) / zProj);
			}

			COLORREF color = CubeHSLtoRGB(hue, 1.0, 0.5);
			HPEN hPen = CreatePen(PS_SOLID, 4, color);
			HGDIOBJ hOldPen = SelectObject(hdc, hPen);

			for (int i = 0; i < 12; ++i) {
				MoveToEx(hdc, projectedPoints[edges[i][0]].x, projectedPoints[edges[i][0]].y, NULL);
				LineTo(hdc, projectedPoints[edges[i][1]].x, projectedPoints[edges[i][1]].y);
			}

			SelectObject(hdc, hOldPen);
			DeleteObject(hPen);

			Sleep(16);
		}

		ReleaseDC(NULL, hdc);
		RedrawWindow(NULL, NULL, NULL, MalwareDefs::REDRAW);

		return 0;
	}

	DWORD WINAPI Texts(LPVOID lpParam)
	{
		HDC hdc = GetDC(0);

		LOGFONTW lFont = { 0 };

		lFont.lfWidth = 20;
		lFont.lfHeight = 60;
		lFont.lfOrientation = 400;
		lFont.lfWeight = 600;
		lFont.lfUnderline = true;
		lFont.lfQuality = DRAFT_QUALITY;
		lFont.lfPitchAndFamily = DEFAULT_PITCH | FF_ROMAN;

		lstrcpy(lFont.lfFaceName, L"Arial Black");

		int state = 0;
		int r = 0;
		int g = 0;
		int b = 0;

		while (true)
		{
			if (state == 0)
			{
				g += 5; if (g >= 255) state = 1;
			}

			else if (state == 1)
			{
				r -= 5; if (r <= 0) state = 2;
			}

			else if (state == 2)
			{
				b += 5; if (b >= 255) state = 3;
			}

			else if (state == 3)
			{
				g -= 5; if (g <= 0) state = 4;
			}

			else if (state == 4)
			{
				r += 5; if (r >= 255) state = 5;
			}

			else if (state == 5)
			{
				b -= 5; if (b <= 0) state = 0;
			}

			lFont.lfEscapement = rand() % 60;

			HFONT hFont = CreateFontIndirectW(&lFont);
			SelectObject(hdc, hFont);

			SetTextColor(hdc, RGB(rand() % 255, rand() % 255, rand() % 255));
			SetBkColor(hdc, RGB(r, g, b));

			int index = rand() % 40;

			TextOutA(hdc, rand() % w, rand() % h, "Holioxide.exe", lstrlenA("Holioxide.exe"));

			Sleep(rand() % 5);
		}

		return 0;
	}

	DWORD WINAPI UDStretch(LPVOID lpParam)
	{
		// Credits to UltraDasher

		HDC hdc = GetDC(0);

		float radius = 0.f;
		double angle = 0;

		while (1)
		{
			hdc = GetDC(0);

			float x = (cos(angle)) * radius;
			float y = (sin(angle)) * radius;

			StretchBlt(hdc, x, y, w - x * 2, h - y * 2, hdc, 0, 0, w, h, SRCCOPY);
			radius += 0.1f;

			ReleaseDC(0, hdc);

			angle = fmod(angle + M_PI / radius, M_PI * radius) / 1.001;
		}
	}

	DWORD WINAPI GDIShader3(LPVOID lpParam)
	{
		HDC hdc = GetDC(0);
		HDC dcCopy = CreateCompatibleDC(hdc);

		BITMAPINFO bmpi = { 0 };
		HBITMAP bmp;

		bmpi.bmiHeader.biSize = sizeof(bmpi.bmiHeader);
		bmpi.bmiHeader.biWidth = w;
		bmpi.bmiHeader.biHeight = -h;
		bmpi.bmiHeader.biPlanes = 1;
		bmpi.bmiHeader.biBitCount = 32;
		bmpi.bmiHeader.biCompression = BI_RGB;

		RGBQUAD* rgbquad = NULL;

		bmp = CreateDIBSection(dcCopy, &bmpi, DIB_RGB_COLORS, (void**)&rgbquad, NULL, 0);
		SelectObject(dcCopy, bmp);

		while (1)
		{
			HBRUSH hBrush = CreateSolidBrush(RGB(rand() % 255, rand() % 255, rand() % 255));
			SelectObject(dcCopy, hBrush);

			StretchBlt(dcCopy, 0, 0, w, h, hdc, 0, 0, w, h, SRCCOPY | PATINVERT);

			static float t = 0.0f;
			t += 0.1f;

			for (int y = 0; y < h; y++)
			{
				int shift = (int)(sin((y + t * 20) * 0.03f) * 10);

				for (int x = 0; x < w; x++)
				{
					int sx = x + shift;
					if (sx < 0 || sx >= w) continue;

					RGBQUAD px = rgbquad[y * w + sx];

					rgbquad[y * w + x] = px;
				}
			}

			BitBlt(hdc, 0, 0, w, h, dcCopy, 0, 0, SRCCOPY);

			if (rand() % 5 == 4) RedrawWindow(NULL, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

			DeleteObject(hBrush);
			Sleep(50);
		}
	}

	DWORD WINAPI UDStretch2(LPVOID lpParam)
	{
		// Credits to UltraDasher

		HDC hdc = GetDC(0);

		float radius = 0.f;
		double angle = 0;

		while (1)
		{
			hdc = GetDC(0);

			float x = (cos(angle)) * radius;
			float y = (sin(angle)) * radius;

			StretchBlt(hdc, x, y, w - x * 2, h - y * 2, hdc, 0, 0, w, h, SRCINVERT);
			radius += 0.1f;

			ReleaseDC(0, hdc);

			angle = fmod(angle + M_PI / radius, M_PI * radius) / 1.001;
		}
	}

	DWORD WINAPI GDIShader4(LPVOID lpParam)
	{
		HDC hdc = GetDC(NULL);
		HDC hdcCopy = CreateCompatibleDC(hdc);
		BITMAPINFO bmpi = { 0 };
		HBITMAP bmp;

		bmpi.bmiHeader.biSize = sizeof(bmpi);
		bmpi.bmiHeader.biWidth = w;
		bmpi.bmiHeader.biHeight = h;
		bmpi.bmiHeader.biPlanes = 1;
		bmpi.bmiHeader.biBitCount = 32;
		bmpi.bmiHeader.biCompression = BI_RGB;


		PRGBQUAD_t rgbquad = NULL;
		HSL hslcolor;

		bmp = CreateDIBSection(hdc, &bmpi, DIB_RGB_COLORS, (void**)&rgbquad, NULL, 0);
		SelectObject(hdcCopy, bmp);

		INT i = 0;

		while (1)
		{
			hdc = GetDC(NULL);
			StretchBlt(hdcCopy, 0, 0, w, h, hdc, 0, 0, w, h, SRCCOPY);

			RGBQUAD_t rgbquadCopy;

			for (int x = 0; x < w; x++)
			{
				for (int y = 0; y < h; y++)
				{
					int index = y * w + x;

					int cx = x - (w / 2);

					int fx = i * (cbrt((cx * cx) + (i ^ y)));

					rgbquadCopy = rgbquad[index];

					hslcolor = RGBtoHSL(rgbquadCopy);
					hslcolor.h = fmod(fx / 400.f + y / h * .2f, 1.f);
					rgbquad[index] = HSLtoRGB(hslcolor);
				}
			}

			i++;

			StretchBlt(hdc, 0, 0, w, h, hdcCopy, 0, 0, w, h, SRCCOPY);
			ReleaseDC(NULL, hdc);
			DeleteDC(hdc);
		}

		return 0;
	}

	DWORD WINAPI GDIShader5(LPVOID lpParam)
	{
		HDC hdc = GetDC(NULL);
		HDC dcCopy = CreateCompatibleDC(hdc);

		BITMAPINFO bmpi = { 0 };
		HBITMAP bmp;

		bmpi.bmiHeader.biSize = sizeof(bmpi);
		bmpi.bmiHeader.biWidth = w;
		bmpi.bmiHeader.biHeight = h;
		bmpi.bmiHeader.biPlanes = 1;
		bmpi.bmiHeader.biBitCount = 32;
		bmpi.bmiHeader.biCompression = BI_RGB;

		PRGBQUAD_t rgbquad = NULL;
		HSL hslcolor;

		bmp = CreateDIBSection(hdc, &bmpi, DIB_RGB_COLORS, (void**)&rgbquad, NULL, 0);
		SelectObject(dcCopy, bmp);

		INT i = 0;

		while (1)
		{
			StretchBlt(dcCopy, 0, 0, w, h, hdc, 0, 0, w, h, SRCCOPY);

			RGBQUAD_t rgbquadCopy;

			for (int x = 0; x < w; x++)
			{
				for (int y = 0; y < h; y++)
				{
					int index = y * w + x;

					int cy = y - (h / 2);

					FLOAT fx = (x + i) & (cy * y + i);

					rgbquadCopy = rgbquad[index];

					hslcolor = RGBtoHSL(rgbquadCopy);
					hslcolor.h = fmod(fx / 100.5f + y / h * .1f, 1.1f);

					rgbquad[index] = HSLtoRGB(hslcolor);
				}
			}

			i++;
			StretchBlt(hdc, 0, 0, w, h, dcCopy, 0, 0, w, h, SRCCOPY);
		}

		return 0x00;
	}

	DWORD WINAPI GDIShader6(LPVOID lpParam)
	{
		HDC hdc = GetDC(NULL);
		HDC dcCopy = CreateCompatibleDC(hdc);

		BITMAPINFO bmpi = { 0 };
		HBITMAP bmp;

		bmpi.bmiHeader.biSize = sizeof(bmpi);
		bmpi.bmiHeader.biWidth = w;
		bmpi.bmiHeader.biHeight = h;
		bmpi.bmiHeader.biPlanes = 1;
		bmpi.bmiHeader.biBitCount = 32;
		bmpi.bmiHeader.biCompression = BI_RGB;

		PRGBQUAD_t rgbquad = NULL;
		HSL hsl;

		bmp = CreateDIBSection(hdc, &bmpi, DIB_RGB_COLORS, (void**)&rgbquad, NULL, 0);
		SelectObject(dcCopy, bmp);

		INT i = 0;

		while (1)
		{
			StretchBlt(dcCopy, 0, 0, w, h, hdc, 0, 0, w, h, SRCCOPY);

			RGBQUAD_t rgbquadCopy;

			for (int x = 0; x < w; x++)
			{
				for (int y = 0; y < h; y++)
				{
					int index = y * w + x;

					FLOAT fx = (x + i) & (y + i);

					rgbquadCopy = rgbquad[index];

					hsl = RGBtoHSL(rgbquadCopy);
					hsl.h = fmod(fx / 100.5f + y / h * .1f, 1.1f);
					
					hsl.s = 1.f;

					hsl.l *= 1.125f;

					if (hsl.l > .5f)
					{
						hsl.l -= .5f;
					}

					if (hsl.l < .25f)
					{
						hsl.l += .25f;
					}

					rgbquad[index] = HSLtoRGB(hsl);
				}
			}

			i++;
			StretchBlt(hdc, 0, 0, w, h, dcCopy, 0, 0, w, h, SRCCOPY);
		}

		return 0x00;
	}

	DWORD WINAPI MasherBlend(LPVOID lpParam)
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 1);

		POINT lppoint[3];

		while (true)
		{
			HDC hdc = GetDC(nullptr);
			HDC mhdc = CreateCompatibleDC(hdc);
			HBITMAP hbit = CreateCompatibleBitmap(hdc, w, h);
			HGDIOBJ holdbit = SelectObject(mhdc, hbit);

			int ws = w / 3;
			int hs = h / 3;

			if (dis(gen) == 1)
			{
				lppoint[0] = { 5 + 5, 0 - 5 };
				lppoint[1] = { w + ws, 0 + 30 };
				lppoint[2] = { hs - 30, h - 3 };
			}

			else
			{
				lppoint[0] = { 0 - 30, 0 + 30 };
				lppoint[1] = { ws - w, 0 - 30 };
				lppoint[2] = { h + 30, hs + 30 };
			}

			PlgBlt(mhdc, lppoint, hdc, 0, 0, w, h, nullptr, 0, 0);

			BLENDFUNCTION blend = { AC_SRC_OVER, 0, 60, 0 };
			AlphaBlend(hdc, 0, 0, w, h, mhdc, 0, 0, w, h, blend);

			SelectObject(mhdc, holdbit);
			DeleteObject(hbit);
			DeleteDC(mhdc);
			ReleaseDC(nullptr, hdc);

			Sleep(50);
		}
	}

	DWORD WINAPI GDIShader7(LPVOID lpParam)
	{
		HDC hdc = GetDC(NULL);
		HDC dcCopy = CreateCompatibleDC(hdc);

		BITMAPINFO bmi = { 0 };
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = -h;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;

		PRGBQUAD_t prgbDst = nullptr;
		HBITMAP hbmDst = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&prgbDst, NULL, 0);
		SelectObject(dcCopy, hbmDst);

		PRGBQUAD_t prgbSrc =
		(PRGBQUAD_t)VirtualAlloc(NULL, w * h * sizeof(RGBQUAD_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

		int t = 0;
		float div = (float)t / 30;
		float a = sinf(div) * 78.0f;
		float b = cosf(div) * 72.0f;

		while (1)
		{
			BitBlt(dcCopy, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
			memcpy(prgbSrc, prgbDst, w * h * sizeof(RGBQUAD_t));

			for (INT y = 0; y < h; y++)
			{
				for (INT x = 0; x < w; x++)
				{
					int u = (x + (int)a) % w;
					if (u < 0) u += w;

					int v = (y + (int)b) % h;
					if (v < 0) v += h;

					u %= w;
					v %= h;

					RGBQUAD_t rgbDst = prgbSrc[v * w + u];
					rgbDst.r += ~prgbSrc[y * w + x].r / 32;
					rgbDst.g += ~prgbSrc[y * w + x].g / 32;
					rgbDst.b += ~prgbSrc[y * w + x].b / 32;
					prgbDst[y * w + x] = rgbDst;
				}
			}

			BitBlt(hdc, 0, 0, w, h, dcCopy, 0, 0, SRCCOPY);
			Sleep(1);
		}

		DeleteObject(hbmDst);
		DeleteDC(dcCopy);
		ReleaseDC(0, hdc);
		return 0;
	}

	DWORD WINAPI SierspinskiTriangle(LPVOID lpParam)
	{
		HDC hdc = GetDC(NULL);

		HDC dcCopy = CreateCompatibleDC(hdc);
		HBITMAP memBM = CreateCompatibleBitmap(hdc, w, h);
		SelectObject(dcCopy, memBM);

		HICON hErrorIcon = LoadIcon(NULL, IDI_APPLICATION);

		Point3D top = { 0.0f, -280.0f, 0.0f };
		Point3D left = { -320.0f, 240.0f, 0.0f };
		Point3D right = { 320.0f, 240.0f, 0.0f };

		std::vector<Point3D> sierpinskiPoints;
		generateSierpinski(top, left, right, 4, sierpinskiPoints);

		float angleX = 0.0f;

		while (true)
		{
			angleX += 0.04f;

			BitBlt(dcCopy, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

			for (const auto& pt : sierpinskiPoints)
			{
				float rotY = pt.y * cosf(angleX) - pt.z * sinf(angleX);
				float rotZ = pt.y * sinf(angleX) + pt.z * cosf(angleX);
				float rotX = pt.x;

				float fov = 500.0f;
				float distance = 600.0f;
				float zProj = rotZ + distance;

				int screenX = static_cast<int>(w / 2 + (rotX * fov) / zProj);
				int screenY = static_cast<int>(h / 2 + (rotY * fov) / zProj);

				DrawIconEx(dcCopy, screenX - 16, screenY - 16, hErrorIcon, 32, 32, 0, NULL, DI_NORMAL);
			}

			BitBlt(hdc, 0, 0, w, h, dcCopy, 0, 0, SRCCOPY);

			Sleep(1);
		}

		DestroyIcon(hErrorIcon);
		DeleteObject(memBM);
		DeleteDC(dcCopy);
		ReleaseDC(NULL, hdc);
		RedrawWindow(NULL, NULL, NULL, REDRAW);

		return 0;
	}

	DWORD WINAPI RandomIcons(LPVOID lpParam)
	{
		INT directionX = rand() % w;
		INT directionY = rand() % h;
		INT nDirection = (rand() % 4) + 1;

		while (true)
		{
			HDC hdc = GetDC(HWND_DESKTOP);
			
			HICON Icons[6] =
			{
				LoadIcon(NULL, IDI_ERROR),
				LoadIcon(NULL, IDI_WARNING),
				LoadIcon(NULL, IDI_INFORMATION),
				LoadIcon(NULL, IDI_ASTERISK),
				LoadIcon(NULL, IDI_APPLICATION),
				LoadIcon(NULL, IDI_SHIELD),
			};

			DrawIcon(hdc, directionX, directionY, Icons[rand() % 6]);

			if (directionX >= w)
			{
				directionX = 0;
			}

			if (directionX < 0)
			{
				directionX = w - 0.1;
			}

			if (directionY >= h)
			{
				directionY = 0;
			}

			if (directionY < 0)
			{
				directionY = h - 0.1;
			}

			switch (nDirection)
				{
				case 1: directionX += 3; break;
				case 2: directionX -= 3; break;

				case 3: directionY += 3; break;
				case 4: directionY -= 3; break;
			}
			if ((rand() % 100) < 3)
			{
				nDirection = (rand() % 7) + 1;
			}

			ReleaseDC(NULL, hdc);
			Sleep(20);
			DeleteDC(hdc);
		}
	}

	DWORD WINAPI GDIShader8(LPVOID lpParam)
	{
		HDC hdc = GetDC(NULL);
		if (!hdc)
		{
			return 0;
		}

		BITMAPINFO bmi = { 0 };
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = -h;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		RGBQUAD* rgbScr = NULL;

		HDC dcCopy = CreateCompatibleDC(hdc);
		HBITMAP hbmp =
			CreateDIBSection(dcCopy, &bmi, DIB_RGB_COLORS, (void**)&rgbScr, NULL, 0);
		SelectObject(dcCopy, hbmp);

		while (1)
		{
			BitBlt(dcCopy, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

			int total = w * h;

			for (int i = 0; i < total; ++i)
			{
				BYTE idk = (rgbScr[i].rgbRed + rgbScr[i].rgbGreen + rgbScr[i].rgbBlue) * 3;
				idk += 40;
				if (idk > 255) idk = 255;
				rgbScr[i].rgbRed = idk;
				rgbScr[i].rgbGreen = idk;
				rgbScr[i].rgbBlue = idk;
				rgbScr[i].rgbReserved = 0;
			}

			BitBlt(hdc, 0, 0, w, h, dcCopy, 0, 0, SRCCOPY);
			Sleep(10);
		}

		DeleteObject(hbmp);
		DeleteDC(dcCopy);
		ReleaseDC(NULL, hdc);
		return 0;
	}

	DWORD WINAPI FinalGDIEffect(LPVOID lpParam)
	{
		while (true)
		{
			// This is a placeholder. I waiting someone make this effect
			MessageBox(NULL, L"Placeholder", L"Placeholder", MB_OK);
		}
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	HANDLE GDIPayload1 = CreateThread(NULL, 0, GDIPayloads::GDIShader1, NULL, 0, NULL); // HSL Shader
	HANDLE GDIPayloadWhile4 = CreateThread(NULL, 0, GDIPayloads::RandomIcons, NULL, 0, NULL); // Random Icons
	CreateThread(0, 0, Bytebeats::Sound1, 0, 0, 0);
	Sleep(20000); // 20 seconds delay before starting the next payload
	TerminateThread(GDIPayload1, 0);
	CloseHandle(GDIPayload1);
	HANDLE GDIPayload2 = CreateThread(NULL, 0, GDIPayloads::GDIShader2, NULL, 0, NULL); // HSL Shader
	CreateThread(0, 0, Bytebeats::Sound2, 0, 0, 0);
	Sleep(20000); // 20 seconds delay before starting the next payload
	TerminateThread(GDIPayload2, 0);
	CloseHandle(GDIPayload2);
	HANDLE GDIPayload3 = CreateThread(NULL, 0, GDIPayloads::Pats, NULL, 0, NULL); // PatBlt Effect
	HANDLE GDIPayloadWhile1 = CreateThread(NULL, 0, GDIPayloads::Cube, NULL, 0, NULL); // Cube
	CreateThread(0, 0, Bytebeats::Sound3, 0, 0, 0);
	Sleep(20000); // 20 seconds delay before starting the next payload
	HANDLE GDIPayload4 = CreateThread(NULL, 0, GDIPayloads::UDStretch, NULL, 0, NULL); // UltraDasher stretch
	HANDLE GDIPayloadWhile2 = CreateThread(NULL, 0, GDIPayloads::Texts, NULL, 0, NULL); // Random texts
	CreateThread(0, 0, Bytebeats::Sound4, 0, 0, 0);
	Sleep(20000); // 20 seconds delay
	TerminateThread(GDIPayload3, 0);
	CloseHandle(GDIPayload3);
	TerminateThread(GDIPayload4, 0);
	CloseHandle(GDIPayload4);
	HANDLE GDIPayload5 = CreateThread(NULL, 0, GDIPayloads::GDIShader3, NULL, 0, NULL); // Wavy PatBlt
	CreateThread(0, 0, Bytebeats::Sound5, 0, 0, 0);
	Sleep(20000); // 20 seconds delay
	TerminateThread(GDIPayload5, 0);
	CloseHandle(GDIPayload5);
	HANDLE GDIPayload6 = CreateThread(NULL, 0, GDIPayloads::UDStretch2, NULL, 0, NULL); // UltraDasher stretch again
	CreateThread(0, 0, Bytebeats::Sound6, 0, 0, 0);
	Sleep(20000); // 20 seconds delay
	TerminateThread(GDIPayload6, 0);
	CloseHandle(GDIPayload6);
	HANDLE GDIPayload7 = CreateThread(NULL, 0, GDIPayloads::GDIShader4, NULL, 0, NULL); // HSL Shader again
	CreateThread(0, 0, Bytebeats::Sound7, 0, 0, 0);
	Sleep(20000); // 20 seconds delay
	TerminateThread(GDIPayload7, 0);
	CloseHandle(GDIPayload7);
	HANDLE GDIPayload8 = CreateThread(NULL, 0, GDIPayloads::GDIShader5, NULL, 0, NULL); // More HSL Shader
	CreateThread(0, 0, Bytebeats::Sound8, 0, 0, 0);
	Sleep(20000); // 20 seconds delay
	TerminateThread(GDIPayload8, 0);
	CloseHandle(GDIPayload8);
	HANDLE GDIPayload9 = CreateThread(NULL, 0, GDIPayloads::GDIShader6, NULL, 0, NULL); // Even more HSL Shader
	CreateThread(0, 0, Bytebeats::Sound9, 0, 0, 0);
	Sleep(20000); // 20 seconds delay
	TerminateThread(GDIPayload9, 0);
	CloseHandle(GDIPayload9);
	TerminateThread(GDIPayloadWhile1, 0);
	CloseHandle(GDIPayloadWhile1);
	TerminateThread(GDIPayloadWhile2, 0);
	CloseHandle(GDIPayloadWhile2);
	HANDLE GDIPayloadWhile3 = CreateThread(NULL, 0, GDIPayloads::SierspinskiTriangle, NULL, 0, NULL);
	HANDLE GDIPayload10 = CreateThread(NULL, 0, GDIPayloads::MasherBlend, NULL, 0, NULL); // Blurred masher
	CreateThread(0, 0, Bytebeats::Sound10, 0, 0, 0);
	Sleep(20000); // 20 seconds delay
	TerminateThread(GDIPayload10, 0);
	CloseHandle(GDIPayload10);
	HANDLE GDIPayload11 = CreateThread(NULL, 0, GDIPayloads::GDIShader7, NULL, 0, NULL); // Shaking RGB
	CreateThread(0, 0, Bytebeats::Sound11, 0, 0, 0);
	Sleep(20000); // 20 seconds delay
	TerminateThread(GDIPayload11, 0);
	CloseHandle(GDIPayload11);
	HANDLE GDIPayload12 = CreateThread(NULL, 0, GDIPayloads::FinalGDIEffect, NULL, 0, NULL);
	Sleep(20000); // 20 seconds delay
	TerminateThread(GDIPayload12, 0);
	CloseHandle(GDIPayload12);
	TerminateThread(GDIPayloadWhile4, 0);
	CloseHandle(GDIPayloadWhile4);
	TerminateThread(GDIPayloadWhile3, 0);
	CloseHandle(GDIPayloadWhile3);
}