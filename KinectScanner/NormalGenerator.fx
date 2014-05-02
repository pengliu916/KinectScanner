#include "header.h"
Texture2D<float4>    txRGBZ  : register(t0);

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

static const float2 DepthWidthHeight = float2(DEPTH_WIDTH, DEPTH_HEIGHT);
static const float2 DepthHalfWidthHeight = DepthWidthHeight / 2.0;
static const float2 DepthHalfWidthHeightOffset = DepthHalfWidthHeight-0.5;
static const float2 ColorWidthHeight = float2(DEPTH_WIDTH, DEPTH_HEIGHT);
static const float4 XYScale=float4(XSCALE,-XSCALE,0,0);
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
	 PS_INPUT output;

    // use the minimum of near mode and standard
    static const int minDepth = 300 << 3;

    // use the maximum of near mode and standard
    static const int maxDepth = 4000 << 3;

    // texture load location for the pixel we're on 
	float4 point0 =  txRGBZ.Load(int3(input.Tex,0),0);
    if (point0.a*8000 < minDepth || point0.a*8000 > maxDepth)
       return float4(0,0,0,-1);

	float4 point1 =  txRGBZ.Load(int3(input.Tex,0),int2(1,0));
    if (point1.a*8000 < minDepth || point1.a*8000 > maxDepth)
       return float4(0,0,0,-1);

	float4 point2 =  txRGBZ.Load(int3(input.Tex,0),int2(0,1));
    if (point2.a*8000 < minDepth || point2.a*8000 > maxDepth)
       return float4(0,0,0,-1);
   
    // convert x and y lookup coords to world space meters
    point0.xy = (input.Tex - DepthHalfWidthHeightOffset) * XYScale.xy * point0.a;
    point0.z = point0.a;
    point0.w = 1.0;
	point1.xy = (input.Tex + int2(1,0) - DepthHalfWidthHeightOffset) * XYScale.xy * point1.a;
    point1.z = point1.a;
    point1.w = 1.0;
	point2.xy = (input.Tex + int2(0,1) - DepthHalfWidthHeightOffset) * XYScale.xy * point2.a;
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
