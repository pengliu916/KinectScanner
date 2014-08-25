#include "PointCloudVisualizer.h"
#include <DirectXMath.h>

HRESULT PointCloudVisualizer::CompileFormString(string code,
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

void PointCloudVisualizer::GenerateShaderStr(bool hasColor)
{
	stringstream shaderCode;
	shaderCode << "Texture2D" << (hasColor ? "<float4>" : "<uint>") << " txInput : register( t0 );\n";
	shaderCode << "\n";
	shaderCode << "cbuffer perCall : register( b0 )\n";
	shaderCode << "{ \n";
	shaderCode << "	uint2 inputReso; \n";
	shaderCode << "	float2 fLength; // in pixel unit \n";
	shaderCode << "	float2 principalPt;\n";
	shaderCode << "	float2 xz_offset;\n";
	shaderCode << "} \n";
	shaderCode << "\n";
	shaderCode << "cbuffer perFrame : register( b1 ) \n";
	shaderCode << "{ \n";
	shaderCode << "	matrix worldViewProj;\n";
	shaderCode << "} \n";
	shaderCode << "\n";
	shaderCode << "struct GS_INPUT{};\n";
	shaderCode << "struct PS_INPUT \n";
	shaderCode << "{ \n";
	shaderCode << "	float4 Pos : SV_POSITION;\n";
	shaderCode << "	float4 Col : COLOR;\n";
	shaderCode << "};\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "// Vertex Shader \n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "GS_INPUT VS(){   \n";
	shaderCode << "	GS_INPUT output = (GS_INPUT)0;   \n";
	shaderCode << "	return output;   \n";
	shaderCode << "}\n";

	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "// Geometry Shader\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "[maxvertexcount(1)]\n";
	shaderCode << "void GS(point GS_INPUT particles[1], uint primID : SV_PrimitiveID, inout PointStream<PS_INPUT> triStream){   \n";
	shaderCode << "	PS_INPUT output;\n";
	shaderCode << "	int3 idx = int3(primID % inputReso.x, primID / inputReso.x,0); \n";
	if (hasColor){
		shaderCode << "	float4 rgbd = txInput.Load(idx); \n";
		shaderCode << "	float4 pos = float4( idx.x, idx.y, rgbd.w, 1); \n";
		shaderCode << "	float hasColor = dot(rgbd.xyz,rgbd.xyz);\n";
		shaderCode << "	output.Col = float4(rgbd.xyz, 1);\n";
		shaderCode << "	if(hasColor<0.001) output.Col = float4(0.2,0.2,0.2, 1);\n";
	} else{
		shaderCode << "	uint depth = txInput.Load(idx);\n";
		shaderCode << "	float4 pos = float4( idx.x, idx.y, depth / 1000.f, 1); \n";
		shaderCode << "	output.Col = float4(1,1,1, 1); \n";
	}
	shaderCode << "	if(abs(pos.z+1)<0.001) return; \n";
	shaderCode << "	pos.xy = (pos.xy - principalPt) / fLength * pos.z; \n";
	shaderCode << "	pos.xz += xz_offset; \n";
	shaderCode << "	pos = mul(pos,worldViewProj);\n";
	shaderCode << "	output.Pos = pos;\n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "}\n";

	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "// Pixel Shader \n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "float4 PS(PS_INPUT input) : SV_Target \n";
	shaderCode << "{ \n";
	shaderCode << "	return input.Col;\n";
	shaderCode << "} \n";
	shaderCode << "\n";

	m_strShader = shaderCode.str();

}
PointCloudVisualizer::PointCloudVisualizer() : m_uCompileFlag(D3DCOMPILE_ENABLE_STRICTNESS)
{
	m_fObjectRadius = 2.f;
	m_pOutSRV = NULL;
}

