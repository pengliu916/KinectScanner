#include "header.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
Texture3D<float2> g_srvDepthWeight : register(t0);
Texture3D<float4> g_srvColor : register(t1);

// for DirectCompte output
RWTexture2D<float4> g_uavKinectRGBD : register(u0);
RWTexture2D<float4> g_uavKinectNorm : register(u1);
RWTexture2D<float4> g_uavKinectShade : register(u2);
RWTexture2D<float4> g_uavFreeCamShade : register(u3);
RWTexture2D<float4> g_uavRayCast : register(u4);

SamplerState g_samLinear : register(s0);

// Phong shading variable
static const float4 g_cf4LightOffset = float4 ( 0.0f, 0.10f, 0.0f, 0.0f );
static const float4 g_cf4Ambient = float4 ( 0.1f, 0.1f, 0.1f, 1.0f );
static const float4 g_cf4LightAtte = float4 ( 1, 0, 0, 0 );

//static const float KinectFTRpos_x = DEPTH_WIDTH * XSCALE * 0.5f; // Top right conner x_pos in local space of Kinect's img plane = HalfDepthImgSize*XYScale
//static const float KinectFTRpos_y = DEPTH_HEIGHT * XSCALE * 0.5f; // Top right conner y_pos in local space of Kinect's img plane = HalfDepthImgSize*XYScale

static const float3 g_cf3InvVoxelReso = float3(1.f/VOXEL_NUM_X, 1.f/VOXEL_NUM_Y, 1.f/VOXEL_NUM_Z);
static const float g_cf3InvVoxelSize = 1.f/VOXEL_SIZE;
static const float2 g_cf2DepthImgReso = float2(D_W, D_H);
static const float2 g_cf2DepthRange = float2(R_N, R_F);
static const float2 g_cf2DepthF = float2(F_X, -F_Y);
static const float2 g_cf2DepthC = float2(C_X, C_Y);

static const int3 cb_QuadrantOffset[8] =
{
	int3(0, 0, 0),
	int3(1, 0, 0),
	int3(0, 1, 0),
	int3(1, 1, 0),
	int3(0, 0, 1),
	int3(1, 0, 1),
	int3(0, 1, 1),
	int3(1, 1, 1),
};
//--------------------------------------------------------------------------------------
// Const buffers
//--------------------------------------------------------------------------------------
cbuffer cbInit : register( b0 )
{
	float cb_fTruncDist;
	float3 cb_f3VolHalfSize; //  = volumeRes * voxelSize / 2
	float cb_fStepLen; // step length in meters of ray casting algorithm
	float3 cb_f3InvVolSize; // = 1 / ( volumeRes * voxelSize )
	float3 cb_f3VolBBMin; // Volume bonding box front left bottom pos in world space
	bool cb_bKinectShade; // Whether shading from Kinect's point of view
	float3 cb_f3VolBBMax; // Volume bonding box back right top pos in world space
	float NIU1;
	float2 cb_f2RTReso;
	float2 NIU2;
};
cbuffer cbKinectUpdate : register( b1 )
{
	float4 cb_f4KinectPos; // Kinect's pos in world space
	matrix cb_mInvKinectWorld; // Kinect's view transform matrix, the inversion of cb_mKinectWorld
	matrix cb_mKinectWorld; // Matrix which transform Kinect from ( 0, 0, 0 ) to its current pos and orientation
};
cbuffer cbFreeCamUpdate : register( b2 )
{
	matrix cb_mInvView;
	float4 cb_f4ViewPos;
};

//--------------------------------------------------------------------------------------
// Utility structure
//--------------------------------------------------------------------------------------
struct Ray
{
	float4 o;
	float4 d;
};

//--------------------------------------------------------------------------------------
// Utility Functions
//--------------------------------------------------------------------------------------
// bounding box intersection
bool IntersectBox(Ray r, float3 VolBBMin, float3 VolBBMax, out float fTnear, out float fTfar)
{
	// compute intersection of ray with all six bbox planes
	float3 invR = 1.0 / r.d.xyz;
	float3 tbot = invR * (VolBBMin.xyz - r.o.xyz);
	float3 ttop = invR * (VolBBMax.xyz - r.o.xyz);

	// re-order intersections to find smallest and largest on each axis
	float3 tmin = min (ttop, tbot);
	float3 tmax = max (ttop, tbot);

	// find the largest tmin and the smallest tmax
	float2 t0 = max (tmin.xx, tmin.yz);
	fTnear = max (t0.x, t0.y);
	t0 = min (tmax.xx, tmax.yz);
	fTfar = min (t0.x, t0.y);

	return fTnear<=fTfar;
}

