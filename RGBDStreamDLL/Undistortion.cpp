#include "Undistortion.h"


HRESULT Undistortion::CompileFormString(string code,
												const D3D_SHADER_MACRO* pDefines,
												LPCSTR pEntrypoint, LPCSTR pTarget,
												UINT Flags1, UINT Flags2,
												ID3DBlob** ppCode){
	HRESULT hr;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	Flags1 |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompile(code.c_str(), code.size(), NULL, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, pEntrypoint, pTarget, Flags1, Flags2, ppCode, &pErrorBlob);
#pragma warning( suppress : 6102 )
	if (pErrorBlob)
	{
		OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
		pErrorBlob->Release();
	}

	return hr;
};

void Undistortion::GenerateShaderStr(bool hasColor)
{
	stringstream shaderCode;
	shaderCode << "Texture2D" << (hasColor ? "<float4>" : "<uint>") << " txInput : register( t0 );\n";
	shaderCode << "\n";
	shaderCode << "cbuffer perOnce : register( b0 )\n";
	shaderCode << "{ \n";
	shaderCode << "	float4 k;\n";
	shaderCode << "	float2 p;\n";
	shaderCode << "	float2 f;\n";
	shaderCode << "	float2 c;\n";
	shaderCode << "	uint2 reso;\n";
	shaderCode << "} \n";


	shaderCode << "struct GS_INPUT{};\n";
	shaderCode << "struct PS_INPUT{   \n";
	shaderCode << "	float4 Pos : SV_POSITION;\n";
	shaderCode << "	float2 Tex : TEXCOORD0;   \n";
	shaderCode << "};\n";
	shaderCode << "GS_INPUT VS(){   \n";
	shaderCode << "	GS_INPUT output = (GS_INPUT)0;   \n";
	shaderCode << "	return output;   \n";
	shaderCode << "}\n";
	shaderCode << "[maxvertexcount(4)]\n";
	shaderCode << "void GS(point GS_INPUT particles[1], inout TriangleStream<PS_INPUT> triStream){   \n";
	shaderCode << "	PS_INPUT output;\n";
	shaderCode << "	output.Pos = float4(-1.0f, 1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = float2(0,0);\n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "	output.Pos = float4(1.0f, 1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = float2(reso.x,0);   \n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "	output.Pos = float4(-1.0f, -1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = float2(0, reso.y);   \n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "	output.Pos = float4(1.0f, -1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = reso;\n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "}\n";
	shaderCode << (hasColor ? "float4" : "uint") << " PS(PS_INPUT input) : SV_Target{ \n";
	shaderCode << "	float2 idx = (input.Tex - c)/f;   \n";
	shaderCode << "   \n";
	shaderCode << "	float r2 = dot(idx, idx);\n";
	shaderCode << "	float r4 = r2 * r2;\n";
	shaderCode << "	float r6 = r2 * r4;\n";
	shaderCode << "   \n";
	shaderCode << "	float2 nidx = idx*(1.f + k.x*r2 + k.y*r4 +k.z*r6) + 2.f*p*idx.x*idx.y + p.yx*(r2 + 2.f*idx*idx); \n";
	shaderCode << "	//nidx.x = idx.x*(1.f + k1*r2 + k2*r4 + k3*r6) + 2.f*p1*idx.x*idx.y + p2*(r2 + 2.f*idx.x*idx.x)\n";
	shaderCode << "	//nidx.y = idx.y*(1.f + k1*r2 + k2*r4 + k3*r6) + 2.f*p2*idx.x*idx.y + p1*(r2 + 2.f*idx.y*idx.y)\n";
	shaderCode << "   \n";
	shaderCode << "	nidx = nidx * f + c;   \n";
	shaderCode << "	//nidx.x = nidx.x * fx + cx;   \n";
	shaderCode << "	//nidx.y = nidx.y * fy + cy;   \n";
	shaderCode << "   \n";
	shaderCode << "	//return idx.y*65535;//txInput.Load(int3(nidx,0));\n";
	shaderCode << "	return txInput.Load(int3(nidx,0));\n";
	shaderCode << "	//return txInput.Load(int3(input.Tex,0));\n";
	shaderCode << "}\n";
	m_strShader = shaderCode.str();

}
Undistortion::Undistortion() : m_uCompileFlag(D3DCOMPILE_ENABLE_STRICTNESS)
{
}

HRESULT Undistortion::CreateResource(ID3D11Device* pd3dDevice,
					   ID3D11ShaderResourceView** ppInputSRV,
					   UINT inputWidth, UINT inputHeight, bool hasColor,
					   XMFLOAT4 _k, XMFLOAT2 _p, XMFLOAT2 _f, XMFLOAT2 _c)
{
	m_ppInputSRV = ppInputSRV;
	m_uInputWidth = inputWidth;
	m_uInputHeight = inputHeight;

	m_bHasColor = hasColor;

	GenerateShaderStr(hasColor);

	HRESULT hr = S_OK;

	ID3DBlob* pVSBlob = NULL;
	V_RETURN(CompileFormString(m_strShader, nullptr, "VS", "vs_5_0", m_uCompileFlag, 0, &pVSBlob));
	V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pPassVS));
	DXUT_SetDebugName(m_pPassVS, "m_pPassVS");

	ID3DBlob* pGSBlob = NULL;
	V_RETURN(CompileFormString(m_strShader, nullptr, "GS", "gs_5_0", m_uCompileFlag, 0, &pGSBlob));
	V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pQuadGS));
	DXUT_SetDebugName(m_pQuadGS, "m_pQuadGS");
	pGSBlob->Release();

	ID3DBlob* pPSBlob = NULL;
	V_RETURN(CompileFormString(m_strShader, nullptr, "PS", "ps_5_0", m_uCompileFlag, 0, &pPSBlob));
	V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pUndistortPS));
	DXUT_SetDebugName(m_pUndistortPS, "m_pUndistortPS");
	pPSBlob->Release();

	D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
	V_RETURN(pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pPassIL));
	DXUT_SetDebugName(m_pPassIL, "m_pPassIL");
	pVSBlob->Release();

	// Create the vertex buffer
	D3D11_BUFFER_DESC bd = { 0 };
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(short);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pPassVB));
	DXUT_SetDebugName(m_pPassVB, "m_pPassVB");

	// Setup constant buffers
	D3D11_BUFFER_DESC Desc = {0};
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.ByteWidth = sizeof(CB_once);

	CB_once CBonce;
	CBonce.k = _k;
	CBonce.p = _p;
	CBonce.f = _f;
	CBonce.c = _c;
	CBonce.reso = XMUINT2(inputWidth,inputHeight);

	D3D11_SUBRESOURCE_DATA cbOnce = {0};
	cbOnce.pSysMem = &CBonce;
	V_RETURN(pd3dDevice->CreateBuffer(&Desc, &cbOnce, &m_pCBonce));
	DXUT_SetDebugName(m_pCBonce, "m_pCBonce");

	V_RETURN(Resize(pd3dDevice,inputWidth,inputHeight));

	return hr;
}


