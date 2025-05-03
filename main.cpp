#include <windows.h>
#include "glad/glad.h"

typedef float f32;
typedef unsigned int u32;
typedef int s32;

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

Internal HWND Win32CreateWindow(HINSTANCE Instance, int width, int height) {
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

Internal HGLRC Win32InitializeOpenGLContext(HDC WindowDC) {
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

Internal void Win32HandleWindowMessages(HWND Window) {
	MSG Message = {};
	
	while(PeekMessageW(&Message, Window, 0, 0, PM_REMOVE)) {
		switch(Message.message) {
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP: {
				unsigned long long VirtualKeyCode = Message.wParam;
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

Internal char* read_entire_text_file(const char* filename) {
	char* data = 0;
	unsigned long size = 0;
		
	FILE* file = fopen(filename, "rb");
	if(file) {
		fseek(file, 0, SEEK_END);
		size = ftell(file);
		data = (char*)malloc(size + 1);
		data[size] = 0;
		fseek(file, 0, SEEK_SET);
	
		if(fread(data, 1, size, file) != size) {
			free(data);
			data = 0;
		}

		fclose(file);
	}
		
	return data;
}

u32 create_shader_program (const char* vertex_shader_path, const char* fragment_shader_path) {
	u32 id = 0;
		
	char* vertex_shader_source = read_entire_text_file(vertex_shader_path);
	if(!vertex_shader_source) {
		std::cout << "Zero pointer provided as vertex shader source path" << std::endl;
		return 0;
	}
	char* fragment_shader_source = read_entire_text_file(fragment_shader_path);
	if(!fragment_shader_source) {
		std::cout << "Zero pointer provided as fragment shader source path" << std::endl;
		return 0;
	}

	u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vertex_shader, 1, &vertex_shader_source, 0);
	glShaderSource(fragment_shader, 1, &fragment_shader_source, 0);

	glCompileShader(vertex_shader);
		
	int valid;
	char info[512];
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &valid);
	if(!valid) {
		glGetShaderInfoLog(vertex_shader, 512, 0, info);
		std::cout << "Vertex shader compilation failed. " << info << std::endl;
	}

	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &valid);
	if(!valid) {
		glGetShaderInfoLog(fragment_shader, 512, 0, info);
		std::cout << "Fragment shader compilation failed. " << info << std::endl;
	}
		
	id = glCreateProgram();
	glAttachShader(id, vertex_shader);
	glAttachShader(id, fragment_shader);
	glLinkProgram(id);
	
	glGetProgramiv(id, GL_LINK_STATUS, &valid);
	if(!valid) {
		glGetProgramInfoLog(id, 512, 0, info);
		std::cout << "Shader program linking failed. " << info << std::endl;
	}

	free(vertex_shader_source);
	free(fragment_shader_source);

	glDetachShader(id, vertex_shader);
	glDetachShader(id, fragment_shader);
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return id;
}

Internal u32 create_compute_shader_program(const char* compute_shader_path) {
	u32 id = 0;

	char* compute_shader_source = read_entire_text_file(compute_shader_path);
	if(!compute_shader_source) {
		std::cout << "Zero pointer provided as compute shader source path" << std::endl;
		return 0;
	}

	u32 compute_shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute_shader, 1, &compute_shader_source, 0);
	glCompileShader(compute_shader);

	s32 valid;
	char info[512];
	glGetShaderiv(compute_shader, GL_COMPILE_STATUS, &valid);
	if(!valid) {
		glGetShaderInfoLog(compute_shader, 512, 0, info);
		std::cout << "Compute shader compilation failed. " << info << std::endl;
	}

	id = glCreateProgram();
	glAttachShader(id, compute_shader);
	glLinkProgram(id);

	glGetProgramiv(id, GL_LINK_STATUS, &valid);
	if(!valid) {
		glGetProgramInfoLog(id, 512, 0, info);
		std::cout << "Compute shader program linking failed. " << info << std::endl;;
	}
	
	free(compute_shader_source);

	glDetachShader(id, compute_shader);
	glDeleteShader(compute_shader);
	
	return id;
}

Internal void use_shader_program(u32 id) {
	glUseProgram(id);
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

				f32 target_rectangle_vertex_data[] = {
					-0.5f, -0.5f,  0.0f,  0.0f,  0.0f,
					 0.5f,  0.5f,  0.0f,  1.0f,  1.0f,
					-0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
					-0.5f, -0.5f,  0.0f,  0.0f,  0.0f,
					 0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
					 0.5f,  0.5f,  0.0f,  1.0f,  1.0f,
				};
									
				u32 target_rect_vbo;
				glGenBuffers(1, &target_rect_vbo);
				glBindBuffer(GL_ARRAY_BUFFER, target_rect_vbo);
				glBufferData(GL_ARRAY_BUFFER, sizeof(target_rectangle_vertex_data), target_rectangle_vertex_data, GL_STATIC_DRAW);

				u32 vao;
				glGenVertexArrays(1, &vao);
				glBindVertexArray(vao);

				glEnableVertexAttribArray(0);
				glEnableVertexAttribArray(1);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(f32), (void*)(0));
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(f32), (void*)(3*sizeof(f32)));

				const u32 TEXTURE_WIDTH = 400, TEXTURE_HEIGHT = 400;
				u32 texture;
				glGenTextures(1, &texture);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, texture);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, 
							 GL_FLOAT, NULL);
				glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
				
				u32 base_shader_program = create_shader_program("base.vert", "base.frag");
				use_shader_program(base_shader_program);
				glUniform1i(glGetUniformLocation(base_shader_program, "sampler"), 0);
				
				u32 compute_shader = create_compute_shader_program("compute_shader.comp");
				
				while(GlobalEngineRunning) {
					Win32HandleWindowMessages(window);
					
					glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
					glClear(GL_COLOR_BUFFER_BIT);
					
					use_shader_program(compute_shader);
					glDispatchCompute((u32)TEXTURE_WIDTH, (u32)TEXTURE_HEIGHT, 1);
					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

					use_shader_program(base_shader_program);
					glDrawArrays(GL_TRIANGLES, 0, 6);

					SwapBuffers(hdc);
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
