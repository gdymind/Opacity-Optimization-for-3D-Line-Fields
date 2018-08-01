#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aDirection;
layout (location = 2) in vec2 aTexCoords;

uniform float stripWidth;
uniform mat4 modelViewProjectionMatrix;
uniform vec3 viewDirection;
uniform mat4 transform;

void main(void)
{
	vec3 d = (transform * vec4(aDirection, 1.0f)).xyz;
	vec3 offset = normalize(cross(d, viewDirection)) * (aTexCoords.y - 0.5f) * stripWidth;

	gl_Position = modelViewProjectionMatrix * (transform * vec4(aPos, 1.0f) + vec4(offset, 0.0f));
}