//--------------------------------------------------------------------------------------
// GPU algorithm for Fast-ICP ( Point-to Plane Minimization )
// see http://www.cs.princeton.edu/~smr/papers/icpstability.pdf for more detail
//--------------------------------------------------------------------------------------
#include "header.h"

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct InterData
{
	float4 f4DataElm0;//cxcx,cxcy,cxcz,cxnx
	float4 f4DataElm1;//cxny,cxnz,cycy,cycz
	float4 f4DataElm2;//cynx,cyny,cynz,czcz
	float4 f4DataElm3;//cznx,czny,cznz,nxnx
	float4 f4DataElm4;//nxny,nxnz,nyny,nynz
	float4 f4DataElm5;//nznz,cxpqn,cypqn,czpqn
	float4 f4DataElm6;//nxpqn,nypqn,nzpqn,NumOfPair
};

RWStructuredBuffer<InterData> g_bufOut : register(u0);
Texture2D<float4> txRGBZ_kinect  : register(t0);
Texture2D<float4> txRGBZ_tsdf  : register(t1);
Texture2D<float4> txNorm_kinect	: register(t2);
Texture2D<float4> txNorm_tsdf	: register(t3);

cbuffer cbMeshTransform :register( b0 ){
	// the following two matrix is identical
	matrix cb_mKinect;
	uint2 cb_u2UAVDim;
	uint cb_uStepLen;
	uint NIU;
};
//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
static const float2 reso = float2(D_W, D_H);
static const float2 range = float2(R_N, R_F);
static const float2 f = float2(F_X, -F_Y);
static const float2 c = float2(C_X, C_Y);
//--------------------------------------------------------------------------------------
// Compute Shader
//--------------------------------------------------------------------------------------
[numthreads(THREAD2D_X, THREAD2D_Y, 1)]
void CS(uint3 DTid:SV_DispatchThreadID)
{
	InterData output;
	// create default output value for invalid read
	output.f4DataElm0 = float4(0.f,0.f,0.f,0.f);
	output.f4DataElm1 = float4(1.f,0.f,0.f,0.f);
	output.f4DataElm2 = float4(2.f,0.f,0.f,0.f);
	output.f4DataElm3 = float4(3.f,0.f,0.f,0.f);
	output.f4DataElm4 = float4(4.f,0.f,0.f,0.f);
	output.f4DataElm5 = float4(5.f,0.f,0.f,0.f);
	output.f4DataElm6 = float4(6.f,0.f,0.f,0.f);
	g_bufOut[DTid.x + DTid.y * cb_u2UAVDim.x] = output; return;
	// read normal from TSDF and return default output if normal from TSDF is invalid
	float4 tsdfNormalmap = txNorm_tsdf.Load ( DTid * cb_uStepLen );
	if ( tsdfNormalmap.w < 0.f ){ g_bufOut[DTid.x + DTid.y * cb_u2UAVDim.x] = output; return;}
	float3 tsdfNormal = tsdfNormalmap.xyz * 2.0f - 1.0f;

	// read normal from Kinect and return default output if normal from Kinect is invalid
	float4 kinectNormalmap = txNorm_kinect.Load(DTid* cb_uStepLen);
	if ( kinectNormalmap.w < 0.f ){ g_bufOut[DTid.x + DTid.y * cb_u2UAVDim.x] = output; return;}
	float3 kinectNormal = kinectNormalmap.xyz * 2.0f - 1.0f;

	// return default output if normal from TSDF and Kinect has angle larger than the threashold
	float sine = length(cross(tsdfNormal,kinectNormal));
	if(sine >= ANGLE_THRES){ g_bufOut[DTid.x + DTid.y * cb_u2UAVDim.x] = output; return;}

	// read depth from kinect and return default output if depth is out of range
	float3 kinectPoint;
	kinectPoint.z = txRGBZ_kinect.Load(DTid* cb_uStepLen).w;
	if ( kinectPoint.z < range.x || kinectPoint.z >range.y ){ g_bufOut[DTid.x + DTid.y * cb_u2UAVDim.x] = output; return;}
	kinectPoint.xy = (DTid.xy* cb_uStepLen - c) / f * kinectPoint.z;
	
	// read depth from TSDF and return default output if depth is out of range
	float3 tsdfPoint;
	tsdfPoint.z = txRGBZ_tsdf.Load(DTid* cb_uStepLen).w;
	if ( tsdfPoint.z < range.x || tsdfPoint.z > range.y ){ g_bufOut[DTid.x + DTid.y * cb_u2UAVDim.x] = output; return;}
	tsdfPoint.xy = (DTid.xy* cb_uStepLen - c) / f * tsdfPoint.z;

	// compute the distance of point pair and reject those with greater value than the threashold
	float dist = length(tsdfPoint-kinectPoint);
	if(dist>DIST_THRES) { g_bufOut[DTid.x + DTid.y * cb_u2UAVDim.x] = output; return;}

	// transform both point into worldframe is not necessary, but experiment result says this one is more robust
	tsdfNormal = mul ( float4 ( tsdfNormal, 0.f ), cb_mKinect ).xyz;
	kinectPoint = mul ( float4 ( kinectPoint, 1.f ), cb_mKinect ).xyz;
	tsdfPoint = mul ( float4 ( tsdfPoint, 1.f ), cb_mKinect ).xyz;

	// see http://www.cs.princeton.edu/~smr/papers/icpstability.pdf
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
	
	float pqn = dot(kinectPoint - tsdfPoint, tsdfNormal);
	float3 cpqn = c*pqn;
	float3 npqn = tsdfNormal*pqn;

	output.f4DataElm0 = float4(cxc,cxn.x);
	output.f4DataElm1 = float4(cxn.yz,cyc.yz);
	output.f4DataElm2 = float4(cyn,czc.z);
	output.f4DataElm3 = float4(czn,nxn.x);
	output.f4DataElm4 = float4(nxn.yz,nyn.yz);
	output.f4DataElm5 = float4(nzn.z,cpqn);
	output.f4DataElm6 = float4(npqn,1);

	g_bufOut[DTid.x + DTid.y * cb_u2UAVDim.x] = output;

	return;
}