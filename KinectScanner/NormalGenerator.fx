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
	float4 point0 =  txRGBZ.Load(int3(input.Tex,0),0);
    if (point0.a < range.x || point0.a > range.y)
       return float4(0,0,0,-1);

	float4 point1 =  txRGBZ.Load(int3(input.Tex,0),int2(1,0));
    if (point1.a < range.x || point1.a > range.y)
       return float4(0,0,0,-1);

	float4 point2 =  txRGBZ.Load(int3(input.Tex,0),int2(0,1));
    if (point2.a < range.x|| point2.a > range.y)
       return float4(0,0,0,-1);
   
    // convert x and y lookup coords to world space meters
    point0.xy = (input.Tex - c) / f * point0.a;
    point0.z = point0.a;
    point0.w = 1.0;
	point1.xy = (input.Tex + int2(1,0) - c) / f * point1.a;
    point1.z = point1.a;
    point1.w = 1.0;
	point2.xy = (input.Tex + int2(0,1) - c) / f * point2.a;
    point2.z = point2.a;
    point2.w = 1.0;

	float3 u = point1.xyz-point0.xyz;
	float3 v = point2.xyz-point0.xyz;

	float3 normal = cross(u,v);
	/*float3 normal;
	normal.x = u.y*v.z-u.z*v.y;
	normal.y = u.z*v.x-u.x*v.z;
	normal.z = u.x*v.y-u.y*v.x;*/

	return float4(normalize(normal),0)*0.5+float4(0.5,0.5,0.5,0);
}
