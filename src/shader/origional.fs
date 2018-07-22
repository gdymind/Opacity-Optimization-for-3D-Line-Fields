#version 460 core


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

	//if(weight >=1 && weight <= 40) FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
	//if(gl_FragCoord.x< 450) FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);// divide gl_FragCoord.w?
	//if(windowSize.x == 900 && windowSize.y == 900) FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}