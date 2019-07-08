#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <inttypes.h>
#include <string>

#include <d3d11.h>
#include <D3D11Shader.h>
#include <D3Dcompiler.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3dcompiler.lib")

#include "Detours/detours.h"
#pragma comment(lib, "Detours/detours.lib")

#include "renderer.h"

#define LOG_STREAM stdout

#define FONT_SIZE 38.0f
#define FONT_TYPE L"Verdana"

std::unique_ptr<Renderer> renderer;

typedef HRESULT(__stdcall *D3D11PresentHook) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
D3D11PresentHook phookD3D11Present = NULL;

ID3D11Device *pDevice = NULL;
ID3D11DeviceContext *pContext = NULL;
DWORD_PTR* pSwapChainVtable = NULL;

ID3D11Texture2D* pRenderTargetTexture = NULL;
ID3D11RenderTargetView* pRenderTargetView = NULL;

UINT numViewports = 1;
D3D11_VIEWPORT viewport;
float screenCenterX;
float screenCenterY;

enum MsgType
{
	INFO_MSG = 0,
	WARNING_MSG = 1,
	ERROR_MSG = 2
};

void Log(std::string msg, MsgType type = INFO_MSG)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	fprintf(LOG_STREAM, "[");
	SetConsoleTextAttribute(hConsole, 11);
	fprintf(LOG_STREAM, "D3D11");
	SetConsoleTextAttribute(hConsole, 7);
	fprintf(LOG_STREAM, "][");

	switch (type)
	{
	case INFO_MSG:
		SetConsoleTextAttribute(hConsole, 10);
		fprintf(LOG_STREAM, "INFO");
		break;
	case WARNING_MSG:
		SetConsoleTextAttribute(hConsole, 14);
		fprintf(LOG_STREAM, "WARNING");
		break;
	case ERROR_MSG:
		SetConsoleTextAttribute(hConsole, 12);
		fprintf(LOG_STREAM, "ERROR");
		break;
	}

	SetConsoleTextAttribute(hConsole, 7);
	fprintf(LOG_STREAM, "] %s\n", msg.c_str());
}

template <typename ...Args>
std::string FormatString(const std::string& format, Args && ...args)
{
	auto size = std::snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args)...);
	std::string output(size + 1, '\0');
	std::sprintf(&output[0], format.c_str(), std::forward<Args>(args)...);
	return output;
}

bool firstTime = true;
HRESULT __stdcall hookD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (firstTime)
	{
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void **)&pDevice)))
		{
			pSwapChain->GetDevice(__uuidof(pDevice), (void**)&pDevice);
			pDevice->GetImmediateContext(&pContext);
			Log("D3D11Device Initialized");
		}
		else
		{
			Log("Failed to initialize D3D11Device", ERROR_MSG);
		}

		if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pRenderTargetTexture)))
		{
			pDevice->CreateRenderTargetView(pRenderTargetTexture, NULL, &pRenderTargetView);
			pRenderTargetTexture->Release();
			Log("D3D11RenderTargetView Initialized");
		}
		else
		{
			Log("Failed to initialize D3D11RenderTargetView", ERROR_MSG);
		}

		pContext->RSGetViewports(&numViewports, &viewport);
		screenCenterX = viewport.Width / 2.0f;
		screenCenterY = viewport.Height / 2.0f;

		Log(FormatString("D3D11Viewport resolution: %.0f x %.0f", viewport.Width, viewport.Height));

		renderer = std::make_unique<Renderer>(pDevice);

		firstTime = false;
	}

	pContext->OMSetRenderTargets(1, &pRenderTargetView, NULL);

	renderer->begin();

	std::wstring str = L"D3D11Present Drawing!";
	auto size = renderer->getTextExtent(str, FONT_SIZE, FONT_TYPE);
	renderer->drawText(Vec2(
		screenCenterX - size.x * .5f, 
		screenCenterY - size.y * .5f), 
		str, 
		Color{ 0.f, 1.f, 1.f, 1.f }, 
		0, 
		FONT_SIZE, 
		FONT_TYPE);
	renderer->drawFilledRect(Vec4(
		screenCenterX - 20.f - size.x * .5f,
		screenCenterY - 20.f - size.y * .5f,
		40.f + size.x,
		40.f + size.y),
		Color{ 0.f, 0.f, 0.f, .75f });

	renderer->draw();
	renderer->end();

	return phookD3D11Present(pSwapChain, SyncInterval, Flags);
}


LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

DWORD __stdcall InitHook(LPVOID)
{
	Log("Setting up D3D11Present hook");

	HMODULE hDXGIDLL = 0;
	do
	{
		hDXGIDLL = GetModuleHandle("dxgi.dll");
		Sleep(100);
	} while (!hDXGIDLL);
	Sleep(100);

	IDXGISwapChain* pSwapChain;

	WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
	RegisterClassExA(&wc);

	HWND hWnd = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

	D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
	D3D_FEATURE_LEVEL obtainedLevel;
	ID3D11Device* d3dDevice = nullptr;
	ID3D11DeviceContext* d3dContext = nullptr;

	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(scd));
	scd.BufferCount = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	scd.OutputWindow = hWnd;
	scd.SampleDesc.Count = 1;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	scd.Windowed = ((GetWindowLongPtr(hWnd, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

	scd.BufferDesc.Width = 1;
	scd.BufferDesc.Height = 1;
	scd.BufferDesc.RefreshRate.Numerator = 0;
	scd.BufferDesc.RefreshRate.Denominator = 1;

	UINT createFlags = 0;
#ifdef _DEBUG
	createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	IDXGISwapChain* d3dSwapChain = 0;

	if (FAILED(D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createFlags,
		requestedLevels,
		sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL),
		D3D11_SDK_VERSION,
		&scd,
		&pSwapChain,
		&pDevice,
		&obtainedLevel,
		&pContext)))
	{
		Log("Failed to create D3D device and swapchain", ERROR_MSG);
		return NULL;
	}

	pSwapChainVtable = (DWORD_PTR*)pSwapChain;
	pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];

	Log(FormatString("SwapChain:              0x%" PRIXPTR, pSwapChain));
	Log(FormatString("SwapChainVtable:        0x%" PRIXPTR, pSwapChainVtable));
	Log(FormatString("Device:                 0x%" PRIXPTR, pDevice));
	Log(FormatString("DeviceContext:          0x%" PRIXPTR, pContext));
	Log(FormatString("D3D11Present:           0x%" PRIXPTR, pSwapChainVtable[8]));

	phookD3D11Present = (D3D11PresentHook)pSwapChainVtable[8];
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)phookD3D11Present, hookD3D11Present);
	DetourTransactionCommit();

	DWORD dwOld;
	VirtualProtect(phookD3D11Present, 2, PAGE_EXECUTE_READWRITE, &dwOld);

	while (true)
	{
		Sleep(10);
	}

	pDevice->Release();
	pContext->Release();
	pSwapChain->Release();

	return NULL;
}

BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);

		FreeConsole();
		AllocConsole();
		SetConsoleTitleA("D3D11 - DEBUG");
		freopen("CON", "w", stdout);

		Log("Injected Successfully");

		CreateThread(NULL, 0, InitHook, NULL, 0, NULL);
		break;
	}
	case DLL_PROCESS_DETACH:
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(PVOID&)pSwapChainVtable[8], hookD3D11Present);
		DetourTransactionCommit();
		break;
	}
	}

	return TRUE;
}
