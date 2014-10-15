#include "header.h"
Texture2D<float4>    inputTex  : register(t0);

static const float filter[7] = { 0.030078323,
					0.104983664, 0.222250419,
					0.285375187, 0.222250419,
					0.104983664, 0.030078323};



static const float2 reso = float2(D_W, D_H);
static const float2 range = float2(R_N, R_F);
static const float2 f = float2(F_X, -F_Y);
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
// Pixel Shader just half the distance (test purpose)
//--------------------------------------------------------------------------------------
float4 PS_Bilateral_Filter_V(PS_INPUT input) : SV_Target
{
	int3 currentLocation = int3(input.Tex.xy,0);
	float4 centerColor = inputTex.Load(currentLocation);

	const float rsigma = 0.051f;
	float4 Color = 0.0f;
	float4 Weight = 0.0f;

	[unroll]
	for(int i = -3; i <= 3; ++i)
	{
		float4 sampleColor = inputTex.Load(currentLocation,int2(0,i));
		float4 delta = centerColor - sampleColor;
		float4 range = exp((-1.0f*delta*delta)/(2.0f*rsigma*rsigma));
		Color += sampleColor*range*filter[i+3];
		Weight += range*filter[i+3];
	}
	return Color/Weight;
}

float4 PS_Bilateral_Filter_H(PS_INPUT input) : SV_Target
{
	//int3 currentLocation = int3(input.Tex.xy,0);
	//// Mirror the input data
	////currentLocation.x = reso.x - currentLocation.x;
	//float4 centerColor = inputTex.Load(currentLocation);

	//const float rsigma = 0.051f;
	//float4 Color = 0.0f;
	//float4 Weight = 0.0f;

	//[unroll]
	//for(int i = -3; i <= 3; ++i)
	//{
	//	float4 sampleColor = inputTex.Load(currentLocation,int2(i,0));
	//	float4 delta = centerColor - sampleColor;
	//	float4 range = exp((-1.0f*delta*delta)/(2.0f*rsigma*rsigma));
	//	Color += sampleColor*range*filter[i+3];
	//	Weight += range*filter[i+3];
	//}
	//return Color/Weight;

	int2 location = input.Tex.xy - reso / 2.f;
	if( dot(location,location) < 2000 ) return float4(1,1,1,0.8);
	else return float4(1,1,1,1.5);
}