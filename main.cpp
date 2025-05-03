#include <windows.h>
#include "glad/glad.h"

#include <iostream>

#define __ignore__(x)((void)(x))
#define Internal static

bool GlobalEngineRunning = true;

Internal LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
	LRESULT Result = 0;

	switch(Message) {
		case WM_CLOSE: {
			GlobalEngineRunning = false;
		} break;
		case WM_DESTROY: {
			GlobalEngineRunning = false;
		} break;
		case WM_LBUTTONDOWN: {

		} break;
		case WM_MBUTTONDOWN: {

		} break;
		case WM_RBUTTONDOWN: {
			
		} break;
		case WM_MOUSEWHEEL: {
			
		} break;
		case WM_MOUSEMOVE: {
			
		} break;
		case WM_SIZE: {
			
		} break;
		case WM_SIZING: {
			
		} break;
		case WM_PAINT: {
			PAINTSTRUCT Paint = {};
			HDC WindowDC = BeginPaint(Window, &Paint);
			__ignore__(WindowDC);
			EndPaint(Window, &Paint);
		} break;
		default: {
			Result = DefWindowProcW(Window, Message, WParam, LParam);
		}
	}

	return Result;
}

HWND Win32CreateWindow(HINSTANCE Instance, int width, int height) {
	WNDCLASSEXW WindowClass = {};

	WindowClass.cbSize			= sizeof(WindowClass);
	WindowClass.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
	WindowClass.lpfnWndProc		= Win32MainWindowCallback;
	WindowClass.hInstance		= Instance;
	WindowClass.hCursor			= 0;
	WindowClass.lpszClassName	= L"EngineWindowClass";

	HWND Window = 0;
	
	if(RegisterClassExW(&WindowClass)) {
		DWORD WindowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
		RECT WindowRect = {};
		WindowRect.left = 0;
		WindowRect.right = width;
		WindowRect.top = 0;
		WindowRect.bottom = height;
		
		AdjustWindowRect(&WindowRect, WindowStyle, FALSE);
		
		Window = CreateWindowExW(0, WindowClass.lpszClassName, L"ComputeShaderPlayground",
								 WindowStyle,
								 CW_USEDEFAULT,
								 CW_USEDEFAULT,
								 (WindowRect.right - WindowRect.left),
								 (WindowRect.bottom - WindowRect.top),
								 0, 0, Instance, 0);
	}

	return Window;
}

HGLRC Win32InitializeOpenGLContext(HDC WindowDC) {
	HGLRC OpenGLContext = 0;

	PIXELFORMATDESCRIPTOR PixelFormatDescriptor = {};
	PixelFormatDescriptor.nSize					= sizeof(PixelFormatDescriptor);
	PixelFormatDescriptor.nVersion				= 1;
	PixelFormatDescriptor.dwFlags				= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	PixelFormatDescriptor.iPixelType			= PFD_TYPE_RGBA;
	// NOTE(stekap): Seems like this should be 32 for RGBA and documentation is wrong.
	PixelFormatDescriptor.cColorBits			= 32;
	PixelFormatDescriptor.cAlphaBits			= 8;
	PixelFormatDescriptor.cDepthBits			= 24;
	PixelFormatDescriptor.cStencilBits		    = 8;

	int PixelFormat = ChoosePixelFormat(WindowDC, &PixelFormatDescriptor);
	DescribePixelFormat(WindowDC, PixelFormat, sizeof(PixelFormatDescriptor), &PixelFormatDescriptor);

	if(PixelFormat) {
		if(SetPixelFormat(WindowDC, PixelFormat, &PixelFormatDescriptor)) {
			OpenGLContext = wglCreateContext(WindowDC);
		}
		else {
			std::cout << "Pixel format could not be set (needed for creation of OpenGL context)." << std::endl;
		}
	}
	else {
		std::cout << "Could not find pixel format that matches provided description." << std::endl;
	}

	return OpenGLContext;
}

void Win32HandleWindowMessages(HWND Window) {
	MSG Message = {};
	
	while(PeekMessageW(&Message, Window, 0, 0, PM_REMOVE)) {
		switch(Message.message) {
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP: {
				unsigned long VirtualKeyCode = Message.wParam;
				unsigned char KeyWasDown = (Message.lParam & (1 << 30)) != 0;
				unsigned char KeyIsDown = (Message.lParam & (1 << 31)) == 0;

				// TODO(stekap): Later handle both on press and on release, as well as hold, in a more
				//               structured way.

				// NOTE(stekap): When button is pressed and held, windows will generate one message
				//               and then insert a small delay during which there will be no messages
				//               for that particular key. After delay, it seems (from testing) that
				//               windows starts generating messages for that key but at a higher
				//               frequency (probably shortens some internal timer that is used
				//               to artificially inject delay between successive messages for given
				//               button). I guess that the potential reason for this might be that
				//               if you hold down a button, that means that you are trying to
				//               continually do something, so the OS gives you a higher event rate,
				//               thus allowing you to do what you are doing more frequently, just in
				//               case that you are directly reacting to messages, instead of keeping
				//               state.
				//               We keep state, because delays between successive button down messages
				//               are too high for them to be used for hold. 

				// NOTE(stekap): This detects button press and release ie. end states when button
				//               is held down.
				if(KeyWasDown != KeyIsDown) {
					
				}

				// NOTE(stekap): This is on press.
				if(KeyIsDown && !KeyWasDown) {
					switch(VirtualKeyCode) {
						case VK_ESCAPE: {
							GlobalEngineRunning = false;
						} break;
						case 'O': {
							
						} break;
						case 'P': {
							
						} break;
						case 'F': {
							
						} break;
					}
				}
			} break;
			default: {
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			}
		}
	}
}

#if !defined(ATTACH_CONSOLE)
int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PreviousInstance, LPSTR CommandLine, int ShowCode) {
	__ignore__(PreviousInstance);
	__ignore__(CommandLine);
	__ignore__(ShowCode);
#else
int main() {	
	HINSTANCE Instance = (HINSTANCE)GetModuleHandle(0);
#endif

	int width = 400;
	int height = 400;
	HWND window = Win32CreateWindow(Instance, width, height);

	if(window) {
		HDC hdc = GetDC(window);
		HGLRC OpenGLContext = Win32InitializeOpenGLContext(hdc);

		if(OpenGLContext) {
			wglMakeCurrent(hdc, OpenGLContext);

			if(gladLoadGL()) {
				std::cout << "GLAD loaded" << std::endl;

				glViewport(0, 0, width, height);

				while(GlobalEngineRunning) {
					Win32HandleWindowMessages(window);

					
				}
			}
			else {
				std::cout << "GLAD could not be loaded" << std::endl;
			}
		}
		else {
			std::cout << "OpenGL context could not be created." << std::endl;
		}
	}
	else {
		std::cout << "Could not create window." << std::endl;
	}

	return 0;
}