void Undistortion::Destory()
{
	Release();
	SAFE_RELEASE(m_pPassVS);
	SAFE_RELEASE(m_pQuadGS);
	SAFE_RELEASE(m_pUndistortPS);
	SAFE_RELEASE(m_pPassIL);
	SAFE_RELEASE(m_pPassVB);
	SAFE_RELEASE(m_pCBonce);
}

HRESULT Undistortion::Resize(ID3D11Device* pd3dDevice, int width, int height)
{
	HRESULT hr;

	//Create rendertaget resource
	D3D11_TEXTURE2D_DESC	RTtextureDesc = { 0 };
	RTtextureDesc.Width = width;
	RTtextureDesc.Height = height;
	RTtextureDesc.MipLevels = 1;
	RTtextureDesc.ArraySize = 1;
	if(m_bHasColor) RTtextureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	else RTtextureDesc.Format = DXGI_FORMAT_R16_UINT;
	RTtextureDesc.SampleDesc.Count = 1;
	RTtextureDesc.Usage = D3D11_USAGE_DEFAULT;
	RTtextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	RTtextureDesc.CPUAccessFlags = 0;
	RTtextureDesc.MiscFlags = 0;

	V_RETURN(pd3dDevice->CreateTexture2D(&RTtextureDesc, NULL, &m_pOutTex));
	DXUT_SetDebugName(m_pOutTex, "m_pOutTex");
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pOutTex, NULL, &m_pOutSRV));
	DXUT_SetDebugName(m_pOutSRV, "m_pOutSRV");
	V_RETURN(pd3dDevice->CreateRenderTargetView(m_pOutTex, NULL, &m_pOutRTV));
	DXUT_SetDebugName(m_pOutRTV, "m_pOutRTV");

	// Setup the view port
	m_RTviewport.Width = (float)width;
	m_RTviewport.Height = (float)height;
	m_RTviewport.MinDepth = 0.0f;
	m_RTviewport.MaxDepth = 1.0f;
	m_RTviewport.TopLeftX = 0;
	m_RTviewport.TopLeftY = 0;

	return hr = S_OK;
}

void Undistortion::Release()
{
	SAFE_RELEASE(m_pOutTex);
	SAFE_RELEASE(m_pOutSRV);
	SAFE_RELEASE(m_pOutRTV);
}

void Undistortion::Update(float fElapsedTime)
{
}

void Undistortion::Render(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Setup the pipeline
	pd3dImmediateContext->IASetInputLayout(m_pPassIL);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	UINT stride = 0;
	UINT offset = 0;
	pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pPassVB, &stride, &offset);
	pd3dImmediateContext->VSSetShader(m_pPassVS, nullptr, 0);
	pd3dImmediateContext->GSSetShader(m_pQuadGS, nullptr, 0);
	pd3dImmediateContext->PSSetShader(m_pUndistortPS, nullptr, 0);
	pd3dImmediateContext->OMSetRenderTargets(1, &m_pOutRTV, nullptr);
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pCBonce);
	pd3dImmediateContext->GSSetConstantBuffers(0, 1, &m_pCBonce);
	pd3dImmediateContext->PSSetShaderResources(0, 1, m_ppInputSRV);
	pd3dImmediateContext->RSSetViewports(1, &m_RTviewport);

	// Draw
	pd3dImmediateContext->Draw(1, 0);
}

Undistortion::~Undistortion()
{
}