// pos is f3 in world space, value contain f2IntersectResult value, the function will return false
// if sample data contain invalide value, otherwise will return true
bool SampleDW( float3 uv, inout float2 value, int3 offset = int3(0,0,0) )
{
	float3 fIdx = uv - g_cf3InvVoxelReso *0.5f;
	float neighbor[8];
	float2 sum = 0;
	int invalidCount = 0;
	// return Invalid value if the sample point is in voxel which contain invalid value;
	[unroll]for(int i=0; i<8; i++){
		neighbor[i] = g_srvDepthWeight.SampleLevel(g_samLinear, fIdx, 0,cb_QuadrantOffset[i]+offset);
		if (abs(value.x - INVALID_VALUE) < EPSILON) invalidCount++;
		else sum+=neighbor[i];
	}
	value = sum / (8.f - invalidCount);
	if( invalidCount >= 9) return false;
	else return true;
}

//--------------------------------------------------------------------------------------
// Compute Shaders
//--------------------------------------------------------------------------------------
/* CS for extracting RGBZ, f3Normal, shaded image from volume */
[numthreads(THREAD2D_X,THREAD2D_Y,1)]
void CS_KinectView(uint3 DTid : SV_DispatchThreadID)
{
	// Get world space pos of this texel
	float4 f4CurPos = mul(float4((DTid.xy - g_cf2DepthC) / g_cf2DepthF, 1.f, 1.f), cb_mKinectWorld);

	Ray eyeray;
	//world space
	eyeray.o = cb_f4KinectPos;
	eyeray.d = f4CurPos - eyeray.o;
	eyeray.d = normalize(eyeray.d);
	eyeray.d.x = ( eyeray.d.x == 0.f ) ? 1e-15f : eyeray.d.x;
	eyeray.d.y = ( eyeray.d.y == 0.f ) ? 1e-15f : eyeray.d.y;
	eyeray.d.z = ( eyeray.d.z == 0.f ) ? 1e-15f : eyeray.d.z;

	// calculate ray intersection with bounding box
	float fTnear, fTfar;
	bool bHit = IntersectBox(eyeray, cb_f3VolBBMin, cb_f3VolBBMax , fTnear, fTfar);
	
	// output nothing if ray didn't intesect with volume
	if (!bHit){
		g_uavKinectRGBD[DTid.xy] = float4(0.f,0.f,0.f,-1.f);
		g_uavKinectNorm[DTid.xy] = float4(0.f,0.f,0.f,-1.f);
		if(cb_bKinectShade) g_uavKinectShade[DTid.xy] = float4(0.f,0.f,0.f,0.f);
		return;
	}

	// avoid artifact if eye are inside volume;
	if( fTnear < 0.f ) fTnear = 0.f;

	// calculate the fIteration count need to sample the volume
	float fIteration = (fTfar-fTnear)/cb_fStepLen;

	// calculate intersection points
	float3 f3P = eyeray.o.xyz + eyeray.d.xyz * fTnear;

	//float t = fTnear;
	
	float3 f3P_pre = f3P;
	float3 f3Step = eyeray.d.xyz * cb_fStepLen;

	// read the first value and check whether it is bValid(is it in edges?)
	float2 f2IntersectResult = float2(0.0f, 0.0f);
	bool bValid_pre = SampleDW(f3P*cb_f3InvVolSize + 0.5f, f2IntersectResult);

	// initialize depth pre and cur
	float fDist_pre = f2IntersectResult.x*TRUNC_DIST;
	float fDist_cur;

	// ray marching
	[fastopt]
	for(float i = 0; i <fIteration; i=i+1.f) {
		// convert to texture space
		float3 f3txCoord = f3P * cb_f3InvVolSize + 0.5f;
		// read depth and weight
		float2 f2DistWeight = float2(0.0f,0.0f);
		bool bValid = SampleDW(f3txCoord, f2DistWeight);

		fDist_pre = fDist_cur;
		fDist_cur = f2DistWeight.x * TRUNC_DIST;
		if (abs(fDist_cur - INVALID_VALUE*TRUNC_DIST)>1e-8 &&abs(fDist_pre - INVALID_VALUE*TRUNC_DIST)>1e-8){
			//if ( fDist_pre < 0 ) return output;
			if ( fDist_pre * fDist_cur < 0 )
			{
				// get sub voxel txCoord
				float3 f3SurfacePos = f3P_pre + (f3P - f3P_pre) * fDist_pre / (fDist_pre - fDist_cur); 
				f3txCoord = f3SurfacePos * cb_f3InvVolSize + 0.5f;

				// get surface pos in view space
				float4 f4SurfacePos = mul(float4 (f3SurfacePos, 1), cb_mInvKinectWorld);

				// get and compute f3Normal
				float2 temp0 = float2(0.0f, 0.0f);
				float2 temp1 = float2(0.0f, 0.0f);
				SampleDW(f3txCoord, temp0, int3(1, 0, 0));
				SampleDW(f3txCoord, temp1, int3(-1, 0, 0));
				float depth_dx = temp0.x - temp1.x;
				SampleDW(f3txCoord, temp0, int3(0, 1, 0));
				SampleDW(f3txCoord, temp1, int3(0, -1, 0));
				float depth_dy = temp0.x - temp1.x;
				SampleDW(f3txCoord, temp0, int3(0, 0, 1));
				SampleDW(f3txCoord, temp1, int3(0, 0, -1));
				float depth_dz = temp0.x - temp1.x;
				// the normal is calculated and stored in view space
				float3 f3Normal = normalize(mul(float3 (depth_dx, depth_dy, depth_dz),cb_mInvKinectWorld));

				// get color (maybe later I will use color to guide ICP)
				float4 f4ColOrg = g_srvColor.SampleLevel ( g_samLinear, f3txCoord, 0);
				// only if we want to shade from kinect's point of view
				if(cb_bKinectShade){
					// the light is calculated in view space, since the normal is in view space
					float4 f4LightPos = g_cf4LightOffset + float4 ( 0.f, 0.f, 0.f, 1.f ); 
					float4 f4LightDir = f4LightPos - float4 ( f3SurfacePos, 1.f );
					float fLightDist = length ( f4LightDir );
					f4LightDir /= fLightDist;
					f4LightDir.w = clamp ( 1.0f / ( g_cf4LightAtte.x + 
										 g_cf4LightAtte.y * fLightDist + 
										 g_cf4LightAtte.z * fLightDist * fLightDist ), 0.f, 1.f );
					float fNdotL = clamp ( dot ( f3Normal, f4LightDir.xyz ), 0, 1 );
					float4 f4Color = f4ColOrg * f4LightDir.w * fNdotL;
					// output shaded image
					g_uavKinectShade[DTid.xy] = f4Color;
				}

				// output RGBD
				g_uavKinectRGBD[DTid.xy] = float4(f4ColOrg.xyz, f4SurfacePos.z);
				g_uavKinectNorm[DTid.xy] = float4(f3Normal, 0.f) * 0.5f + 0.5f;
				return ;
			}
		}
		f3P_pre = f3P;
		f3P += f3Step;
	}
	return;
}

