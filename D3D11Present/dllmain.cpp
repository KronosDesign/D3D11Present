#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <inttypes.h>
#include <tchar.h>
#include <string>

#include <d3d11.h>
#include <D3D11Shader.h>
#include <D3Dcompiler.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3dcompiler.lib")

#include "Detours/detours.h"
#pragma comment(lib, "Detours/detours.lib")

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "renderer.h"
#include "logger.h"

#define FONT_SIZE 16.0f
#define FONT_TYPE L"Verdana"

std::unique_ptr<Renderer> customRenderer;

typedef HRESULT(__stdcall *D3D11PresentHook) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
D3D11PresentHook phookD3D11Present = NULL;

DWORD_PTR* pSwapChainVtable = NULL;
ID3D11Device *pDevice = NULL;
ID3D11DeviceContext *pContext = NULL;
ID3D11RenderTargetView* pRenderTargetView = NULL;

WNDPROC hGameWindowProc;

D3D11_VIEWPORT viewport;
float screenCenterX;
float screenCenterY;

void DrawImGui()
{
	ImGui::Begin("ImGui D3D11", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);
	ImGui::Spacing();

	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("Client size: %.0f x %.0f", viewport.Width, viewport.Height);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	static bool bShowDemo = false;
	ImGui::Checkbox("Show Demo Window", &bShowDemo);

	ImGui::Spacing();
	ImGui::End();

	if (bShowDemo)
	{
		ImGui::ShowDemoWindow(&bShowDemo);
	}
}

void DrawCustomRenderer()
{
	std::wstring str = L"D3D11 Custom Renderer";
	auto size = customRenderer->getTextExtent(str, FONT_SIZE, FONT_TYPE);
	customRenderer->drawText(Vec2(
		screenCenterX - size.x * .5f,
		screenCenterY - size.y * .5f),
		str,
		Color{ 0.f, 1.f, 1.f, 1.f },
		0,
		FONT_SIZE,
		FONT_TYPE);
	customRenderer->drawFilledRect(Vec4(
		screenCenterX - 20.f - size.x * .5f,
		screenCenterY - 20.f - size.y * .5f,
		40.f + size.x,
		40.f + size.y),
		Color{ 0.f, 0.f, 0.f, .75f });
}

HWND FindWindow(DWORD pid, TCHAR* className)
{
	HWND hCurWnd = GetTopWindow(0);
	while (hCurWnd != NULL)
	{
		DWORD cur_pid;
		DWORD dwTheardId = GetWindowThreadProcessId(hCurWnd, &cur_pid);

		if (cur_pid == pid)
		{
			if (IsWindowVisible(hCurWnd) != 0)
			{
				TCHAR szClassName[256];
				GetClassName(hCurWnd, szClassName, 256);
				if (_tcscmp(szClassName, className) == 0)
				{
					return hCurWnd;
				}
			}
		}
		hCurWnd = GetNextWindow(hCurWnd, GW_HWNDNEXT);
	}
	return NULL;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK hookWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CallWindowProc(ImGui_ImplWin32_WndProcHandler, hWnd, uMsg, wParam, lParam);

	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
		return true;
	}

	return CallWindowProc(hGameWindowProc, hWnd, uMsg, wParam, lParam);
}

bool initRendering = true;
HRESULT __stdcall hookD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (initRendering)
	{
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void **)&pDevice)) &&
			SUCCEEDED(pSwapChain->GetDevice(__uuidof(pDevice), (void**)&pDevice)))
		{
			pDevice->GetImmediateContext(&pContext);
			Log::Info("D3D11Device Initialized");
		}
		else
		{
			Log::Error("Failed to initialize D3D11Device");
		}

		ID3D11Texture2D* pRenderTargetTexture = NULL;
		if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pRenderTargetTexture)) &&
			SUCCEEDED(pDevice->CreateRenderTargetView(pRenderTargetTexture, NULL, &pRenderTargetView)))
		{
			pRenderTargetTexture->Release();
			Log::Info("D3D11RenderTargetView Initialized");
		}
		else
		{
			Log::Error("Failed to initialize D3D11RenderTargetView");
		}

		UINT numViewports = 1;
		pContext->RSGetViewports(&numViewports, &viewport);
		screenCenterX = viewport.Width / 2.0f;
		screenCenterY = viewport.Height / 2.0f;

		Log::Info("Viewport resolution: %.0f x %.0f", viewport.Width, viewport.Height);

		ImGui::CreateContext();

		HWND hGameWindow = FindWindow(GetCurrentProcessId(), "UnrealWindow");
		hGameWindowProc = (WNDPROC)SetWindowLongPtr(hGameWindow, GWLP_WNDPROC, (LONG_PTR)hookWndProc);
		ImGui_ImplWin32_Init(hGameWindow);

		ImGui_ImplDX11_CreateDeviceObjects();
		ImGui_ImplDX11_Init(pDevice, pContext);

		customRenderer = std::make_unique<Renderer>(pDevice);

		initRendering = false;
	}

	// must call before drawing
	pContext->OMSetRenderTargets(1, &pRenderTargetView, NULL);

	// ImGui Rendering ---------------------------------------------

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	DrawImGui();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// Custom Renderer ---------------------------------------------

	customRenderer->begin();

	DrawCustomRenderer();

	customRenderer->draw();
	customRenderer->end();

	return phookD3D11Present(pSwapChain, SyncInterval, Flags);
}


LRESULT WINAPI tmpWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

DWORD __stdcall InitHook(LPVOID)
{
	Log::Info("Setting up D3D11Present hook");

	HMODULE hDXGIDLL = 0;
	do
	{
		hDXGIDLL = GetModuleHandle("dxgi.dll");
		Sleep(100);
	} while (!hDXGIDLL);
	Sleep(100);

	IDXGISwapChain* pSwapChain;

	WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, tmpWndProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
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
		Log::Error("Failed to create D3D device and swapchain");
		return NULL;
	}

	pSwapChainVtable = (DWORD_PTR*)pSwapChain;
	pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];

	Log::Info("BaseAddr:               0x%" PRIXPTR, (DWORD_PTR)GetModuleHandle(NULL));
	Log::Info("SwapChain:              0x%" PRIXPTR, pSwapChain);
	Log::Info("SwapChainVtable:        0x%" PRIXPTR, pSwapChainVtable);
	Log::Info("Device:                 0x%" PRIXPTR, pDevice);
	Log::Info("DeviceContext:          0x%" PRIXPTR, pContext);
	Log::Info("D3D11Present:           0x%" PRIXPTR, pSwapChainVtable[8]);

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

		Log::Info("Injected Successfully");

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
