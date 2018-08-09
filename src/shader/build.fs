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
uniform float stripWidth;

//layout (location = 0) out vec4 FragColor;

in vec2 TexCoords;
in float weight;
in vec3 FragPos;
in vec3 T;

bool isCenter()
{
	return (abs(TexCoords.y - 0.5f) < 0.35f);
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
	    // vec3 V = viewDirection;
	    float LT = abs(dot(L, T));
	    float VT = abs(dot(V, T));

		// ambient
	    float ambient = 0.3;

	    // diffuse
	    float diffuse = sqrt(1 - LT * LT);

	    //specular
	    float specular = LT * VT - sqrt(1 - LT * LT) * sqrt(1 - VT * VT);
	    specular = abs(specular);
	    specular = pow(specular, 64);

	   	color = (ambient + 0.5 * diffuse) * lineColor + 0.7 * specular * lightColor;
	   	color = clamp(color, vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 1.0f, 1.0f));
	}
	else
		color = vec3(0.1f, 0.1f, 0.1f);


    return vec4(color, opa);
}

void main(void)
{
	if (isCenter()) gl_FragDepth = gl_FragCoord.z / gl_FragCoord.w;
	//else gl_FragDepth = gl_FragCoord.z / gl_FragCoord.w;
	else gl_FragDepth = (gl_FragCoord.z +  stripWidth * abs(TexCoords.y - 0.5)) / gl_FragCoord.w;

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