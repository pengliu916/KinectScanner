#include "D3DX_DXGIFormatConvert.inl"
#include "header.h"

RWTexture3D<uint> tex3D_DistWeight : register(t0);
RWTexture3D<uint> tex3D_RGBColor : register(t1);
Texture2D<float4> txRGBZ  : register(t2);

//static const float4 XYScale = float4(-XSCALE,-XSCALE,0,0); // Mirro the input image for correctness
//static const float2 HalfDepthImgSize = float2(DEPTH_WIDTH, DEPTH_HEIGHT) * 0.5f; // Half resolution of depth image



static const float2 reso = float2(D_W, D_H);
static const float2 range = float2(R_N, R_F);
static const float2 f = float2(F_X, -F_Y);
static const float2 c = float2(C_X, C_Y);

cbuffer cbVolumeInit : register ( b0 )
{
	const int3 HalfVoxelRes; // Half number of voxels in each dimension
	const float VoxelSize;	// Voxel size in meters
	const float TruncDist;	// tuncated distant
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
void GS( point GS_INPUT particles[1], uint primID : SV_PrimitiveID, 
					 inout TriangleStream<PS_INPUT> triStream )
{
	PS_INPUT output;

	output.Pos = float4( -1.0f, 1.0f, 0.0f, 1.0f );
	output.Coord = float3( -HalfVoxelRes.x, HalfVoxelRes.y, ( float )primID - HalfVoxelRes.z + 0.5 );
	triStream.Append( output );

	output.Pos = float4( 1.0f, 1.0f, 0.0f, 1.0f );
	output.Coord = float3( HalfVoxelRes.x, HalfVoxelRes.y, ( float )primID - HalfVoxelRes.z + 0.5);
	triStream.Append( output );

	output.Pos = float4( -1.0f, -1.0f, 0.0f, 1.0f);
	output.Coord = float3( -HalfVoxelRes.x, -HalfVoxelRes.y, ( float )primID - HalfVoxelRes.z + 0.5 );
	triStream.Append( output );

	output.Pos = float4( 1.0f, -1.0f, 0.0f, 1.0f );
	output.Coord = float3( HalfVoxelRes.x, -HalfVoxelRes.y, ( float )primID - HalfVoxelRes.z + 0.5 );
	triStream.Append( output );
}

/* PS for volume updating (render to volume) ------------- texVolume read and write need half pixel correction*/
float PS( PS_INPUT input ) : SV_Target
{
	//Voxel position in model space
	//after half pixel correction during GS, the voxel pos is exact and correct for reading Volume and computing
	float4 currentVoxelPos = float4 ( input.Coord * VoxelSize, 1 );//in meters of model space
	currentVoxelPos = mul ( currentVoxelPos, inversedWorld_kinect );

	if( currentVoxelPos.z <= 0.4 ) return 0;
	//should in [-HalfDepthImgSize,HalfDepthImgSize] 
	//float2 backProjectedXY = currentVoxelPos.xy / ( currentVoxelPos.z * XYScale.xy );

	////if ( any ( clamp ( abs ( backProjectedXY ) - HalfDepthImgSize, 0, 10 ) ) )
	//if ( backProjectedXY.x > HalfDepthImgSize.x || backProjectedXY.x < -HalfDepthImgSize.x ||
	//	 backProjectedXY.y > HalfDepthImgSize.y || backProjectedXY.y < -HalfDepthImgSize.x )
	//	 return 0;

	//Shift for texture Load function 
	//backProjectedXY = backProjectedXY + HalfDepthImgSize;

	float2 backProjectedXY = currentVoxelPos.xy / currentVoxelPos.z * f + c;
	if (backProjectedXY.x > reso.x || backProjectedXY.x < 0 ||
		backProjectedXY.y > reso.y || backProjectedXY.y < 0)
		return 0;
	float4 RGBZdata =  txRGBZ.Load ( int3 ( backProjectedXY, 0 ) );
	
	float realDepth = RGBZdata.a;

	float z_dif = realDepth - currentVoxelPos.z;

	if ( z_dif > -TruncDist )
	{
		float weight = 1;
		float tsdf = min ( 1, z_dif / TruncDist );
	
		float2 previousValue = D3DX_R16G16_FLOAT_to_FLOAT2( tex3D_DistWeight[ input.Coord + HalfVoxelRes ] );
		float3 previousColor = D3DX_R10G10B10A2_UNORM_to_FLOAT4( tex3D_RGBColor[ input.Coord + HalfVoxelRes] ).xyz;

		float pre_weight = previousValue.y;
		float pre_tsdf = previousValue.x;

		//if( tsdf >= 1 && pre_tsdf < 1 ) return 0;

		float3 col = (RGBZdata.xyz * weight + previousColor * pre_weight) / (weight + pre_weight);

		float2 DepthWeight;
		DepthWeight.x = ( tsdf * weight + pre_tsdf * pre_weight ) / ( weight + pre_weight );
		DepthWeight.y = min ( weight + pre_weight, MaxWeight );

		tex3D_DistWeight[ input.Coord + HalfVoxelRes ] = D3DX_FLOAT2_to_R16G16_FLOAT( DepthWeight );
		tex3D_RGBColor[ input.Coord + HalfVoxelRes ] = D3DX_FLOAT4_to_R10G10B10A2_UNORM( float4( col,0 ) );

		return 0;
	}
	return 0;
}