/* CS for rendering the volume from free camera point of view */
[numthreads(THREAD2D_X, THREAD2D_Y, 1)]
void CS_FreeView(uint3 DTid : SV_DispatchThreadID)
{
	// Get world space pos of this texel
	float4 f4CurPos = mul(float4((DTid.xy - cb_f2RTReso*0.5f) / (float2(cb_f2RTReso.x,-cb_f2RTReso.x)*0.5f), 1.f, 1.f), cb_mInvView);

	Ray eyeray;
	//world space
	eyeray.o = cb_f4ViewPos;
	eyeray.d = f4CurPos - eyeray.o;
	eyeray.d = normalize(eyeray.d);
	eyeray.d.x = ( eyeray.d.x == 0.f ) ? 1e-15 : eyeray.d.x;
	eyeray.d.y = ( eyeray.d.y == 0.f ) ? 1e-15 : eyeray.d.y;
	eyeray.d.z = ( eyeray.d.z == 0.f ) ? 1e-15 : eyeray.d.z;

	// calculate ray intersection with bounding box+
	float fTnear, fTfar;
	bool bHit = IntersectBox(eyeray, cb_f3VolBBMin, cb_f3VolBBMax , fTnear, fTfar);
	
	// output nothing if ray didn't intesect with volume
	if (!bHit){
		g_uavFreeCamShade[DTid.xy] = float4(0.f, 0.f, 0.f, 0.f);
		return;
	}

	// avoid artifact if eye are inside volume;
	if (fTnear < 0.f) fTnear = 0.f;

	// calculate the fIteration count need to sample the volume
	float fIteration = (fTfar - fTnear) / cb_fStepLen;

	// calculate intersection points
	float3 f3P = eyeray.o.xyz + eyeray.d.xyz * fTnear;

	//float t = fTnear;

	float3 f3P_pre = f3P;
	float3 f3Step = eyeray.d.xyz * cb_fStepLen;

	// read the first value and check whether it is bValid(is it in edges?)
	float2 f2IntersectResult = float2(0.0f, 0.0f);
	bool bValid_pre = SampleDW(f3P*cb_f3InvVolSize + 0.5f, f2IntersectResult);

	// initialize depth pre and cur
	float fDist_pre = f2IntersectResult.x*TRUNC_DIST;
	float fDist_cur;

	// ray marching
	[fastopt]
	for (float i = 0; i <fIteration; i=i+1.f) {
		// convert to texture space
		float3 f3txCoord = f3P * cb_f3InvVolSize + 0.5f;
		// read depth and weight
		float2 f2DistWeight = float2(0.0f, 0.0f);
		bool bValid = SampleDW(f3txCoord, f2DistWeight);

		fDist_pre = fDist_cur;
		fDist_cur = f2DistWeight.x * TRUNC_DIST;
		if (abs(fDist_cur - INVALID_VALUE*TRUNC_DIST)>EPSILON &&abs(fDist_pre - INVALID_VALUE*TRUNC_DIST)>EPSILON){
			if ( fDist_pre * fDist_cur < 0 )
			{
				// get sub voxel txCoord
				float3 f3SurfacePos = f3P_pre + (f3P - f3P_pre) * fDist_pre / (fDist_pre - fDist_cur); 
				f3txCoord = f3SurfacePos * cb_f3InvVolSize + 0.5f;

				// get and compute f3Normal
				float2 temp0 = float2(0.0f, 0.0f);
				float2 temp1= float2(0.0f,0.0f);
				SampleDW(f3txCoord, temp0, int3(1, 0, 0));
				SampleDW(f3txCoord, temp1, int3(-1, 0, 0));
				float depth_dx = temp0.x - temp1.x;
				SampleDW(f3txCoord, temp0, int3(0, 1, 0));
				SampleDW(f3txCoord, temp1, int3(0, -1, 0));
				float depth_dy = temp0.x - temp1.x;
				SampleDW(f3txCoord, temp0, int3(0, 0, 1));
				SampleDW(f3txCoord, temp1, int3(0, 0, -1));
				float depth_dz = temp0.x - temp1.x;
				// the normal is calculated and stored in view space
				float3 f3Normal = normalize(float3 (depth_dx, depth_dy, depth_dz));

				// get color (maybe later I will use color to guide ICP)
				float4 f4ColOrg = g_srvColor.SampleLevel ( g_samLinear, f3txCoord, 0);
				
				// shading part
				float4 f4LightPos = mul(g_cf4LightOffset + float4 (0.f, 0.f, 0.f, 1.f), cb_mInvView);
				float4 f4LightDir = f4LightPos - float4 ( f3SurfacePos, 1 );
				float fLightDist = length ( f4LightDir );
				f4LightDir /= fLightDist;
				f4LightDir.w = clamp ( 1.0f / ( g_cf4LightAtte.x + 
									g_cf4LightAtte.y * fLightDist + 
									g_cf4LightAtte.z * fLightDist * fLightDist ), 0, 1 );
				float fNdotL = clamp ( dot ( f3Normal, f4LightDir.xyz ), 0, 1 );
				float4 f4Color = f4ColOrg * f4LightDir.w * fNdotL + g_cf4Ambient;

				// output shaded image
				g_uavFreeCamShade[DTid.xy] = f4Color;

				return;
			}
		}
		f3P_pre = f3P;
		f3P += f3Step;
	}
	g_uavFreeCamShade[DTid.xy] = float4(0.f, 0.f, 0.f, 0.f);
	return;
}