HRESULT PointCloudVisualizer::CreateResource(ID3D11Device* pd3dDevice, 
											 ID3D11ShaderResourceView** ppInputSRV,
											 UINT inputWidth,UINT inputHeight, bool hasColor,
											 XMFLOAT2 _fLength, XMFLOAT2 _principalPt)
{
	m_ppInputSRV = ppInputSRV;
	m_uInputWidth = inputWidth;
	m_uInputHeight = inputHeight;

	GenerateShaderStr(hasColor);

	HRESULT hr = S_OK;

	ID3DBlob* pVSBlob = NULL;
	V_RETURN(CompileFormString(m_strShader, nullptr, "VS", "vs_5_0", m_uCompileFlag, 0, &pVSBlob));
	V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pPointCloudVS));
	DXUT_SetDebugName(m_pPointCloudVS, "m_pPointCloudVS");
	
	ID3DBlob* pGSBlob = NULL;
	V_RETURN(CompileFormString(m_strShader, nullptr, "GS", "gs_5_0", m_uCompileFlag, 0, &pGSBlob));
	V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pPointCloudGS));
	DXUT_SetDebugName(m_pPointCloudGS, "m_pPointCloudGS");
	pGSBlob->Release();

	ID3DBlob* pPSBlob = NULL;
	V_RETURN(CompileFormString(m_strShader, nullptr, "PS", "ps_5_0", m_uCompileFlag, 0, &pPSBlob));
	V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPointCloudPS));
	DXUT_SetDebugName(m_pPointCloudPS, "m_pPointCloudPS");
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
	D3D11_BUFFER_DESC Desc;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = 0;

	Desc.ByteWidth = sizeof(CB_perCall);
	V_RETURN(pd3dDevice->CreateBuffer(&Desc, NULL, &m_pCBperCall));
	DXUT_SetDebugName(m_pCBperCall, "m_pCBperCall");

	Desc.ByteWidth = sizeof(CB_perFrame);
	V_RETURN(pd3dDevice->CreateBuffer(&Desc, NULL, &m_pCBperFrame));
	DXUT_SetDebugName(m_pCBperFrame, "m_pCBperFrame");

	// Setup virtual camera params
	// Setup the camera's view parameters
	XMVECTORF32 vecEye = {0.0f, 0.0f, -100.0f};
	XMVECTORF32 vecAt = {0.0f, 0.0f, -0.0f};
	m_Camera.SetViewParams(vecEye, vecAt);
	m_Camera.SetRadius(m_fObjectRadius * 3.0f, m_fObjectRadius * 0.01f, m_fObjectRadius * 10.0f);
	
	ID3D11DeviceContext* pd3dImmediateContext;
	pd3dDevice->GetImmediateContext(&pd3dImmediateContext);
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	pd3dImmediateContext->Map(m_pCBperCall, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	CB_perCall* pPerCall = (CB_perCall*)mappedResource.pData;
	pPerCall->reso = XMUINT2(m_uInputWidth,m_uInputHeight);
	pPerCall->fLength = _fLength;
	pPerCall->principalPt = _principalPt;
	pPerCall->xz_offset	 = XMFLOAT2(0,-3);
	pd3dImmediateContext->Unmap(m_pCBperCall, 0);
	SAFE_RELEASE(pd3dImmediateContext);
	return hr;
}

void PointCloudVisualizer::Destory()
{
	SAFE_RELEASE(m_pPointCloudVS);
	SAFE_RELEASE(m_pPointCloudGS);
	SAFE_RELEASE(m_pPointCloudPS);
	SAFE_RELEASE(m_pPassIL);
	SAFE_RELEASE(m_pPassVB);
	SAFE_RELEASE(m_pCBperCall);
	SAFE_RELEASE(m_pCBperFrame);
}

