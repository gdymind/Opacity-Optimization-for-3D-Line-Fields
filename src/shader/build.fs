#version 440 core

//layout (early_fragment_tests) in;

layout (binding = 0, r32ui) uniform uimage2D headPointers;
// layout (binding = 1, rgba32ui) uniform coherent uimageBuffer listBuffer;

layout(binding = 0, offset = 0) uniform atomic_uint listCounter;

layout (location = 0) out vec4 FragColor;

in vec2 TexCoords;
in float weight;

void main(void)
{
	//set Fragment Color
	gl_FragDepth = gl_FragCoord.z;
	float c;
	if (abs(TexCoords.y - 0.5f) < 0.25f)//center
	{
		c = 0.0f;
	}
	else
	{
		c = 1.0f;
		gl_FragDepth = gl_FragCoord.z - 0.000001f  * abs(TexCoords.y - 0.5);
	}
	FragColor = vec4(c, c, c, 1.0f);
	//FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);

	uint index = atomicCounterIncrement(listCounter);
	uint oldHead = imageAtomicExchange(headPointers, ivec2(gl_FragCoord.xy), index);

	// if(gl_FragCoord.x < 100) FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);

	// for(uint i = 0; i < 200; ++i)
	// {
	// 	for(uint j = 0; j < 200; ++j)
	// 	{
	// 		if(imageAtomicExchange(headPointers, ivec2(i, j), uint(0)) > 0u)
	// 			atomicCounterIncrement(listCounter);
	// 	}
	// }

	// uint oldHead = imageAtomicExchange(headPointers, ivec2(0, 0), 10u);
	// if (imageAtomicExchange(headPointers, ivec2(0, 0), 10u) > 0u)
	// 	FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);

	// x,y,z,2: next pointer, depth, color, weight
	uvec4 node;
	node.x = oldHead;
	node.y = floatBitsToUint(gl_FragDepth);
	node.z = floatBitsToUint(c);
	node.w = floatBitsToUint(weight);

	imageStore(listBuffer, int(index), node);

	//if(weight >=1 && weight <= 40) FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
	//if(gl_FragCoord.x< 450) FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);// divide gl_FragCoord.w?
	//if(windowSize.x == 900 && windowSize.y == 900) FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}