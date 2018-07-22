#version 460 core

layout (binding = 3, r32f) uniform imageBuffer opacityBuffer;

layout (location = 0) out vec4 FragColor;

in vec2 TexCoords;
in float weight;

void main(void)
{
	int segId = int(weight);
	float w = weight - segId;
	float opa1 = imageLoad(opacityBuffer, int(segId)).x;
	float opa2 = imageLoad(opacityBuffer, int(segId+1)).x;
	float opa = opa1 + (opa2 - opa1) * w;

	if (abs(TexCoords.y - 0.5f) < 0.25f)//center
	{
		gl_FragDepth = gl_FragCoord.z;
		FragColor = vec4(0.0f, 0.0f, 0.0f, opa);
	}
	else
	{
		gl_FragDepth = gl_FragCoord.z - 0.000001f  * abs(TexCoords.y - 0.5);
		FragColor = vec4(1.0f, 1.0f, 1.0f, opa);
	}
}