HRESULT PointCloudVisualizer::Resize( ID3D11Device* pd3dDevice, int width, int height)
{
	HRESULT hr;
	m_uRTWidth = width;
	m_uRTHeight = height;

	//Create rendertaget resource
	D3D11_TEXTURE2D_DESC	RTtextureDesc = { 0 };
	RTtextureDesc.Width = m_uRTWidth;
	RTtextureDesc.Height = m_uRTHeight;
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

	// Create depth stencil
	//Create DepthStencil buffer and view
	D3D11_TEXTURE2D_DESC descDepth = {0};
	descDepth.Width = m_uRTWidth;
	descDepth.Height = m_uRTHeight;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D16_UNORM;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateTexture2D(&descDepth, NULL, &m_pOutputStencilTexture2D));
	DXUT_SetDebugName(m_pOutputStencilTexture2D, "m_pOutputStencilTexture2D");
	V_RETURN(pd3dDevice->CreateDepthStencilView(m_pOutputStencilTexture2D, NULL, &m_pOutputStencilView));  // [out] Depth stencil view
	DXUT_SetDebugName(m_pOutputStencilView, "m_pOutputStencilView");

	// Setup the view port
	m_RTviewport.Width = (float)m_uRTWidth;
	m_RTviewport.Height = (float)m_uRTHeight;
	m_RTviewport.MinDepth = 0.0f;
	m_RTviewport.MaxDepth = 1.0f;
	m_RTviewport.TopLeftX = 0;
	m_RTviewport.TopLeftY = 0;

	// Setup the camera's projection parameters
	float fAspectRatio = width / (FLOAT)height;
	m_Camera.SetProjParams(XM_PI / 4, fAspectRatio, 0.1f, 1000.0f);
	m_Camera.SetWindow(width, height);
	m_Camera.SetButtonMasks(MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON);

	return hr = S_OK;
}

void PointCloudVisualizer::Release()
{
	SAFE_RELEASE(m_pOutTex);
	SAFE_RELEASE(m_pOutSRV);
	SAFE_RELEASE(m_pOutRTV);
	SAFE_RELEASE(m_pOutputStencilTexture2D);
	SAFE_RELEASE(m_pOutputStencilView);
}

void PointCloudVisualizer::Update( float fElapsedTime )
{
	m_Camera.FrameMove(fElapsedTime);
}

void PointCloudVisualizer::Render(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Clear the render target
	pd3dImmediateContext->ClearRenderTargetView(m_pOutRTV, DirectX::Colors::Black);
	pd3dImmediateContext->ClearDepthStencilView(m_pOutputStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Update the constant buffer
	XMMATRIX m_Proj = m_Camera.GetProjMatrix();
	XMMATRIX m_View = m_Camera.GetViewMatrix();
	XMMATRIX m_World = m_Camera.GetWorldMatrix();
	XMMATRIX m_WorldViewProjection = m_World*m_View*m_Proj;

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	pd3dImmediateContext->Map(m_pCBperFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	CB_perFrame* pPerFrame = (CB_perFrame*)mappedResource.pData;
	pPerFrame->mWorldViewProj = XMMatrixTranspose(m_WorldViewProjection);
	pd3dImmediateContext->Unmap(m_pCBperFrame, 0);

	// Setup the pipeline
	pd3dImmediateContext->IASetInputLayout(m_pPassIL);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	UINT stride = 0;
	UINT offset = 0;
	pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pPassVB, &stride, &offset);
	pd3dImmediateContext->VSSetShader(m_pPointCloudVS, nullptr,0);
	pd3dImmediateContext->GSSetShader(m_pPointCloudGS, nullptr, 0);
	pd3dImmediateContext->PSSetShader(m_pPointCloudPS, nullptr,0);
	//pd3dImmediateContext->OMSetRenderTargets(1, &m_pOutRTV, NULL);
	pd3dImmediateContext->OMSetRenderTargets(1, &m_pOutRTV, m_pOutputStencilView);
	pd3dImmediateContext->GSSetConstantBuffers(0, 1, &m_pCBperCall);
	pd3dImmediateContext->GSSetConstantBuffers(1, 1, &m_pCBperFrame);
	pd3dImmediateContext->GSSetShaderResources(0, 1, m_ppInputSRV);
	pd3dImmediateContext->RSSetViewports(1, &m_RTviewport);

	// Draw
	pd3dImmediateContext->Draw(m_uInputHeight*m_uInputWidth, 0);
}
LRESULT PointCloudVisualizer::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	m_Camera.HandleMessages(hWnd, uMsg, wParam, lParam);
	return 0;
}
PointCloudVisualizer::~PointCloudVisualizer()
{
}
