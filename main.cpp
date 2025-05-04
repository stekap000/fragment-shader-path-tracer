#include <windows.h>
#include "glad/glad.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>

#define __ignore__(x)((void)(x))
#define Internal static

typedef unsigned int u32;
typedef unsigned long long int u64;
typedef int s32;
typedef long long int s64;
typedef float f32;
typedef double f64;

bool global_engine_running = true;

Internal LRESULT CALLBACK win32_main_window_callback(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
	LRESULT result = 0;

	switch(message) {
		case WM_CLOSE: {
			global_engine_running = false;
		} break;
		case WM_DESTROY: {
			global_engine_running = false;
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
			PAINTSTRUCT paint = {};
			HDC window_dc = BeginPaint(window, &paint);
			__ignore__(window_dc);
			EndPaint(window, &paint);
		} break;
		default: {
			result = DefWindowProcW(window, message, wparam, lparam);
		}
	}

	return result;
}

Internal HWND win32_create_window(HINSTANCE instance, int width, int height) {
	WNDCLASSEXW window_class = {};

	window_class.cbSize			= sizeof(window_class);
	window_class.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
	window_class.lpfnWndProc	= win32_main_window_callback;
	window_class.hInstance		= instance;
	window_class.hCursor		= 0;
	window_class.lpszClassName	= L"ComputeShaderplaygroundWindowClass";

	HWND window = 0;
	
	if(RegisterClassExW(&window_class)) {
		DWORD window_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
		RECT window_rect = {};
		window_rect.left = 0;
		window_rect.right = width;
		window_rect.top = 0;
		window_rect.bottom = height;
		
		AdjustWindowRect(&window_rect, window_style, FALSE);
		
		window = CreateWindowExW(0, window_class.lpszClassName, L"ComputeShaderPlayground",
								 window_style,
								 CW_USEDEFAULT,
								 CW_USEDEFAULT,
								 (window_rect.right - window_rect.left),
								 (window_rect.bottom - window_rect.top),
								 0, 0, instance, 0);
	}

	return window;
}

Internal HGLRC win32_initialize_opengl_context(HDC window_dc) {
	HGLRC opengl_context = 0;

	PIXELFORMATDESCRIPTOR pixel_format_descriptor = {};
	pixel_format_descriptor.nSize				= sizeof(pixel_format_descriptor);
	pixel_format_descriptor.nVersion			= 1;
	pixel_format_descriptor.dwFlags				= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pixel_format_descriptor.iPixelType			= PFD_TYPE_RGBA;
	// NOTE(stekap): Seems like this should be 32 for RGBA and documentation is wrong.
	pixel_format_descriptor.cColorBits			= 32;
	pixel_format_descriptor.cAlphaBits			= 8;
	pixel_format_descriptor.cDepthBits			= 24;
	pixel_format_descriptor.cStencilBits	    = 8;

	int pixel_format = ChoosePixelFormat(window_dc, &pixel_format_descriptor);
	DescribePixelFormat(window_dc, pixel_format, sizeof(pixel_format_descriptor), &pixel_format_descriptor);

	if(pixel_format) {
		if(SetPixelFormat(window_dc, pixel_format, &pixel_format_descriptor)) {
			opengl_context = wglCreateContext(window_dc);
		}
		else {
			std::cout << "Pixel format could not be set (needed for creation of OpenGL context)." << std::endl;
		}
	}
	else {
		std::cout << "Could not find pixel format that matches provided description." << std::endl;
	}

	return opengl_context;
}

Internal void win32_handle_window_messages(HWND window) {
	MSG message = {};
	
	while(PeekMessageW(&message, window, 0, 0, PM_REMOVE)) {
		switch(message.message) {
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP: {
				unsigned long long virtual_key_code = message.wParam;
				unsigned char key_was_down = (message.lParam & (1 << 30)) != 0;
				unsigned char key_is_down = (message.lParam & (1 << 31)) == 0;

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
				if(key_was_down != key_is_down) {
					
				}

				// NOTE(stekap): This is on press.
				if(key_is_down && !key_was_down) {
					switch(virtual_key_code) {
						case VK_ESCAPE: {
							global_engine_running = false;
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
				TranslateMessage(&message);
				DispatchMessage(&message);
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

u32 create_shader_program(const char* vertex_shader_path, const char* fragment_shader_path) {
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

struct Time {
	LARGE_INTEGER last_ticks;
	LARGE_INTEGER end_ticks;
	LARGE_INTEGER ticks_frequency;
	u64 frame_ticks = 0;
	f64 frame_time = 0;
	f64 elapsed_seconds = 0;

	void update_time_at_frame_end() {
		QueryPerformanceCounter(&end_ticks);
		frame_ticks = (end_ticks.QuadPart - last_ticks.QuadPart);
		frame_time = ((f64)frame_ticks / (f64)ticks_frequency.QuadPart);
		elapsed_seconds += frame_time;
		last_ticks = end_ticks;
	}
};

#if !defined(ATTACH_CONSOLE)
int CALLBACK WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_code) {
	__ignore__(previous_instance);
	__ignore__(command_line);
	__ignore__(show_code);
#else
int main() {	
	HINSTANCE instance = (HINSTANCE)GetModuleHandle(0);
#endif

	Time time = {};
	QueryPerformanceFrequency(&time.ticks_frequency);

	int width = 400;
	int height = 400;
	HWND window = win32_create_window(instance, width, height);

	if(window) {
		std::cout << "Window created." << std::endl;
		
		HDC hdc = GetDC(window);
		HGLRC opengl_context = win32_initialize_opengl_context(hdc);

		if(opengl_context) {
			std::cout << "OpenGL context created." << std::endl;
			
			wglMakeCurrent(hdc, opengl_context);

			if(gladLoadGL()) {
				std::cout << "GLAD loaded." << std::endl;

				glViewport(0, 0, width, height);

				f32 target_rectangle_vertex_data[] = {
					-1.0f, -1.0f,  0.0f,  0.0f,  0.0f,
					 1.0f,  1.0f,  0.0f,  1.0f,  1.0f,
					-1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
					-1.0f, -1.0f,  0.0f,  0.0f,  0.0f,
					 1.0f, -1.0f,  0.0f,  1.0f,  0.0f,
					 1.0f,  1.0f,  0.0f,  1.0f,  1.0f,
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

				const u32 texture_width = 400, texture_height = 400;
				u32 texture;
				glGenTextures(1, &texture);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, texture);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texture_width, texture_height, 0, GL_RGBA, 
							 GL_FLOAT, NULL);
				glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
				
				u32 base_shader_program = create_shader_program("shaders/base.vert", "shaders/base.frag");
				use_shader_program(base_shader_program);
				glUniform1i(glGetUniformLocation(base_shader_program, "sampler"), 0);
				
				u32 compute_shader = create_compute_shader_program("shaders/compute_shader.comp");

				QueryPerformanceCounter(&time.last_ticks);
				while(global_engine_running) {
					win32_handle_window_messages(window);
					
					glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
					glClear(GL_COLOR_BUFFER_BIT);
					
					use_shader_program(compute_shader);
					glDispatchCompute((u32)texture_width, (u32)texture_height, 1);
					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

					use_shader_program(base_shader_program);
					glDrawArrays(GL_TRIANGLES, 0, 6);

					SwapBuffers(hdc);

					time.update_time_at_frame_end();
					SetWindowText(window, std::to_string(time.frame_time).c_str());
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
