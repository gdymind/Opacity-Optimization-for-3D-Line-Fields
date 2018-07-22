#version 460 core

//layout (early_fragment_tests) in;

layout (binding = 0, r32ui) uniform uimage2D headPointers;
layout (binding = 1, rgba32ui) uniform uimageBuffer listBuffer;
layout (binding = 2, r32ui) uniform uimage2D visit;
// layout (binding = 3, r32ui) uniform uimage2D H;


layout(binding = 0, offset = 0) uniform atomic_uint listCounter;
layout(binding = 0, offset = 4) uniform atomic_uint debugOut;

uniform int segmentNum;

layout (location = 0) out vec4 FragColor;

in vec2 TexCoords;
in float weight;

void main(void)
{

	if (abs(TexCoords.y - 0.5f) < 0.25f)//center
		gl_FragDepth = gl_FragCoord.z;
	else
		gl_FragDepth = gl_FragCoord.z - 0.000001f  * abs(TexCoords.y - 0.5);

	uint index = atomicCounterIncrement(listCounter);
	uint oldHead = imageAtomicExchange(headPointers, ivec2(gl_FragCoord.xy), index);

	// x,y,z,w: next pointer, depth, weight, reserved
	uvec4 node;
	node.x = oldHead;
	node.y = floatBitsToUint(gl_FragDepth);
	node.z = floatBitsToUint(weight);
	node.w = 0u;
	//node.w = floatBitsToUint(c);

	imageStore(listBuffer, int(index), node);
}