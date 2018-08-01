#version 460 core

layout (binding = 0, r32ui) uniform uimage2D headPointers;
layout (binding = 1, rgba32ui) uniform uimageBuffer listBuffer;
layout (binding = 2, r32f) uniform imageBuffer opacityBuffer;

layout(binding = 0, offset = 0) uniform atomic_uint listCounter;
layout(binding = 0, offset = 4) uniform atomic_uint debugOut;

uniform int segmentNum;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 lineColor;

//layout (location = 0) out vec4 FragColor;

in vec2 TexCoords;
in float weight;
in vec3 FragPos;
in vec3 T;

bool isCenter()
{
	return (abs(TexCoords.y - 0.5f) < 0.4f);
}

vec4 setColor()
{
	//---------------------
	//opacity
	int segId = int(weight);
	float opa1 = imageLoad(opacityBuffer, int(segId)).x;
	float opa2 = imageLoad(opacityBuffer, int(segId+1)).x;
	float opa = mix(opa1, opa2, fract(weight));

	vec3 color;

	if(isCenter())
	{
	    vec3 L = normalize(lightPos - FragPos);
	    vec3 V = normalize(-FragPos);
	    //vec3 V = viewDirection;
	    float LT = abs(dot(L, T));
	    float VT = abs(dot(V, T));

		// ambient
	    vec3 ambient = 0.2 * lightColor;

	    // diffuse
	    float diff = sqrt(1 - LT * LT);
	    vec3 diffuse = diff * lightColor;

	    //specular
	    float spec = LT * VT - sqrt(1 - LT * LT) * sqrt(1 - VT * VT);
	    spec = abs(spec);
	    spec = pow(spec, 20);
	    vec3 specular = spec * lightColor;

	   	color = (ambient + diffuse) * lineColor + 0.8 * specular;
		//color = lineColor;
	}
	else
		color = vec3(0.0f, 0.0f, 0.0f);


    return vec4(color, opa);
}

void main(void)
{
	if (isCenter())
		gl_FragDepth = gl_FragCoord.z;
	else
		gl_FragDepth = gl_FragCoord.z - 0.0000001f  * abs(TexCoords.y - 0.5);

	uint index = atomicCounterIncrement(listCounter);
	uint oldHead = imageAtomicExchange(headPointers, ivec2(gl_FragCoord.xy), index);

	// x,y,z,w: next pointer, depth, weight, color
	uvec4 node;
	node.x = oldHead;
	node.y = floatBitsToUint(gl_FragDepth);
	node.z = floatBitsToUint(weight);
	node.w = packUnorm4x8(setColor());

	imageStore(listBuffer, int(index), node);
}