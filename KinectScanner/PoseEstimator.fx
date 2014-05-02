#include "header.h"

Texture2D<float4> txRGBZ_kinect  : register(t0);
Texture2D<float4> txRGBZ_tsdf  : register(t1);
Texture2D<float4> txNorm_kinect	: register(t2);
Texture2D<float4> txNorm_tsdf	: register(t3);

cbuffer cbRTResolution :register( b0 )
{
	int4 compressed;//xy txRT width height
};
cbuffer cbMeshTransform :register( b1 ){
	matrix Model_kinect;
	matrix Model_tsdf;
};
//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
static const float2 DEPTH_WIDTHHeight = float2(DEPTH_WIDTH, DEPTH_HEIGHT);
static const float2 DepthHalfWidthHeight = DEPTH_WIDTHHeight / 2.0;
static const float2 DepthHalfWidthHeightOffset = DepthHalfWidthHeight - 0.5;
static const float4 XYScale=float4(XSCALE,-XSCALE,0,0);

static const float ANGLE_THRES = sin (20.f * 3.14159254f / 180.f);
static const float DIST_THRES = 0.1f;
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

struct PS_OUT_I
{
	float4 dataTex1 : SV_TARGET0;//cxcx,cxcy,cxcz,cxnx
	float4 dataTex2 : SV_TARGET1;//cxny,cxnz,cycy,cycz
	float4 dataTex3 : SV_TARGET2;//cynx,cyny,cynz,czcz
	float4 dataTex4 : SV_TARGET3;//cznx,czny,cznz,nxnx
};
struct PS_OUT_II
{
	float4 dataTex5 : SV_TARGET0;//nxny,nxnz,nyny,nynz
	float4 dataTex6 : SV_TARGET1;//nznz,cxpqn,cypqn,czpqn
	float4 dataTex7 : SV_TARGET2;//nxpqn,nypqn,nzpqn,NumOfPair
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
// Geometry Shader 
//--------------------------------------------------------------------------------------
[maxvertexcount(4)]
void GS(point GS_INPUT particles[1], inout TriangleStream<PS_INPUT> triStream)
{
	PS_INPUT output;
	output.Pos=float4(-1.0f,1.0f,0.0f,1.0f);
	output.Tex=float2(0.0f,0.0f);
	triStream.Append(output);

	output.Pos=float4(-1.0f,-1.0f,0.0f,1.0f);
	output.Tex=float2(0.0f,compressed.y);
	triStream.Append(output);

	output.Pos=float4(1.0f,1.0f,0.0f,1.0f);
	output.Tex=float2(compressed.x,0.0f);
	triStream.Append(output);

	output.Pos=float4(1.0f,-1.0f,0.0f,1.0f);
	output.Tex=float2(compressed.x,compressed.y);
	triStream.Append(output);
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

PS_OUT_I PS_I(PS_INPUT input) : SV_Target
{
	PS_OUT_I output;

	output.dataTex1 = float4(0,0,0,0);
	output.dataTex2 = float4(0,0,0,0);
	output.dataTex3 = float4(0,0,0,0);
	output.dataTex4 = float4(0,0,0,0);

	int3 currentLocation = int3(input.Tex.xy,0);
	
	float4 tsdfNormalmap = txNorm_tsdf.Load ( currentLocation );
	if ( tsdfNormalmap.w < 0 ) return output;
	float3 tsdfNormal = tsdfNormalmap.xyz * 2.0 - 1.0;

	float4 kinectNormalmap = txNorm_kinect.Load(currentLocation);
	if ( kinectNormalmap.w < 0 ) return output;
	float3 kinectNormal = kinectNormalmap.xyz * 2.0 - 1.0;
	float sine = length(cross(tsdfNormal,kinectNormal));
	if(sine >= ANGLE_THRES) return output;

	float3 kinectPoint;
	kinectPoint.z = txRGBZ_kinect.Load(currentLocation).w;
	if ( abs (kinectPoint.z + 1) <= EPSILON ) return output;
	kinectPoint.xy = (input.Tex.xy - DepthHalfWidthHeightOffset) * XYScale.xy * kinectPoint.z;
	
	float3 tsdfPoint;
	tsdfPoint.z = txRGBZ_tsdf.Load(currentLocation).w;
	if ( abs ( tsdfPoint.z + 1) <= EPSILON ) return output;
	tsdfPoint.xy = (input.Tex.xy - DepthHalfWidthHeightOffset) * XYScale.xy * tsdfPoint.z;

	float dist = length(tsdfPoint-kinectPoint);
	if(dist>DIST_THRES) return output;


	tsdfNormal = mul ( float4 ( tsdfNormal, 0 ), Model_tsdf ).xyz;
	kinectPoint = mul ( float4 ( kinectPoint, 1 ), Model_kinect ).xyz;

	float3 c = cross(kinectPoint,tsdfNormal);

	float3 cxc = c.x * c;
	float3 cyc = c.y * c;
	float3 czc = c.z * c;

	float3 cxn = c.x * tsdfNormal;
	float3 cyn = c.y * tsdfNormal;
	float3 czn = c.z * tsdfNormal;

	float3 nxn = tsdfNormal.x * tsdfNormal;
	float3 nyn = tsdfNormal.y * tsdfNormal;
	float3 nzn = tsdfNormal.z * tsdfNormal;
	
	output.dataTex1 = float4(cxc,cxn.x);
	output.dataTex2 = float4(cxn.yz,cyc.yz);
	output.dataTex3 = float4(cyn,czc.z);
	output.dataTex4 = float4(czn,nxn.x);

	return output;
}

PS_OUT_II PS_II(PS_INPUT input) : SV_Target
{
	PS_OUT_II output;

	output.dataTex5 = float4(0,0,0,0);
	output.dataTex6 = float4(0,0,0,0);
	output.dataTex7 = float4(0,0,0,0);

	int3 currentLocation = int3(input.Tex.xy,0);

	float4 tsdfNormalmap = txNorm_tsdf.Load ( currentLocation );
	if ( tsdfNormalmap.w < 0 ) return output;
	float3 tsdfNormal = tsdfNormalmap.xyz * 2.0 - 1.0;

	float4 kinectNormalmap = txNorm_kinect.Load(currentLocation);
	if ( kinectNormalmap.w < 0 ) return output;
	float3 kinectNormal = kinectNormalmap.xyz * 2.0 - 1.0;
	float sine = length(cross(tsdfNormal,kinectNormal));
	if(sine >= ANGLE_THRES) return output;

	float3 kinectPoint;
	kinectPoint.z = txRGBZ_kinect.Load(currentLocation).w;
	if ( abs (kinectPoint.z + 1) <= EPSILON ) return output;
	kinectPoint.xy = (input.Tex.xy - DepthHalfWidthHeightOffset) * XYScale.xy * kinectPoint.z;
	
	float3 tsdfPoint;
	tsdfPoint.z = txRGBZ_tsdf.Load(currentLocation).w;
	if ( abs ( tsdfPoint.z + 1) <= EPSILON ) return output;
	tsdfPoint.xy = (input.Tex.xy - DepthHalfWidthHeightOffset) * XYScale.xy * tsdfPoint.z;
	
	float dist = length(tsdfPoint-kinectPoint);
	if(dist>DIST_THRES) return output;

	tsdfNormal = mul ( float4 ( tsdfNormal, 0 ), Model_tsdf ).xyz;
	kinectPoint = mul ( float4 ( kinectPoint, 1 ), Model_kinect ).xyz;
	tsdfPoint = mul ( float4 ( tsdfPoint, 1 ), Model_tsdf ).xyz;
	
	float3 c = cross(kinectPoint,tsdfNormal);

	float3 cxc = c.x * c;
	float3 cyc = c.y * c;
	float3 czc = c.z * c;

	float3 cxn = c.x * tsdfNormal;
	float3 cyn = c.y * tsdfNormal;
	float3 czn = c.z * tsdfNormal;

	float3 nxn = tsdfNormal.x * tsdfNormal;
	float3 nyn = tsdfNormal.y * tsdfNormal;
	float3 nzn = tsdfNormal.z * tsdfNormal;

	float3 cpqn = c*dot( kinectPoint - tsdfPoint, tsdfNormal);
	float3 npqn = tsdfNormal*dot( kinectPoint- tsdfPoint,tsdfNormal);

	output.dataTex5 = float4(nxn.yz,nyn.yz);
	output.dataTex6 = float4(nzn.z,cpqn);
	output.dataTex7 = float4(npqn,1);
	return output;
}