/* CS for extracting RGBZ, f3Normal, shaded image from volume */
[numthreads(THREAD2D_X, THREAD2D_Y, 1)]
void CS_Raycast(uint3 DTid : SV_DispatchThreadID)
{
	// Get world space pos of this texel
	float4 f4CurPos = mul(float4((DTid.xy - g_cf2DepthC) / g_cf2DepthF, 1.f, 1.f), cb_mKinectWorld);

	Ray eyeray;
	//world space
	eyeray.o = cb_f4KinectPos;
	eyeray.d = f4CurPos - eyeray.o;
	eyeray.d = normalize(eyeray.d);
	eyeray.d.x = ( eyeray.d.x == 0.f ) ? 1e-15f : eyeray.d.x;
	eyeray.d.y = ( eyeray.d.y == 0.f ) ? 1e-15f : eyeray.d.y;
	eyeray.d.z = ( eyeray.d.z == 0.f ) ? 1e-15f : eyeray.d.z;

	// calculate ray intersection with bounding box
	float fTnear, fTfar;
	bool bHit = IntersectBox(eyeray, cb_f3VolBBMin, cb_f3VolBBMax , fTnear, fTfar);
	
	// output nothing if ray didn't intesect with volume
	if (!bHit){
		g_uavRayCast[DTid.xy] = float4(0.f,0.f,0.f,-1.f);
		return;
	}

	// avoid artifact if eye are inside volume;
	if( fTnear < 0.f ) fTnear = 0.f;
	// calculate the fIteration count need to sample the volume
	float fIteration = (fTfar - fTnear) / cb_fStepLen;
	// calculate intersection points
	float3 f3P = eyeray.o.xyz + eyeray.d.xyz * fTnear;

	//float t = fTnear;

	float3 f3P_pre = f3P;
	float3 f3Step = eyeray.d.xyz * cb_fStepLen;
	
	float fDist_pre = g_srvDepthWeight.SampleLevel(g_samLinear,f3P * cb_f3InvVolSize + 0.5f,0.f).x * TRUNC_DIST;
	float fDist_cur;
	
	float4 output = float4(0.03f,0.03f,0.03f,0.03f);
	// ray marching
	for (float i = 0; i <fIteration; i = i + 1.f) {
		// convert to texture space
		float3 f3txCoord = f3P * cb_f3InvVolSize + 0.5f;
		float2 f2DistWeight = g_srvDepthWeight.SampleLevel ( g_samLinear, f3txCoord, 0 ).xy;
		//float3 f2DistWeight = g_srvDepthWeight.Load ( int4 ( f3P/meterPerVoxel + voxelResolution / 2.0f, 0 ), 0 );
		float4 value = float4( 0.7f, 0.f, 0.f, 0.f );
		fDist_pre = fDist_cur;
		fDist_cur = f2DistWeight.x ;
		if( fDist_cur <= 1 )
		{
			value = float4( 0, 1.0, 0, 0 );
			if ( fDist_cur >= -1 )
			{
				// For computing the depth
				float3 f3SurfacePos = f3P_pre + (f3P - f3P_pre) * fDist_pre / (fDist_pre - fDist_cur); 
				f3txCoord = f3SurfacePos * cb_f3InvVolSize + 0.5;
				value = 100.0 * g_srvColor.SampleLevel ( g_samLinear, f3txCoord, 0 );
			}
		}
		
		output += value * 0.001;
	
		f3P += 0.1*f3Step;
	}
	g_uavRayCast[DTid.xy] = output;
	return;
}