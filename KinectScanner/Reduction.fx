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
RWStructuredBuffer<float4> g_idata1 : register(u1);
RWStructuredBuffer<float4> g_idata2 : register(u2);
RWStructuredBuffer<float4> g_idata3 : register(u3);
RWStructuredBuffer<float4> g_idata4 : register(u4);
RWStructuredBuffer<float4> g_idata5 : register(u5);
RWStructuredBuffer<float4> g_idata6 : register(u6);

groupshared float4 sdata0[THREAD1D];
groupshared float4 sdata1[THREAD1D];
groupshared float4 sdata2[THREAD1D];
groupshared float4 sdata3[THREAD1D];
groupshared float4 sdata4[THREAD1D];
groupshared float4 sdata5[THREAD1D];
groupshared float4 sdata6[THREAD1D];

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
	if(i+THREAD1D<cb_u4ElmCount.x){
		sdata0[tid] = g_idata0[i] + g_idata0[i + THREAD1D];
		sdata1[tid] = g_idata1[i] + g_idata1[i + THREAD1D];
		sdata2[tid] = g_idata2[i] + g_idata2[i + THREAD1D];
		sdata3[tid] = g_idata3[i] + g_idata3[i + THREAD1D];
		sdata4[tid] = g_idata4[i] + g_idata4[i + THREAD1D];
		sdata5[tid] = g_idata5[i] + g_idata5[i + THREAD1D];
		sdata6[tid] = g_idata6[i] + g_idata6[i + THREAD1D];
	} else if (i<cb_u4ElmCount.x){
		sdata0[tid] = g_idata0[i];
		sdata1[tid] = g_idata1[i];
		sdata2[tid] = g_idata2[i];
		sdata3[tid] = g_idata3[i];
		sdata4[tid] = g_idata4[i];
		sdata5[tid] = g_idata5[i];
		sdata6[tid] = g_idata6[i];
	} else{	 
		sdata0[tid] = float4(0.f, 0.f, 0.f, 0.f);
		sdata1[tid] = float4(0.f, 0.f, 0.f, 0.f);
		sdata2[tid] = float4(0.f, 0.f, 0.f, 0.f);
		sdata3[tid] = float4(0.f, 0.f, 0.f, 0.f);
		sdata4[tid] = float4(0.f, 0.f, 0.f, 0.f);
		sdata5[tid] = float4(0.f, 0.f, 0.f, 0.f);
		sdata6[tid] = float4(0.f, 0.f, 0.f, 0.f);
	}
	GroupMemoryBarrierWithGroupSync();

	// do reduction in shared memory
	[unroll]for(uint s=THREAD1D/2;s>32;s>>=1){
		if(tid<s){
			sdata0[tid] += sdata0[tid + s];
			sdata1[tid] += sdata1[tid + s];
			sdata2[tid] += sdata2[tid + s];
			sdata3[tid] += sdata3[tid + s];
			sdata4[tid] += sdata4[tid + s];
			sdata5[tid] += sdata5[tid + s];
			sdata6[tid] += sdata6[tid + s];
		}
		GroupMemoryBarrierWithGroupSync();
	}
	if(tid<32){
		sdata0[tid] += sdata0[tid + 32];
		sdata1[tid] += sdata1[tid + 32];
		sdata2[tid] += sdata2[tid + 32];
		sdata3[tid] += sdata3[tid + 32];
		sdata4[tid] += sdata4[tid + 32];
		sdata5[tid] += sdata5[tid + 32];
		sdata6[tid] += sdata6[tid + 32];

		sdata0[tid] += sdata0[tid + 16];
		sdata1[tid] += sdata1[tid + 16];
		sdata2[tid] += sdata2[tid + 16];
		sdata3[tid] += sdata3[tid + 16];
		sdata4[tid] += sdata4[tid + 16];
		sdata5[tid] += sdata5[tid + 16];
		sdata6[tid] += sdata6[tid + 16];

		sdata0[tid] += sdata0[tid + 8];
		sdata1[tid] += sdata1[tid + 8];
		sdata2[tid] += sdata2[tid + 8];
		sdata3[tid] += sdata3[tid + 8];
		sdata4[tid] += sdata4[tid + 8];
		sdata5[tid] += sdata5[tid + 8];
		sdata6[tid] += sdata6[tid + 8];

		sdata0[tid] += sdata0[tid + 4];
		sdata1[tid] += sdata1[tid + 4];
		sdata2[tid] += sdata2[tid + 4];
		sdata3[tid] += sdata3[tid + 4];
		sdata4[tid] += sdata4[tid + 4];
		sdata5[tid] += sdata5[tid + 4];
		sdata6[tid] += sdata6[tid + 4];

		sdata0[tid] += sdata0[tid + 2];
		sdata1[tid] += sdata1[tid + 2];
		sdata2[tid] += sdata2[tid + 2];
		sdata3[tid] += sdata3[tid + 2];
		sdata4[tid] += sdata4[tid + 2];
		sdata5[tid] += sdata5[tid + 2];
		sdata6[tid] += sdata6[tid + 2];

		sdata0[tid] += sdata0[tid + 1];
		sdata1[tid] += sdata1[tid + 1];
		sdata2[tid] += sdata2[tid + 1];
		sdata3[tid] += sdata3[tid + 1];
		sdata4[tid] += sdata4[tid + 1];
		sdata5[tid] += sdata5[tid + 1];
		sdata6[tid] += sdata6[tid + 1];
	}
	if(tid==0){
		g_idata0[groupIdx.x] = sdata0[0];
		g_idata1[groupIdx.x] = sdata1[0];
		g_idata2[groupIdx.x] = sdata2[0];
		g_idata3[groupIdx.x] = sdata3[0];
		g_idata4[groupIdx.x] = sdata4[0];
		g_idata5[groupIdx.x] = sdata5[0];
		g_idata6[groupIdx.x] = sdata6[0];
	}
}