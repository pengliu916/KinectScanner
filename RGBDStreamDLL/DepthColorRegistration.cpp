#include "DepthColorRegistration.h"


HRESULT DepthColorRegistration::CompileFormString(string code,
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

void DepthColorRegistration::GenerateShaderStr()
{
	stringstream shaderCode;
	shaderCode << "Texture2D<uint> txDepth : register( t0 );\n";
	shaderCode << "Texture2D<float4> txColor : register( t1 );\n";
	shaderCode << "\n";
	shaderCode << "cbuffer perOnce : register( b0 )\n";
	shaderCode << "{ \n";
	shaderCode << "	float2 f0;\n";
	shaderCode << "	float2 c0;\n";
	shaderCode << "	uint2 reso0;\n";
	shaderCode << "	float2 f1;\n";
	shaderCode << "	float2 c1;\n";
	shaderCode << "	uint2 reso1;\n";
	shaderCode << "	matrix e;\n";
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
	shaderCode << "	output.Tex = float2(reso0.x,0);   \n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "	output.Pos = float4(-1.0f, -1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = float2(0, reso0.y);   \n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "	output.Pos = float4(1.0f, -1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = reso0;\n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "}\n";
	shaderCode << "float4 PS(PS_INPUT input) : SV_Target{ \n";
	shaderCode << "	float z = txDepth.Load(int3(input.Tex,0))/1000.f; // convert to meters\n";
	shaderCode << "	if( z<0.2 || z>8) return float4(0,0,0,0);\n";
	shaderCode << "	float2 xy = (input.Tex - c0) / f0 * z;\n";
	shaderCode << " float4 pos_inDCam = float4( xy, z, 1.f );\n";
	shaderCode << "	float4 pos_inRGBCam = mul( pos_inDCam, e );\n";
	shaderCode << "	float2 uv = pos_inRGBCam.xy / pos_inRGBCam.z * f1 + c1;\n";
	shaderCode << "	float4 col = txColor.Load(int3(uv,0));\n";
	shaderCode << "	col.w = z;\n";
	shaderCode << "	return col;\n";
	shaderCode << "}\n";
	m_strShader = shaderCode.str();

}
DepthColorRegistration::DepthColorRegistration() : m_uCompileFlag(D3DCOMPILE_ENABLE_STRICTNESS)
{
}

HRESULT DepthColorRegistration::CreateResource(ID3D11Device* pd3dDevice,
											   ID3D11ShaderResourceView** ppDepthSRV, XMUINT2 _reso0, XMFLOAT2 _f0, XMFLOAT2 _c0,
											   ID3D11ShaderResourceView** ppColorSRV, XMUINT2 _reso1, XMFLOAT2 _f1, XMFLOAT2 _c1,
											   XMMATRIX _e)
{
	m_ppInDepthSRV = ppDepthSRV;
	m_ppInColorSRV = ppColorSRV;

	GenerateShaderStr();

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
	V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pRegistrationPS));
	DXUT_SetDebugName(m_pRegistrationPS, "m_pRegistrationPS");
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
	D3D11_BUFFER_DESC Desc = { 0 };
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.ByteWidth = sizeof(CB_once);

	CB_once CBonce;
	CBonce.f0 = _f0;
	CBonce.c0 = _c0;
	CBonce.reso0 = _reso0;
	CBonce.f1 = _f1;
	CBonce.c1 = _c1;
	CBonce.reso1 = _reso1;
	//CBonce.e = XMMatrixTranspose(XMMatrixInverse(NULL, _e));
	CBonce.e = XMMatrixTranspose( _e);

	D3D11_SUBRESOURCE_DATA cbOnce = { 0 };
	cbOnce.pSysMem = &CBonce;
	V_RETURN(pd3dDevice->CreateBuffer(&Desc, &cbOnce, &m_pCBonce));
	DXUT_SetDebugName(m_pCBonce, "m_pCBonce");

	V_RETURN(Resize(pd3dDevice, _reso0.x, _reso0.y));

	return hr;
}


void DepthColorRegistration::Destory()
{
	Release();
	SAFE_RELEASE(m_pPassVS);
	SAFE_RELEASE(m_pQuadGS);
	SAFE_RELEASE(m_pRegistrationPS);
	SAFE_RELEASE(m_pPassIL);
	SAFE_RELEASE(m_pPassVB);
	SAFE_RELEASE(m_pCBonce);
}

HRESULT DepthColorRegistration::Resize(ID3D11Device* pd3dDevice, int width, int height)
{
	HRESULT hr;

	//Create rendertaget resource
	D3D11_TEXTURE2D_DESC	RTtextureDesc = { 0 };
	RTtextureDesc.Width = width;
	RTtextureDesc.Height = height;
	RTtextureDesc.MipLevels = 1;
	RTtextureDesc.ArraySize = 1;
	RTtextureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
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

void DepthColorRegistration::Release()
{
	SAFE_RELEASE(m_pOutTex);
	SAFE_RELEASE(m_pOutSRV);
	SAFE_RELEASE(m_pOutRTV);
}

void DepthColorRegistration::Update(float fElapsedTime)
{
}

void DepthColorRegistration::Render(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Setup the pipeline
	pd3dImmediateContext->IASetInputLayout(m_pPassIL);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	UINT stride = 0;
	UINT offset = 0;
	pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pPassVB, &stride, &offset);
	pd3dImmediateContext->VSSetShader(m_pPassVS, nullptr, 0);
	pd3dImmediateContext->GSSetShader(m_pQuadGS, nullptr, 0);
	pd3dImmediateContext->PSSetShader(m_pRegistrationPS, nullptr, 0);
	pd3dImmediateContext->OMSetRenderTargets(1, &m_pOutRTV, nullptr);
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pCBonce);
	pd3dImmediateContext->GSSetConstantBuffers(0, 1, &m_pCBonce);
	pd3dImmediateContext->PSSetShaderResources(0, 1, m_ppInDepthSRV);
	pd3dImmediateContext->PSSetShaderResources(1, 1, m_ppInColorSRV);
	pd3dImmediateContext->RSSetViewports(1, &m_RTviewport);

	// Draw
	pd3dImmediateContext->Draw(1, 0);
}

DepthColorRegistration::~DepthColorRegistration()
{
}
