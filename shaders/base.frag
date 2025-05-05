#version 430 core

in vec2 texture_coords;

out vec4 fragment_color;

layout (binding = 0) uniform sampler2D sampler;

void main() {
	vec3 texel_color = texture(sampler, texture_coords).rgb;
	fragment_color = vec4(texel_color, 1.0);
}
