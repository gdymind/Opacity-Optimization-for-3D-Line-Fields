#version 440 core

layout (binding = 0, r32ui) uniform uimage2D headPointers;
layout (binding = 1, rgba32ui) uniform writeonly uimageBuffer listBuffer;

layout (location = 0) out vec4 FragColor;

const int MAX_FRAGMENTS = 100;

uvec4 fragmentList[MAX_FRAGMENTS];

void main(void)
{
	int startIndex = headPointers[gl_FragCoord.x][gl_FragCoord.y];
}
