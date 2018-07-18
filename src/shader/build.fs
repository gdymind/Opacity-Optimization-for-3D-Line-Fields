#version 440 core

layout (early_fragment_tests) in;

layout (binding = 0, r32ui) uniform uimage2D headPointers;
layout (binding = 1, rgba32ui) uniform writeonly uimageBuffer listBuffer;

layout(binding = 0, offset = 0) uniform atomic_uint listCounter;

layout (location = 0) out vec4 FragColor;

in vec2 TexCoords;
in float weight;

void main(void)
{
	//set Fragment Color
	float c;
	if (abs(TexCoords.y - 0.5f) < 0.25f)//center
	{
		c = 0.0f;
	}
	else
	{
		c = 1.0f;
		gl_FragDepth = -gl_FragCoord.z - 0.001f * 2 * abs(TexCoords.y - 0.5);
	}
	FragColor = vec4(c, c, c, 1.0f);

	uint index; = atomicCounterIncrement(listCounter);

	uint oldHead = imageAtomicExchange(headPointers, ivec2(gl_FragCoord.xy), uint(index));

	//x: next pointer
	//y: depth
	//z: color
	//w: weight
	uvec4 node;
	node.x = oldHead;
	node.y = floatBitsToUint(gl_FragCoord.z);
	node.z = floatBitsToUint(c);
	node.w = weight;

	imageStore(listBuffer, int(index), node);

	//if(weight >=1 && weight <= 40) FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
	//if(gl_FragCoord.x< 450) FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);// divide gl_FragCoord.w?
	//if(windowSize.x == 900 && windowSize.y == 900) FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}