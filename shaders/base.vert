#version 420 core

layout (location = 0) in vec3 in_position;

out vec3 position;

void main() {
	position = in_position;
	gl_Position = vec4(in_position, 1.0);
}
