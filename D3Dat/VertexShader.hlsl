struct VS_INPUT
{
	float3 pos : POSITION;
	float4 color: COLOR;
};

struct VS_OUTPUT
{
	float4 pos: SV_POSITION;
	float4 color: COLOR;
	float2 xpos: TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.pos = float4(input.pos, 1.0f);
	output.color = input.color;
	output.xpos = float2(input.pos[0], input.pos[1]);
	return output;
}