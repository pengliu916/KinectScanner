#include "D3DX_DXGIFormatConvert.inl"
#include "header.h"
// UAV to write to
RWTexture3D<uint> tex3D_DistWeight : register(t0);
RWTexture3D<uint> tex3D_RGBColor : register(t1);
// SRV to read from
Texture2D<float4> txRGBD  : register(t2); // Contain color and depth(alpha channel)
Texture2D<float4> txNormal : register(t3); // Contain normal, for bad data rejection

static const float2 reso = float2(D_W, D_H); // RGBD tex resolution
static const float2 range = float2(R_N, R_F); // Depth range in meter
static const float2 f = float2(F_X, -F_Y); // Calibration result data fxy
static const float2 c = float2(C_X, C_Y); // Calibration result data cxy

cbuffer cbVolumeInit : register ( b0 )
{
	const int3 HalfVoxelRes; // Half number of voxels in each dimension
	const float VoxelSize;	// Voxel size in meters
	const float trunc;	// tuncated distant
	const float MaxWeight;	// maximum allowed weight
};

cbuffer cbFrameUpdate : register ( b1 )
{
	matrix inversedWorld_kinect;
};

/* Pass through VS */
struct GS_INPUT{};
GS_INPUT VS( )
{
	GS_INPUT output = ( GS_INPUT )0;
	return output;
}

/* GS for Volume updating (render to volume) ---------------texVolume write, half pixel correction needed*/
struct PS_INPUT
{
	float4	Pos : SV_POSITION;
	float3  Coord : TEXCOORD0;
};
[maxvertexcount(4)]
void GS( point GS_INPUT particles[1], uint primID : SV_PrimitiveID, inout TriangleStream<PS_INPUT> triStream )
{
	PS_INPUT output;

	output.Pos = float4( -1.0f, 1.0f, 0.0f, 1.0f );
	output.Coord = float3( -HalfVoxelRes.x, -HalfVoxelRes.y, ( float )primID - HalfVoxelRes.z + 0.5 );
	triStream.Append( output );

	output.Pos = float4( 1.0f, 1.0f, 0.0f, 1.0f );
	output.Coord = float3( HalfVoxelRes.x, -HalfVoxelRes.y, ( float )primID - HalfVoxelRes.z + 0.5);
	triStream.Append( output );

	output.Pos = float4( -1.0f, -1.0f, 0.0f, 1.0f);
	output.Coord = float3( -HalfVoxelRes.x, HalfVoxelRes.y, ( float )primID - HalfVoxelRes.z + 0.5 );
	triStream.Append( output );

	output.Pos = float4( 1.0f, -1.0f, 0.0f, 1.0f );
	output.Coord = float3( HalfVoxelRes.x, HalfVoxelRes.y, ( float )primID - HalfVoxelRes.z + 0.5 );
	triStream.Append( output );
}

/* PS for volume updating (render to volume) ------------- texVolume read and write need half pixel correction*/
float PS( PS_INPUT input ) : SV_Target
{
	//Voxel position in model space
	//after half pixel correction during GS, the voxel pos is exact and correct for reading Volume and computing
	float4 currentVoxelPos = float4 ( input.Coord * VoxelSize, 1 );//in meters of model space
	currentVoxelPos = mul ( currentVoxelPos, inversedWorld_kinect );

	if( currentVoxelPos.z <= range.x ) discard;

	// Project current voxel onto img plane
	float2 backProjectedXY = currentVoxelPos.xy / currentVoxelPos.z * f + c;
	if (backProjectedXY.x > reso.x || backProjectedXY.x < 0 ||
		backProjectedXY.y > reso.y || backProjectedXY.y < 0)
		discard;

	// Read RGBD data
	/*float4 RGBD;
	float term = 0.25 - dot(currentVoxelPos.xy, currentVoxelPos.xy);
	if(term>=0) RGBD = float4(1, 1, 1, -sqrt(term));
	else RGBD = float4(1,1,1,0);
	if(abs(currentVoxelPos.x)<0.2) discard;*/
	float4 RGBD = txRGBD.Load(int3 (backProjectedXY, 0));
	
	float realDepth = RGBD.a;

	float z_dif = realDepth - currentVoxelPos.z;

	if ( z_dif > -TRUNC_DIST )
	{
		// Read Normal Data
		float4 codedNormal = txNormal.Load(int3 (backProjectedXY, 0));
		// Discard if normal is invalid
		//if(codedNormal.a < 0) discard;

		float weight = 1;
		float tsdf;
		if( z_dif > -TRUNC_DIST) tsdf = min ( 1, z_dif / TRUNC_DIST );
		else tsdf = max( -1, z_dif / TRUNC_DIST);
		
		// Read the previous data
		float2 previousValue = D3DX_R16G16_FLOAT_to_FLOAT2( tex3D_DistWeight[ input.Coord + HalfVoxelRes ] );
		float4 previousColor = D3DX_R8G8B8A8_UNORM_to_FLOAT4( tex3D_RGBColor[ input.Coord + HalfVoxelRes] );

		float pre_weight = previousValue.y;
		float pre_tsdf = previousValue.x;

		float3 Normal = (codedNormal - float4( 0.5, 0.5, 0.5, 0)).xyz * 2.f;
		float norAngle = dot( -normalize(currentVoxelPos), Normal);

		//if( norAngle < 1.f/sqrt(2)) discard;
		//if( tsdf >= 1 && pre_tsdf < 1 ) return 0;
		//if( abs(tsdf - pre_tsdf) > 1) discard;

		//if( norAngle < previousColor.z) discard;

		float2 DepthWeight;
		DepthWeight.x = ( tsdf * weight + pre_tsdf * pre_weight ) / ( weight + pre_weight );
		DepthWeight.y = min ( weight + pre_weight, MaxWeight );

		tex3D_DistWeight[ input.Coord + HalfVoxelRes ] = D3DX_FLOAT2_to_R16G16_FLOAT( DepthWeight );

		if (dot(RGBD.xyz, RGBD.xyz)<0.001) discard;
		float3 col = (RGBD.xyz * weight + previousColor.xyz * pre_weight) / (weight + pre_weight);
		tex3D_RGBColor[ input.Coord + HalfVoxelRes ] = D3DX_FLOAT4_to_R8G8B8A8_UNORM( float4( col,norAngle ) );

		return 0;
	}
	return 0;



	//// Uncomment above for correctness, the following is only for testing 
	////Voxel position in model space
	////after half pixel correction during GS, the voxel pos is exact and correct for reading Volume and computing
	//float4 currentVoxelPos = float4 (input.Coord * VoxelSize, 1);//in meters of model space

	//float radius = dot(currentVoxelPos.xyz, currentVoxelPos.xyz);

	//float2 DepthWeight;
	//DepthWeight.x = radius - 0.5;
	//DepthWeight.y = MaxWeight;

	//tex3D_DistWeight[input.Coord + HalfVoxelRes] = D3DX_FLOAT2_to_R16G16_FLOAT(DepthWeight);

	//float3 col = float3(1,1,1);
	//tex3D_RGBColor[input.Coord + HalfVoxelRes] = D3DX_FLOAT4_to_R10G10B10A2_UNORM(float4(col, 0));

	//return 0;
}
