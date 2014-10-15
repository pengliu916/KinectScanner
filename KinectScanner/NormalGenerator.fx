#include "header.h"
Texture2D<float4>    txRGBZ  : register(t0);

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

static const float2 reso = float2(D_W, D_H);
static const float2 range = float2(R_N,R_F);
static const float2 f = float2(F_X,-F_Y);
static const float2 c = float2(C_X, C_Y);
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
// Vertex Shader for every filter
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

	output.Pos=float4(1.0f,1.0f,0.0f,1.0f);
	output.Tex=float2(reso.x,0.0f);
	triStream.Append(output);

	output.Pos=float4(-1.0f,-1.0f,0.0f,1.0f);
	output.Tex=float2(0.0f,reso.y);
	triStream.Append(output);

	output.Pos=float4(1.0f,-1.0f,0.0f,1.0f);
	output.Tex=reso;
	triStream.Append(output);
}

//--------------------------------------------------------------------------------------
// Pixel Shader 
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	 PS_INPUT output;

    // texture load location for the pixel we're on 
	float4 data0 =  txRGBZ.Load(int3(input.Tex,0),0);
    if (data0.a < range.x || data0.a > range.y)
       return float4(0,0,0,-1);

	float4 data1 =  txRGBZ.Load(int3(input.Tex,0),int2(1,0));
    if (data1.a < range.x || data1.a > range.y)
       return float4(0,0,0,-1);

	float4 data2 =  txRGBZ.Load(int3(input.Tex,0),int2(0,1));
    if (data2.a < range.x|| data2.a > range.y)
       return float4(0,0,0,-1);
   
    // convert x and y lookup coords to world space meters
	float4 point0 = float4((input.Tex - c) / f * data0.a, data0.a, 1.f);
	float4 point1 = float4((input.Tex + float2(1,0) - c) / f * data1.a, data1.a, 1.f);
	float4 point2 = float4((input.Tex + float2(0,1) - c) / f * data2.a, data2.a, 1.f);
   

	float3 u = point1.xyz-point0.xyz;
	float3 v = point2.xyz-point0.xyz;

	float3 normal = cross(u,v);
	/*float3 normal;
	normal.x = u.y*v.z-u.z*v.y;
	normal.y = u.z*v.x-u.x*v.z;
	normal.z = u.x*v.y-u.y*v.x;*/

	return float4(normalize(normal),0)*0.5+float4(0.5,0.5,0.5,0);
}
