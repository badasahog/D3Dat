struct VS_OUTPUT
{
	float4 pos: SV_POSITION;
	float4 color: COLOR;
	float2 xpos: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{

	int iterations = 1000;
	int n = 0;

	float cr = input.xpos[0];
	float ci = input.xpos[1];
	float zr = 0;
	float zi = 0;
	float re = 0;
	float im = 0;
	n = 0;
	while ((zr * zr + zi * zi) < 4.0 && n < iterations)
	{
		re = zr * zr - zi * zi + cr;
		im = zr * zi * 2.0 + ci;
		zr = re;
		zi = im;

		n++;
	}
	float a = 0.1;
	//0.5f * sin(a * n) + 0.5f, 0.5f * sin(a * n + 2.094f) + 0.5f, 0.5f * sin(a * n + 4.188f) + 0.5f)
	return(float4(
		sin(a * n) + 0.5,
		0.5 * sin(a * n + 2.094) + 0.5,
		0.5 * sin(a * n + 4.188) + 0.5,
		input.color[3]));


	//return(
	//	float4(
			//fmod(sqrt(pow(input.xpos[0], 2) + pow(input.xpos[1], 2)), .4),
			//fmod(sqrt(pow(input.xpos[0], 2) + pow(input.xpos[1], 2)), .5),
			//fmod(sqrt(pow(input.xpos[0], 2) + pow(input.xpos[1], 2)), .6),
			//input.xpos[1],
			//input.xpos[1],
			//input.xpos[1],
			//input.color[0],
			//input.color[1],
			//input.color[2],
			//input.color[3]
	//	)
	//);


}