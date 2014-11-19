#include "D3DX_DXGIFormatConvert.inl"
#include "header.h"
// UAV to write to
RWTexture3D<uint> tex_DistWeight : register(u0);
RWTexture3D<uint> tex_RGBColor : register(u1);
// SRV to read from
Texture2D<float4> txRGBD  : register(t0); // Contain color and depth(alpha channel)
Texture2D<float4> txNormal : register(t1); // Contain normal, for bad data rejection

static const float2 reso = float2(D_W, D_H); // RGBD tex resolution
static const float2 range = float2(R_N, R_F); // Depth range in meter
static const float2 f = float2(F_X, -F_Y); // Calibration result data fxy
static const float2 c = float2(C_X, C_Y); // Calibration result data cxy

cbuffer cbVolumeInit : register ( b0 )
{
	const float3 HalfVoxelRes; // Half number of voxels in each dimension
	const float VoxelSize;	// Voxel size in meters
	const float trunc;	// tuncated distant
	const float MaxWeight;	// maximum allowed weight
};

cbuffer cbFrameUpdate : register ( b1 )
{
	matrix inversedWorld_kinect;
};

//--------------------------------------------------------------------------------------
// Compute Shader
//--------------------------------------------------------------------------------------
[numthreads(THREAD_X, THREAD_Y, THREAD_Z)]
void CS(uint3 DTid: SV_DispatchThreadID)
{
	// Current voxel pos in local space
	float4 currentVoxelPos = float4((DTid - HalfVoxelRes + 0.5f) * VoxelSize,1);
	// Convert to kinect sensor space
	currentVoxelPos = mul(currentVoxelPos, inversedWorld_kinect);
	// Discard if current z value is outside the valid kinect frustum
	if (currentVoxelPos.z <= range.x) return;
	// Project current voxel onto img plane
	float2 backProjectedXY = currentVoxelPos.xy / currentVoxelPos.z * f + c;
	// Discard if UV is outside the valid range
	if (backProjectedXY.x > reso.x || backProjectedXY.x < 0 ||
		backProjectedXY.y > reso.y || backProjectedXY.y < 0)
		return;

	// Read RGBD data
	float4 RGBD = txRGBD.Load(int3 (backProjectedXY, 0));
	// Get the depth difference between current voxel and income surface
	float z_dif = RGBD.a - currentVoxelPos.z;

	if (z_dif > -TRUNC_DIST)
	{
		// Read Normal Data
		float4 codedNormal = txNormal.Load(int3 (backProjectedXY, 0));
			// Discard if normal is invalid
			//if(codedNormal.a < 0) discard;

		float weight = 1;
		float tsdf;
		tsdf = min(1, z_dif / TRUNC_DIST);

		// Read the previous data
		float2 previousValue = D3DX_R16G16_FLOAT_to_FLOAT2(tex_DistWeight[DTid]);
		float4 previousColor = D3DX_R10G10B10A2_UNORM_to_FLOAT4(tex_RGBColor[DTid]);

		float pre_weight = previousValue.y;
		float pre_tsdf = previousValue.x;

		float3 Normal = (codedNormal - float4(0.5, 0.5, 0.5, 0)).xyz * 2.f;
		float norAngle = dot(-normalize(currentVoxelPos.xyz), Normal) * 256;

		//if( norAngle < 0.5f/sqrt(2)) discard;
		//if( tsdf >= 1 && pre_tsdf < 1 ) return 0;
		//if( abs(tsdf - pre_tsdf) > 1) discard;


		float2 DepthWeight;
		DepthWeight.x = (tsdf * weight + pre_tsdf * pre_weight) / (weight + pre_weight);
		DepthWeight.y = min(weight + pre_weight, MaxWeight);

		tex_DistWeight[DTid] = D3DX_FLOAT2_to_R16G16_FLOAT(DepthWeight);

		//if( norAngle < previousColor.z) return 0;
		if (dot(RGBD.xyz, RGBD.xyz)<0.001) return;
		float3 col = (RGBD.xyz * weight + previousColor.xyz * pre_weight) / (weight + pre_weight);
		tex_RGBColor[DTid] = D3DX_FLOAT4_to_R10G10B10A2_UNORM(float4(col, norAngle));

		return;
	}
	//tex_DistWeight[DTid] = D3DX_FLOAT2_to_R16G16_FLOAT(float2(R_F, 0.f));
	return;
}

[numthreads(THREAD_X, THREAD_Y, THREAD_Z)]
void ResetCS(uint3 DTid: SV_DispatchThreadID)
{
	tex_DistWeight[DTid] = D3DX_FLOAT2_to_R16G16_FLOAT(float2(INVALID_VALUE,0.f));
	tex_RGBColor[DTid] = D3DX_FLOAT4_to_R10G10B10A2_UNORM(float4(0, 0, 0, 0));
}