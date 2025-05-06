#version 420 core

in vec3 position;

out vec4 fragment_color;

uniform float time;

void main() {
	fragment_color = vec4(0.0, 1 - pow(sin(15*position.x - 3*time) - 15*position.y, 2), 0.0, 1.0);
}
