/*
struct VS_OUTPUT
{
	float4 pos: SV_POSITION;
	float4 color: COLOR;
	float2 xpos: TEXCOORD;
};

cbuffer ConstantBuffer : register(b0)
{
	float4 colorMultiplier;
	int iterations;

};

float4 main(VS_OUTPUT input) : SV_TARGET
{

	int iterations = iterations;
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

	//return(float4(
	//	.5,
	//	255,
	//	.5,
	//	input.color[3]));

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

*/

#define cx_one float2(1.0, 0.0)
float2 cx_mul(float2 a, float2 b) {
	return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}
float2 cx_sqr(float2 a) {
	float x2 = a.x * a.x;
	float y2 = a.y * a.y;
	float xy = a.x * a.y;
	return float2(x2 - y2, xy + xy);
}
float2 cx_cube(float2 a) {
	float x2 = a.x * a.x;
	float y2 = a.y * a.y;
	float d = x2 - y2;
	return float2(a.x * (d - y2 - y2), a.y * (x2 + x2 + d));
}
float2 cx_div(float2 a, float2 b) {
	float denom = 1.0 / (b.x * b.x + b.y * b.y);
	return float2(a.x * b.x + a.y * b.y, a.y * b.x - a.x * b.y) * denom;
}
float2 cx_sin(float2 a) {
	return float2(sin(a.x) * cosh(a.y), cos(a.x) * sinh(a.y));
}
float2 cx_cos(float2 a) {
	return float2(cos(a.x) * cosh(a.y), -sin(a.x) * sinh(a.y));
}
float2 cx_exp(float2 a) {
	return exp(a.x) * float2(cos(a.y), sin(a.y));
}

//Fractal equations
float2 mandelbrot(float2 z, float2 c) {
	return cx_sqr(z) + c;
}
float2 burning_ship(float2 z, float2 c) {
	return float2(z.x * z.x - z.y * z.y, 2.0 * abs(z.x * z.y)) + c;
}
float2 feather(float2 z, float2 c) {
	return cx_div(cx_cube(z), cx_one + z * z) + c;
}
float2 sfx(float2 z, float2 c) {
	return z * dot(z, z) - cx_mul(z, c * c);
}
float2 henon(float2 z, float2 c) {
	return float2(1.0 - c.x * z.x * z.x + z.y, c.y * z.x);
}
float2 duffing(float2 z, float2 c) {
	return float2(z.y, -c.y * z.x + c.x * z.y - z.y * z.y * z.y);
}
float2 ikeda(float2 z, float2 c) {
	float t = 0.4 - 6.0 / (1.0 + dot(z, z));
	float st = sin(t);
	float ct = cos(t);
	return float2(1.0 + c.x * (z.x * ct - z.y * st), c.y * (z.x * st + z.y * ct));
}
float2 chirikov(float2 z, float2 c) {
	z.y += c.y * sin(z.x);
	z.x += c.x * z.y;
	return z;
}
float2 drop(float2 z, float2 c)
{
	return cx_sqr(z) - cx_div(float2(0.0, 1.0), c);
}
float2 leaf(float2 z, float2 c)
{
	return cx_sqr(z) + cx_div(float2(1.0, 0.0), cx_sqr(c));
}
float biohazard(float2 z, float2 c)
{
	return cx_sin(z) + cx_exp(z) + c;
}

struct VS_OUTPUT
{
	float4 pos: SV_POSITION;
	float4 color: COLOR;
	float2 xpos: TEXCOORD;
};
cbuffer ConstantBuffer : register(b0)
{
	float4 colorMultiplier;
	float4 mousePos;
	int iterations;
	int cameraX;
	int cameraY;
};
float4 main(VS_OUTPUT input) : SV_TARGET
{
	int n = 0;
	//julia:

	float2 c = float2(mousePos.x, mousePos.y);
	float2 z = float2(
		(input.xpos[0] * colorMultiplier[2] + colorMultiplier[0]),
		(input.xpos[1] * colorMultiplier[2] + colorMultiplier[1]));
	//mandelbrot:
	//float2 c = float2(
	//	(input.xpos[0] * colorMultiplier[2] + colorMultiplier[0]),
	//	(input.xpos[1] * colorMultiplier[2] + colorMultiplier[1]));
	//float2 z = float2(0, 0);



	while ((z.x * z.x + z.y * z.y) < 4//measure current distance from root
		//&& n < iterations//give up
		)
	{
		//mandelbrot
		//re = zr * zr - zi * zi + cr;
		//im = zr * zi * 2.0 + ci;

		//burning ship
		//re = zr * zr - zi * zi + cr;
		//im = 2 * abs(zr * zi) + ci;

		//chirikov map
		//im = zi + ci * sin(zr);
		//re = zr + cr * im;

		//z = mandelbrot(z,c);
		
		
		z = mandelbrot(z,c);


		n++;
		if (n >= iterations)
			return(float4(0,0,0,1));
	}
	float a = 0.1;

	//0.5f * sin(a * n) + 0.5f, 0.5f * sin(a * n + 2.094f) + 0.5f, 0.5f * sin(a * n + 4.188f) + 0.5f)
	return(float4(
		sin(a * n) + 0.5,
		0.5 * sin(a * n + 2.094) + 0.5,
		0.5 * sin(a * n + 4.188) + 0.5,
		1));
	//return colorMultiplier;
	// return interpolated color
	//return float4(colorMultiplier.x*iterations, colorMultiplier.y, colorMultiplier.z, colorMultiplier.w);
}

