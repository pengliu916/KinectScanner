//--------------------------------------------------------------------------------------
// GPU algorithm for parallel reduction see 
// http://channel9.msdn.com/Blogs/gclassy/DirectCompute-Lecture-Series-210-GPU-Optimizations-and-Performance 
// for more detail
//--------------------------------------------------------------------------------------
#include "header.h"
//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------

RWStructuredBuffer<float4> g_idata0 : register(u0);

groupshared float4 sdata0[THREAD1D];

cbuffer cbPerFrame :register(b0){
	uint4 cb_u4ElmCount;
};


//--------------------------------------------------------------------------------------
// Compute Shader
//--------------------------------------------------------------------------------------
[numthreads(THREAD1D,1,1)]
void CS(uint3 threadIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID)
{
	// each thread loads one element from global to share memory
	uint tid = threadIdx.x;
	uint i = groupIdx.x * THREAD1D*2 + threadIdx.x;
	if(i+THREAD1D<cb_u4ElmCount.x) sdata0[tid] = g_idata0[i] + g_idata0[i + THREAD1D];
	else if (i<cb_u4ElmCount.x) sdata0[tid] = g_idata0[i];
	else sdata0[tid] = float4(0.f, 0.f, 0.f, 0.f);
	
	GroupMemoryBarrierWithGroupSync();

	// do reduction in shared memory
	[unroll]for(uint s=THREAD1D/2;s>32;s>>=1){
		if(tid<s){
			sdata0[tid] += sdata0[tid + s];
		}
		GroupMemoryBarrierWithGroupSync();
	}
	if(tid<32){
		sdata0[tid] += sdata0[tid + 32];

		sdata0[tid] += sdata0[tid + 16];

		sdata0[tid] += sdata0[tid + 8];

		sdata0[tid] += sdata0[tid + 4];

		sdata0[tid] += sdata0[tid + 2];

		sdata0[tid] += sdata0[tid + 1];
	}
	if(tid==0){
		g_idata0[groupIdx.x] = sdata0[0];
	}
}