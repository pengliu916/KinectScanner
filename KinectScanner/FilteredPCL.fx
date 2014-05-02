#include "header.h"
Texture2D<int>    txDepth  : register(t0);
Texture2D<float4> txColor  : register(t1);

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct GS_INPUT
{
};

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
GS_INPUT VS( )
{
	GS_INPUT output = (GS_INPUT)0;
 
	return output;
}

//--------------------------------------------------------------------------------------
// Geometry Shader for every filter
//--------------------------------------------------------------------------------------
[maxvertexcount(4)]
void GS(point GS_INPUT particles[1], inout TriangleStream<PS_INPUT> triStream)
{
	PS_INPUT output;
	output.Pos=float4(-1.0f,1.0f,0.0f,1.0f);
	output.Tex=float2(0.0f,0.0f);
	triStream.Append(output);

	output.Pos=float4(-1.0f,-1.0f,0.0f,1.0f);
	output.Tex=float2(0.0f,DEPTH_HEIGHT);
	triStream.Append(output);

	output.Pos=float4(1.0f,1.0f,0.0f,1.0f);
	output.Tex=float2(DEPTH_WIDTH,0.0f);
	triStream.Append(output);

	output.Pos=float4(1.0f,-1.0f,0.0f,1.0f);
	output.Tex=float2(DEPTH_WIDTH,DEPTH_HEIGHT);
	triStream.Append(output);
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	int3 coord = int3( input.Tex, 0 );
	int depth = txDepth.Load(coord);
	float4 color = txColor.Load(coord);

	// remove player index information and convert to meters
	float realDepth = depth / 8000.0;
	if ( realDepth > 4.0f || realDepth < 0.3 ) return float4 ( color.xyz, -1 );
	float4 output = float4(color.xyz,realDepth);
	return output;
}