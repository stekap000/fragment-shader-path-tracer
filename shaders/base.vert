#version 420 core

layout (location = 0) in vec3 in_position;

out vec3 position;

uniform float width;
uniform float height;

void main() {
	position = in_position;

	// Adjust view based on the larger dimension. If width is larger, then it
	// dominates in what is viewed (larger horizontal view), otherwise height
	// dominates (larger vertical view).
	
	if(width > height) {
		position.x *= width/height;
	}
	else {
		position.y *= height/width;
	}
	
	gl_Position = vec4(in_position, 1.0);
}
