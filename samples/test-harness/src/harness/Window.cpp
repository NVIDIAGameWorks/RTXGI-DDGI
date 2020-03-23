/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN         // Exclude rarely-used items from Windows headers.
#endif

#include <Windows.h>
#include <iostream>
#include <Keyboard.h>
#include <Mouse.h>

#include "UI.h"

/**
 * Windows message loop.
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) 
{
    if (UI::WndProc(hWnd, message, wParam, lParam)) return true;

    PAINTSTRUCT ps;
    switch( message ) 
    {
        case WM_PAINT:
            BeginPaint( hWnd, &ps );
            EndPaint( hWnd, &ps );
            break;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0; // Disable ALT application menu  
            break;
        case WM_ACTIVATEAPP:
            DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
            DirectX::Mouse::ProcessMessage(message, wParam, lParam);
            break;
        case WM_INPUT:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_MOUSEHOVER:
            if (UI::WantsMouseCapture()) return false;
            DirectX::Mouse::ProcessMessage(message, wParam, lParam);
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
            break;
        case WM_DESTROY:
            PostQuitMessage( 0 );
            break; 
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

namespace Window
{

/**
* Create a window.
*/
bool Create(LONG width, LONG height, HINSTANCE &instance, HWND &window, LPCWSTR title)
{
    // Register the window class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = instance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "WindowClass";
    wcex.hIcon = nullptr;
    wcex.hIconSm = nullptr;

    if (!RegisterClassEx(&wcex))
    {
        throw std::runtime_error("Failed to register window!");
    }

    // Get the desktop resolution
    RECT desktop;
    const HWND hDesktop = GetDesktopWindow();
    GetWindowRect(hDesktop, &desktop);

    int x = (desktop.right - width) / 2;

    // Create the window
    RECT rc = { 0, 0, width, height };
	DWORD dwStyle = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    AdjustWindowRect(&rc, dwStyle, FALSE);
    window = CreateWindowExW(NULL, L"WindowClass", title, dwStyle, x, 0, (rc.right - rc.left), (rc.bottom - rc.top), NULL, NULL, instance, NULL);
    if (!window) return false;

    // Set the window icon
    HANDLE hIcon = LoadImageA(GetModuleHandle(NULL), "nvidia.ico", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
    SendMessage(window, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    // Show the window
    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);

    return true;
}

}
