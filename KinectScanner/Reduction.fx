//--------------------------------------------------------------------------------------
// GPU algorithm for parallel reduction see 
// http://channel9.msdn.com/Blogs/gclassy/DirectCompute-Lecture-Series-210-GPU-Optimizations-and-Performance 
// for more detail
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

RWStructuredBuffer<InterData> g_buf : register(u0);

groupshared InterData share_buf[THREAD1D];

cbuffer cbPerFrame :register(b0){
	uint4 cb_u4ElmCount;
};
//--------------------------------------------------------------------------------------
// Compute Shader
//--------------------------------------------------------------------------------------
[numthreads(THREAD1D,1,1)]
void CS(uint Tid : SV_GroupIndex, uint3 GTid : SV_GroupID)
{
	// each thread loads one element from global to share memory
	uint tid = Tid;
	uint idx = GTid.x * THREAD1D + Tid;
	if(idx<cb_u4ElmCount.x){
		share_buf[tid] = g_buf[idx];
	}else{
		share_buf[tid].f4DataElm0 = float4(0.f, 0.f, 0.f, 0.f);
		share_buf[tid].f4DataElm1 = float4(0.f, 0.f, 0.f, 0.f);
		share_buf[tid].f4DataElm2 = float4(0.f, 0.f, 0.f, 0.f);
		share_buf[tid].f4DataElm3 = float4(0.f, 0.f, 0.f, 0.f);
		share_buf[tid].f4DataElm4 = float4(0.f, 0.f, 0.f, 0.f);
		share_buf[tid].f4DataElm5 = float4(0.f, 0.f, 0.f, 0.f);
		share_buf[tid].f4DataElm6 = float4(0.f, 0.f, 0.f, 0.f);
	}
	GroupMemoryBarrierWithGroupSync();

	// do reduction in shared memory
	for(uint i=1;i<THREAD1D;i*=2){
		if(tid%(2*i)==0){
			share_buf[tid].f4DataElm0 += share_buf[tid + i].f4DataElm0;
			share_buf[tid].f4DataElm1 += share_buf[tid + i].f4DataElm1;
			share_buf[tid].f4DataElm2 += share_buf[tid + i].f4DataElm2;
			share_buf[tid].f4DataElm3 += share_buf[tid + i].f4DataElm3;
			share_buf[tid].f4DataElm4 += share_buf[tid + i].f4DataElm4;
			share_buf[tid].f4DataElm5 += share_buf[tid + i].f4DataElm5;
			share_buf[tid].f4DataElm6 += share_buf[tid + i].f4DataElm6;
		}
		GroupMemoryBarrierWithGroupSync();
	}
	if(tid==0) g_buf[GTid.x] = share_buf[0];
}