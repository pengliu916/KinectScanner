#include "header.h"

Texture3D g_txVolume : register(t0);
Texture3D g_txVolume_color : register(t1);

SamplerState samRaycast : register(s0);

// Phong shading variable
static const float4 light_offset = float4 ( 0.0f, 0.10f, 0.0f, 0.0f );
static const float4 ambient = float4 ( 0.3f, 0.3f, 0.3f, 1.0f );
static const float4 light_attenuation = float4 ( 1, 0, 0, 0 );

//static const float KinectFTRpos_x = DEPTH_WIDTH * XSCALE * 0.5f; // Top right conner x_pos in local space of Kinect's img plane = HalfDepthImgSize*XYScale
//static const float KinectFTRpos_y = DEPTH_HEIGHT * XSCALE * 0.5f; // Top right conner y_pos in local space of Kinect's img plane = HalfDepthImgSize*XYScale


static const float2 reso = float2(D_W, D_H);
static const float2 range = float2(R_N, R_F);
static const float2 f = float2(F_X, -F_Y);
static const float2 c = float2(C_X, C_Y);

//--------------------------------------------------------------------------------------
// Const buffers
//--------------------------------------------------------------------------------------
cbuffer cbInit : register( b0 )
{
	float Tstep; // step length in meters of ray casting algorithm
	float3 VolumeHalfSize; //  = volumeRes * voxelSize / 2
	float TruncDist;
	float3 ReversedTotalSize; // = 1 / ( volumeRes * voxelSize )
	float3 BoxMin; // Volume bonding box front left bottom pos in world space
	float NIU0;
	float3 BoxMax; // Volume bonding box back right top pos in world space
	float NIU1;
};
cbuffer cbKinectUpdate : register( b1 )
{
	float4 KinectPos; // Kinect's pos in world space
	matrix KinectView; // Kinect's view transform matrix, the inversion of KinectTransform
	matrix KinectTransform; // Matrix which transform Kinect from ( 0, 0, 0 ) to its current pos and orientation
};
cbuffer cbFreeCamUpdate : register( b2 )
{
	matrix WorldViewProjection;
	float4 viewPos;
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
bool IntersectBox(Ray r, float3 BoxMin, float3 BoxMax, out float tnear, out float tfar)
{
	// compute intersection of ray with all six bbox planes
	float3 invR = 1.0 / r.d.xyz;
		float3 tbot = invR * (BoxMin.xyz - r.o.xyz);
		float3 ttop = invR * (BoxMax.xyz - r.o.xyz);

		// re-order intersections to find smallest and largest on each axis
		float3 tmin = min (ttop, tbot);
		float3 tmax = max (ttop, tbot);

		// find the largest tmin and the smallest tmax
		float2 t0 = max (tmin.xx, tmin.yz);
		tnear = max (t0.x, t0.y);
	t0 = min (tmax.xx, tmax.yz);
	tfar = min (t0.x, t0.y);

	return tnear<=tfar;
}

//--------------------------------------------------------------------------------------
// Shader I/O structure
//--------------------------------------------------------------------------------------
struct GS_INPUT{};

struct PS_INPUT
{
	float4	projPos : SV_POSITION;
	float4	Pos : TEXCOORD0;
};

struct PS_3_OUT
{
	float4 Depth : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float4 Image : SV_TARGET2;
};

//--------------------------------------------------------------------------------------
// Shaders
//--------------------------------------------------------------------------------------
/* Pass through VS */
GS_INPUT VS( )
{
	GS_INPUT output = ( GS_INPUT )0;
	return output;
}
/* GS for extracting RGBZ image from volume ------------ texVolume read, no half pixel correction*/
[maxvertexcount(4)]
void GS_KinectQuad( point GS_INPUT particles[1], inout TriangleStream<PS_INPUT> triStream )
{
	PS_INPUT output;
	//output.Pos = float4(-KinectFTRpos_x, KinectFTRpos_y, 1.0f, 1.0f);
	output.Pos = float4((float2(0,0) - c) / f, 1.0f, 1.0f);
	output.projPos = float4( -1, 1, 0, 1 );
	triStream.Append( output );

	//output.Pos = float4(KinectFTRpos_x, KinectFTRpos_y, 1.0f, 1.0f);
	output.Pos = float4((float2(reso.x,0) - c) / f, 1.0f, 1.0f);
	output.projPos = float4( 1, 1, 0, 1 );
	triStream.Append( output );

	//output.Pos = float4(-KinectFTRpos_x, -KinectFTRpos_y, 1.0f, 1.0f);// TEXCOORD0
	output.Pos = float4((float2(0,reso.y) - c) / f , 1.0f, 1.0f);// TEXCOORD0
	output.projPos = float4( -1, -1, 0, 1 );// SV_POSITION
	triStream.Append( output );

	//output.Pos = float4(KinectFTRpos_x, -KinectFTRpos_y, 1.0f, 1.0f);
	output.Pos = float4((reso - c) / f,1.0f, 1.0f);
	output.projPos = float4( 1, -1, 0, 1 );
	triStream.Append( output );
}

/*GS for rendering the volume on screen ------------texVolume Read, no half pixel correction*/
[maxvertexcount(18)]
void GS_VolumeCube(point GS_INPUT particles[1], inout TriangleStream<PS_INPUT> triStream)
{
	PS_INPUT output;
	output.Pos=float4(1.0f,1.0f,1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(1.0f,-1.0f,1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(1.0f,1.0f,-1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(1.0f,-1.0f,-1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(-1.0f,1.0f,-1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(-1.0f,-1.0f,-1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(-1.0f,1.0f,1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(-1.0f,-1.0f,1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(1.0f,1.0f,1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(1.0f,-1.0f,1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);

	triStream.RestartStrip();

	output.Pos=float4(1.0f,1.0f,1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(1.0f,1.0f,-1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(-1.0f,1.0f,1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(-1.0f,1.0f,-1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);

	triStream.RestartStrip();

	output.Pos=float4(1.0f,-1.0f,-1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(1.0f,-1.0f,1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(-1.0f,-1.0f,-1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);
	output.Pos=float4(-1.0f,-1.0f,1.0f,1.0f)*float4(VolumeHalfSize,1);
	output.projPos=mul(output.Pos,WorldViewProjection);
	triStream.Append(output);

}
/* PS for extracting RGBZ, normal, shaded image from volume */
PS_3_OUT PS_KinectView(PS_INPUT input) : SV_Target
{
	PS_3_OUT output;
	output.Depth = float4 ( 0, 0, 0, -1 );
	output.Normal = float4 ( 0, 0, 0, -1 );
	output.Image = float4 ( 0, 0, 0, 0 );

	Ray eyeray;
	//world space
	eyeray.o = KinectPos;
	eyeray.d = mul( input.Pos, KinectTransform ) - eyeray.o;
	eyeray.d = normalize(eyeray.d);
	eyeray.d.x = ( eyeray.d.x == 0.f ) ? 1e-15 : eyeray.d.x;
	eyeray.d.y = ( eyeray.d.y == 0.f ) ? 1e-15 : eyeray.d.y;
	eyeray.d.z = ( eyeray.d.z == 0.f ) ? 1e-15 : eyeray.d.z;

	// calculate ray intersection with bounding box
	float tnear, tfar;
	bool hit = IntersectBox(eyeray, BoxMin, BoxMax , tnear, tfar);
	if (!hit) return output;       
	if( tnear < 0 ) tnear = 0;

	// calculate light pos
	float4 light_pos = mul ( light_offset + float4 ( 0, 0, 0, 1 ), KinectTransform ); 

	// calculate intersection points
	float3 Pnear = eyeray.o.xyz + eyeray.d.xyz * tnear;
	float3 Pfar = eyeray.o.xyz + eyeray.d.xyz * tfar;

	float3 P = Pnear;
	float t = tnear;
	
	float3 P_pre = Pnear;
	float3 PStep = eyeray.d.xyz * Tstep;

	float3 currentPixPos;

	float dist_pre = g_txVolume.SampleLevel(samRaycast,P * ReversedTotalSize + 0.5,0).x * TruncDist;;
	float dist_now;
	
	while ( t <= tfar ) {
		float3 txCoord = P * ReversedTotalSize + 0.5;
		float2 DistWeight = g_txVolume.SampleLevel ( samRaycast, txCoord, 0 );
		//float3 DistWeight = g_txVolume.Load ( int4 ( P/meterPerVoxel + voxelResolution / 2.0f, 0 ), 0 );

		dist_pre = dist_now;
		dist_now = DistWeight.x * TruncDist;
		
		//if ( dist_pre < 0 ) return output;
		if ( dist_pre * dist_now < 0 )
		//if ( dist_pre >= 0 && dist_now < 0 )
		{
			// For computing the depth
			currentPixPos = P_pre + (P - P_pre) * dist_pre / (dist_pre - dist_now); 
			txCoord = currentPixPos * ReversedTotalSize + 0.5;
			float4 surfacePos = float4 ( currentPixPos, 1 );
			surfacePos = mul ( surfacePos, KinectView );
			float dist = surfacePos.z;

			// For computing the normal
			float depth_dx = g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( 1, 0, 0 ) ).x - 
								g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( -1, 0, 0 ) ).x;
			float depth_dy = g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( 0, 1, 0 ) ).x - 
								g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( 0, -1, 0 ) ).x;
			float depth_dz = g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( 0, 0, 1 ) ).x - 
								g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( 0, 0, -1 ) ).x;

			float3 normal = normalize ( float3 ( depth_dx, depth_dy, depth_dz ) );


			// shading part
			float4 light_dir = light_pos - float4 ( currentPixPos, 1 );
			float light_dist = length ( light_dir );
			light_dir /= light_dist;
			light_dir.w = clamp ( 0, 1, 1.0f / ( light_attenuation.x + 
												 light_attenuation.y * light_dist + 
												 light_attenuation.z * light_dist * light_dist ) );
			float angleAttn = clamp ( 0, 1, dot ( normal, light_dir.xyz ) );
			float4 col_org = g_txVolume_color.SampleLevel ( samRaycast, txCoord, 0);
			float4 col = col_org * light_dir.w * angleAttn;

			output.Depth = float4 ( col_org.xyz, dist );
			output.Normal = float4 ( normal, 0 ) * 0.5 + 0.5;
			//output.Normal = float4 ( 0,1,0,0);
			output.Image = col;

			return output;
		}
		P_pre = P;
		P += PStep;
		t += Tstep;
	}
	return output;       
}


float4 PS_FreeView(PS_INPUT input) : SV_Target
{
	float4 output = float4 ( 0.02, 0.02, 0.02, 0 );

	Ray eyeray;
	//world space
	eyeray.o = viewPos;
	eyeray.d = input.Pos-eyeray.o;
	eyeray.d = normalize(eyeray.d);
	eyeray.d.x = ( eyeray.d.x == 0.f ) ? 1e-15 : eyeray.d.x;
	eyeray.d.y = ( eyeray.d.y == 0.f ) ? 1e-15 : eyeray.d.y;
	eyeray.d.z = ( eyeray.d.z == 0.f ) ? 1e-15 : eyeray.d.z;

	// calculate ray intersection with bounding box+
	float tnear, tfar;
	bool hit = IntersectBox(eyeray, BoxMin, BoxMax , tnear, tfar);
	if (!hit) return output;          
	if( tnear < 0 ) tnear = 0; 

	// calculate light pos
	float4 light_pos =  light_offset + viewPos ; 

	// calculate intersection points
	float3 Pnear = eyeray.o.xyz + eyeray.d.xyz * tnear;
	float3 Pfar = eyeray.o.xyz + eyeray.d.xyz * tfar;

	float3 P = Pnear;
	float t = tnear;
	
	float3 P_pre = Pnear;
	float3 PStep = eyeray.d.xyz * Tstep;

	float3 currentPixPos;

	float dist_pre = g_txVolume.SampleLevel(samRaycast,P * ReversedTotalSize + 0.5,0).x * TruncDist;
	float dist_now;
	
	while ( t <= tfar ) {
		float3 txCoord = P * ReversedTotalSize + 0.5;
		float2 DistWeight = g_txVolume.SampleLevel ( samRaycast, txCoord, 0 ).xy;
		//float3 DistWeight = g_txVolume.Load ( int4 ( P/meterPerVoxel + voxelResolution / 2.0f, 0 ), 0 );

		dist_pre = dist_now;
		dist_now = DistWeight.x * TruncDist;
		
		if ( dist_pre >= 0 && dist_now < 0 )
		{
			// For computing the depth
			currentPixPos = P_pre + (P - P_pre) * dist_pre / (dist_pre - dist_now); 
			txCoord = currentPixPos * ReversedTotalSize + 0.5;

			// For computing the normal
			float depth_dx = g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( 1, 0, 0 ) ).x - 
								g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( -1, 0, 0 ) ).x;
			float depth_dy = g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( 0, 1, 0 ) ).x - 
								g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( 0, -1, 0 ) ).x;
			float depth_dz = g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( 0, 0, 1 ) ).x - 
								g_txVolume.SampleLevel ( samRaycast, txCoord, 0, int3 ( 0, 0, -1 ) ).x;

			float3 normal = normalize ( float3 ( depth_dx, depth_dy, depth_dz ) );


			// shading part
			float4 light_dir = light_pos - float4 ( currentPixPos, 1 );
			float light_dist = length ( light_dir );
			light_dir /= light_dist;
			light_dir.w = clamp ( 0, 1, 1.0f / ( light_attenuation.x + 
												 light_attenuation.y * light_dist + 
												 light_attenuation.z * light_dist * light_dist ) );
			float angleAttn = clamp ( 0, 1, dot ( normal, light_dir.xyz ) );
			float4 col_org = g_txVolume_color.SampleLevel ( samRaycast, txCoord, 0);
			float4 col = col_org * light_dir.w * angleAttn;

			output = col;

			return output;
		}
		P_pre = P;
		P += PStep;
		t += Tstep;
	}
	return output;       
}

float4 PS_Raycast(PS_INPUT input) : SV_Target
{
	float4 output = float4 ( 0.03, 0.03, 0.03, 0 );

	Ray eyeray;
	//world space
	eyeray.o = viewPos;
	eyeray.d = input.Pos-eyeray.o;
	eyeray.d = normalize(eyeray.d);
	eyeray.d.x = ( eyeray.d.x == 0.f ) ? 1e-15 : eyeray.d.x;
	eyeray.d.y = ( eyeray.d.y == 0.f ) ? 1e-15 : eyeray.d.y;
	eyeray.d.z = ( eyeray.d.z == 0.f ) ? 1e-15 : eyeray.d.z;

	// calculate ray intersection with bounding box
	float tnear, tfar;
	bool hit = IntersectBox(eyeray, BoxMin, BoxMax , tnear, tfar);
	if (!hit) return output;           
	if( tnear < 0 ) tnear = 0;

	// calculate intersection points
	float3 Pnear = eyeray.o.xyz + eyeray.d.xyz * tnear;
	float3 Pfar = eyeray.o.xyz + eyeray.d.xyz * tfar;

	float3 P = Pnear;
	float t = tnear;

	float3 P_pre = Pnear;
	float3 PStep = eyeray.d.xyz * Tstep;

	float3 currentPixPos;
	
	float dist_pre = g_txVolume.SampleLevel(samRaycast,P * ReversedTotalSize + 0.5,0).x * TruncDist;
	float dist_now;
	
	while ( t <= tfar ) {
		float3 txCoord = P * ReversedTotalSize + 0.5;
		float2 DistWeight = g_txVolume.SampleLevel ( samRaycast, txCoord, 0 ).xy;
		//float3 DistWeight = g_txVolume.Load ( int4 ( P/meterPerVoxel + voxelResolution / 2.0f, 0 ), 0 );
		float4 value = float4( 0.7, 0, 0, 0 );
		dist_pre = dist_now;
		dist_now = DistWeight.x ;
		if( dist_now <= 1 )
		{
			value = float4( 0, 1.0, 0, 0 );
			if ( dist_now >= -1 || dist_now < 1 )
			{
				// For computing the depth
				currentPixPos = P_pre + (P - P_pre) * dist_pre / (dist_pre - dist_now); 
				txCoord = currentPixPos * ReversedTotalSize + 0.5;
				value = 100.0 * g_txVolume_color.SampleLevel ( samRaycast, txCoord, 0 );
			}
		}
		
		output += value * 0.001;
	
		P += 0.1*PStep;
		t += 0.1*Tstep;
	}
	return output;       
	//return float4(1,1,0,0);       
}