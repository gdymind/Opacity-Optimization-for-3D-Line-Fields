#version 460 core

layout (early_fragment_tests) in;

layout (binding = 0, r32ui) uniform uimage2D headPointers;
layout (binding = 1, rgba32ui) uniform uimageBuffer listBuffer;

layout(binding = 0, offset = 4) uniform atomic_uint debugOut;

out vec4 FragColor;

void main(void)
{
	//node list
	const int MAX_NODES_NUM = 800;
	uvec4 nodeList[MAX_NODES_NUM];

	//collect nodes of this pixel
	//---------------------------
	uint cnt = 0;//the number of fragments in this pixel
	uint preIndex = 0;
	uint curIndex = imageLoad(headPointers, ivec2(gl_FragCoord.xy)).x;
	while(curIndex != 0 && cnt < MAX_NODES_NUM)
	{
		// x,y,z,w: next pointer, depth, weight, color
		uvec4 node = imageLoad(listBuffer, int(curIndex));
		nodeList[cnt] = node;
		++cnt;
		curIndex = node.x;//node.x is the next pointer
	}

	//sort nodeList
	if(cnt > 1)
	{
		for(uint i = 0; i < cnt - 1; ++i)
		{
			for(uint j = i + 1; j < cnt; ++j)
			{
				uvec4 node1 = nodeList[i];
				uvec4 node2 = nodeList[j];

				float depth1 = uintBitsToFloat(node1.y);
				float depth2 = uintBitsToFloat(node2.y);

				if(depth1 < depth2)
				{
					nodeList[i] = node2;
					nodeList[j] = node1;
				}
			}
		}
	}


	vec4 finalColor = vec4(1.0);

	for(uint i = 0; i < cnt; ++i)
	{
		vec4 fragColor = unpackUnorm4x8(nodeList[i].w);
		finalColor = mix(finalColor, fragColor ,fragColor.a);
	}

	FragColor = finalColor;
}