#version 460 core

layout (binding = 0, r32ui) uniform uimage2D headPointers;
layout (binding = 1, rgba32ui) uniform uimageBuffer listBuffer;
layout (binding = 2, r32ui) uniform uimage2D visit;
layout (binding = 3, r32f) uniform imageBuffer opacityBuffer;

layout(binding = 0, offset = 0) uniform atomic_uint listCounter;
layout(binding = 0, offset = 4) uniform atomic_uint debugOut;

in vec4 color;

layout (location = 0) out vec4 FragColor;

void main(void)
{
	//node list
	const int MAX_NODES_NUM = 800;
	uvec4 nodeList[MAX_NODES_NUM];

	uint vis = imageAtomicExchange(visit, ivec2(gl_FragCoord.xy), uint(0));
	if(vis != 0)
	{
		//collect nodes of this pixel
		//---------------------------
		uint cnt = 0;//the number of frgments in this pixel
		uint preIndex = 0;
		uint curIndex = imageLoad(headPointers, ivec2(gl_FragCoord.xy)).x;
		while(curIndex != 0 && cnt < MAX_NODES_NUM)
		{
			// x,y,z,w: next pointer, depth, weight, reserved
			uvec4 node = imageLoad(listBuffer, int(curIndex));
			node.z = curIndex;
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

					float depth1 = uintBitsToFloat(node1.z);
					float depth2 = uintBitsToFloat(node2.z);

					if(depth1 < depth2)
					{
						nodeList[i] = node2;
						nodeList[j] = node1;
					}
				}
			}

			//relink the list in listBuffer
			for(uint i = 0; i < cnt - 1; ++i)
			{
				uint curIndex = nodeList[i].z;
				uint nxtIndex = nodeList[i+1].z;
				uvec4 node = imageLoad(listBuffer, int(curIndex));
				node.x = nxtIndex;
				imageStore(listBuffer, int(curIndex), node);
			}
		}


	}

	FragColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);
}
