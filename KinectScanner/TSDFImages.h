// Need to be improved
#pragma once
#include <D3D11.h>
#include <DirectXMath.h>
#include "DXUT.h"
#include "DXUTcamera.h"

#include "header.h"

#include "VolumeTSDF.h"

struct CB_TSDFImg_PerCall
{
	float TruncDist;
	XMFLOAT3 VolumeHalfSize;
	float Tstep;
	XMFLOAT3 InvVolumeSize;
	XMFLOAT3 BoxMin;
	bool KinectShade;
	XMFLOAT3 BoxMax;
	float NIU1;
	XMFLOAT2 RTreso;
	XMFLOAT2 NIU2;
};

struct m_CB_TSDFImg_KinectPerFrame
{
	XMFLOAT4 KinectPos;
	XMMATRIX InvKinectTransform;
	XMMATRIX KinectTransform;
	XMMATRIX KinectViewProj;
};

struct m_CB_TSDFImg_FreecamPerFrame
{
	XMMATRIX InvView;
	XMFLOAT4 ViewPos;
};

class TSDFImages
{
public:
#if DEBUG
	ID3D11GeometryShader*			m_pDebug_CellGS;
	ID3D11PixelShader*				m_pDebug_CellPS;
	ID3D11Texture2D*				m_pDebug_CellTex;
	ID3D11ShaderResourceView*		m_pDebug_CellSRV;
	ID3D11RenderTargetView*			m_pDebug_CellRTV;

	ID3D11Texture2D*				m_pDebug_DSTex;
	ID3D11DepthStencilView*			m_pDebug_DSSV;
#endif
	CModelViewerCamera				m_cCamera;
	D3D11_VIEWPORT					m_cKinectViewport;

	ID3D11VertexShader*				m_pPassVS;
	ID3D11GeometryShader*			m_pActiveCellAndSOGS;
	ID3D11GeometryShader*			m_pRaymarchGS;
	ID3D11PixelShader*				m_pFarNearPS;
	ID3D11PixelShader*				m_pRayCastingPS;
	ID3D11Buffer*					m_pOutVB;
	ID3D11Buffer*					m_pPassVB;
	ID3D11InputLayout*				m_pOutVL;

	ID3D11RasterizerState*			m_pBackFaceRS;// Output rasterizer state
	ID3D11RasterizerState*			m_pFarNearRS;// Output rasterizer state

	ID3D11BlendState*				m_pBlendState;

	ID3D11Texture2D*				m_pKinectOutTex[3]; // 0-2 depth, normal, shaded
	ID3D11ShaderResourceView*		m_pKinectOutSRV[3];
	ID3D11RenderTargetView*			m_pKinectOutRTV[3];

	ID3D11Texture2D*				m_pFarNearTex;
	ID3D11ShaderResourceView*		m_pFarNearSRV;
	ID3D11RenderTargetView*			m_pFarNearRTV;

	ID3D11SamplerState*				m_pGenSampler;

	CB_TSDFImg_PerCall				m_CBperCall;
	ID3D11Buffer*					m_pCBperCall;

	m_CB_TSDFImg_KinectPerFrame		m_CB_KinectPerFrame;
	ID3D11Buffer*					m_pCB_KinectPerFrame;

	m_CB_TSDFImg_FreecamPerFrame	m_CB_FreecamPerFrame;
	ID3D11Buffer*					m_pCB_FreecamPerFrame;

	XMMATRIX						m_mKinectProj;
	VolumeTSDF*						m_pTSDFVolume;

	bool							m_bEmptyTSDF;

	TransformedPointClould*			m_pGeneratedTPC;

	TSDFImages( VolumeTSDF* pTSDFVolume, UINT RTwidth = SUB_TEXTUREWIDTH, UINT RTheight = SUB_TEXTUREHEIGHT, bool ShadeFromKinect = false )
	{
		m_pTSDFVolume = pTSDFVolume;
		m_CBperCall.TruncDist = pTSDFVolume->m_CBperCall.truncDist;
		m_CBperCall.VolumeHalfSize.x = pTSDFVolume->m_CBperCall.halfWidth * pTSDFVolume->m_CBperCall.voxelSize ;
		m_CBperCall.VolumeHalfSize.y = pTSDFVolume->m_CBperCall.halfHeight * pTSDFVolume->m_CBperCall.voxelSize ;
		m_CBperCall.VolumeHalfSize.z = pTSDFVolume->m_CBperCall.halfDepth * pTSDFVolume->m_CBperCall.voxelSize ;
		m_CBperCall.Tstep = 0.5f * m_CBperCall.TruncDist;
		m_CBperCall.InvVolumeSize.x = 1.0f / ( m_CBperCall.VolumeHalfSize.x * 2.0f );
		m_CBperCall.InvVolumeSize.y = 1.0f / ( m_CBperCall.VolumeHalfSize.y * 2.0f );
		m_CBperCall.InvVolumeSize.z = 1.0f / ( m_CBperCall.VolumeHalfSize.z * 2.0f );
		m_CBperCall.BoxMin.x = -m_CBperCall.VolumeHalfSize.x;
		m_CBperCall.BoxMin.y = -m_CBperCall.VolumeHalfSize.y;
		m_CBperCall.BoxMin.z = -m_CBperCall.VolumeHalfSize.z;
		m_CBperCall.BoxMax.x = m_CBperCall.VolumeHalfSize.x;
		m_CBperCall.BoxMax.y = m_CBperCall.VolumeHalfSize.y;
		m_CBperCall.BoxMax.z = m_CBperCall.VolumeHalfSize.z;
		m_CBperCall.KinectShade = ShadeFromKinect;

		m_bEmptyTSDF = true;
		
		m_pGeneratedTPC = new TransformedPointClould();
		
		m_pGeneratedTPC->ppMeshRGBZTexSRV = &m_pKinectOutSRV[0];
		m_pGeneratedTPC->ppMeshNormalTexSRV = &m_pKinectOutSRV[1];

		// create the projection matrix for the kinect sensor
		float far_plane = 20.f;
		float near_plane = 0.1f;
		float q = far_plane / (far_plane - near_plane);
		XMFLOAT4X4 mat(	F_X * 2.f / D_W,0,				0,				0,
						0,				F_Y * 2.f / D_H,0,				0,
						0,				0,				q,				1,
						0,				0,				-q*near_plane,	0);
		m_mKinectProj = XMLoadFloat4x4(&mat);

	}

	HRESULT CreateResource( ID3D11Device* pd3dDevice )
	{
		HRESULT hr = S_OK;

		ID3DBlob* pVSBlob = NULL;
		DXUT_SetDebugName(m_pOutVL, "m_pOutVL");
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "VS", "vs_5_0", COMPILE_FLAG, 0, &pVSBlob));
		V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pPassVS));
		DXUT_SetDebugName(m_pPassVS, "m_pPassVS");
		D3D11_SO_DECLARATION_ENTRY outputstreamLayout[] = { { 0, "POSITION", 0, 0, 3, 0 } };
		D3D11_INPUT_ELEMENT_DESC inputLayout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
		V_RETURN(pd3dDevice->CreateInputLayout(inputLayout, ARRAYSIZE(inputLayout), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pOutVL));
		pVSBlob->Release();

		ID3DBlob* pPSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "FarNearPS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pFarNearPS));
		DXUT_SetDebugName(m_pFarNearPS, "m_pFarNearPS");
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "RaymarchPS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pRayCastingPS));
		DXUT_SetDebugName(m_pRayCastingPS, "m_pRayCastingPS");

#if DEBUG
		// debug layer, show cell grid
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "Debug_CellPS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pDebug_CellPS));
		DXUT_SetDebugName(m_pDebug_CellPS, "m_pDebug_CellPS");
#endif
		pPSBlob->Release();

		
		ID3DBlob* pGSBlob = NULL;
		// For output-stream GS
		// Streamoutput GS
		UINT stride = 3 * sizeof(float);
		UINT elems = sizeof(outputstreamLayout) / sizeof(D3D11_SO_DECLARATION_ENTRY);
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "ActiveCellAndSOGS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pActiveCellAndSOGS));
		/*V_RETURN(pd3dDevice->CreateGeometryShaderWithStreamOutput(pGSBlob->GetBufferPointer(),
			pGSBlob->GetBufferSize(), outputstreamLayout, elems, &stride, 1,
			0, NULL, &m_pActiveCellAndSOGS));*/
		DXUT_SetDebugName(m_pActiveCellAndSOGS, "m_pActiveCellAndSOGS");

		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "RaymarchGS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pRaymarchGS));
		DXUT_SetDebugName(m_pRaymarchGS, "m_pRaymarchGS");

#if DEBUG
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "Debug_CellGS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pDebug_CellGS));
		DXUT_SetDebugName(m_pDebug_CellGS, "m_pDebug_CellGS");
#endif
		pGSBlob->Release();


		D3D11_BUFFER_DESC bd;
		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(float) * 8 * VOXEL_NUM_X * VOXEL_NUM_Y * VOXEL_NUM_Z / CELLRATIO / CELLRATIO / CELLRATIO;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pOutVB));
		DXUT_SetDebugName(m_pOutVB, "m_pOutVB");
		bd.ByteWidth = sizeof(float);
		V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pPassVB));
		DXUT_SetDebugName(m_pPassVB, "m_pPassVB");

		ZeroMemory( &bd, sizeof(bd) );
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0    ;
		bd.ByteWidth = sizeof(CB_TSDFImg_PerCall);
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCBperCall ));
		DXUT_SetDebugName( m_pCBperCall, "TSDFImages_pCB_TSDFImg_PerCall");
		bd.ByteWidth = sizeof(m_CB_TSDFImg_KinectPerFrame);
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCB_KinectPerFrame ));
		DXUT_SetDebugName( m_pCB_KinectPerFrame, "TSDFImages_pm_CB_TSDFImg_KinectPerFrame");
		bd.ByteWidth = sizeof(m_CB_TSDFImg_FreecamPerFrame);
		V_RETURN(pd3dDevice->CreateBuffer( &bd, NULL, &m_pCB_FreecamPerFrame ));
		DXUT_SetDebugName( m_pCB_FreecamPerFrame, "TSDFImages_pm_CB_TSDFImg_FreecamPerFrame");

		// Create the sample state
		D3D11_SAMPLER_DESC sampDesc;
		ZeroMemory( &sampDesc, sizeof(sampDesc) );
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP       ;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP       ;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP       ;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(pd3dDevice->CreateSamplerState( &sampDesc, &m_pGenSampler ));
		DXUT_SetDebugName(m_pGenSampler, "m_pGenSampler");

		// Create resource for texture
		D3D11_TEXTURE2D_DESC	TEXDesc;
		ZeroMemory( &TEXDesc, sizeof(TEXDesc));
		TEXDesc.MipLevels = 1;
		TEXDesc.ArraySize = 1;
		TEXDesc.SampleDesc.Count = 1;
		TEXDesc.Usage = D3D11_USAGE_DEFAULT;
		TEXDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		TEXDesc.CPUAccessFlags = 0;
		TEXDesc.MiscFlags = 0;
		TEXDesc.Width = D_W;
		TEXDesc.Height = D_H;
		TEXDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pKinectOutTex[0])); DXUT_SetDebugName(m_pKinectOutTex[0], "m_pKinectOutTex[0]"); // RGBZ tex
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pKinectOutTex[1])); DXUT_SetDebugName(m_pKinectOutTex[1], "m_pKinectOutTex[1]");// Normal tex
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pKinectOutTex[2])); DXUT_SetDebugName(m_pKinectOutTex[2], "m_pKinectOutTex[2]");// Normal tex
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pFarNearTex)); DXUT_SetDebugName(m_pFarNearTex, "m_pFarNearTex");// Normal tex
#if DEBUG
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pDebug_CellTex)); DXUT_SetDebugName(m_pDebug_CellTex, "m_pDebug_CellTex");
#endif

		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pKinectOutTex[0], nullptr, &m_pKinectOutRTV[0])); DXUT_SetDebugName(m_pKinectOutRTV[0], "m_pKinectOutRTV[0]");
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pKinectOutTex[1], nullptr, &m_pKinectOutRTV[1])); DXUT_SetDebugName(m_pKinectOutRTV[1], "m_pKinectOutRTV[1]");
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pKinectOutTex[2], nullptr, &m_pKinectOutRTV[2])); DXUT_SetDebugName(m_pKinectOutRTV[2], "m_pKinectOutRTV[2]");
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pFarNearTex, nullptr, &m_pFarNearRTV)); DXUT_SetDebugName(m_pFarNearRTV, "m_pFarNearRTV");
#if DEBUG
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pDebug_CellTex, nullptr, &m_pDebug_CellRTV)); DXUT_SetDebugName(m_pDebug_CellRTV, "m_pDebug_CellRTV");
#endif

		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pKinectOutTex[0], nullptr, &m_pKinectOutSRV[0])); DXUT_SetDebugName(m_pKinectOutSRV[0], "m_pKinectOutSRV[0]");
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pKinectOutTex[1], nullptr, &m_pKinectOutSRV[1])); DXUT_SetDebugName(m_pKinectOutSRV[1], "m_pKinectOutSRV[1]");
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pKinectOutTex[2], nullptr, &m_pKinectOutSRV[2])); DXUT_SetDebugName(m_pKinectOutSRV[2], "m_pKinectOutSRV[2]");
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pFarNearTex, nullptr, &m_pFarNearSRV)); DXUT_SetDebugName(m_pFarNearSRV, "m_pFarNearSRV");
#if DEBUG
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pDebug_CellTex, nullptr, &m_pDebug_CellSRV)); DXUT_SetDebugName(m_pDebug_CellSRV, "m_pDebug_CellSRV");

		D3D11_TEXTURE2D_DESC DSDesc;
		ZeroMemory(&DSDesc,sizeof(DSDesc));
		DSDesc.MipLevels = 1;
		DSDesc.ArraySize = 1;
		DSDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		DSDesc.SampleDesc.Count = 1;
		DSDesc.Usage = D3D11_USAGE_DEFAULT;
		DSDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		DSDesc.Width = D_W;
		DSDesc.Height = D_H;
		V_RETURN(pd3dDevice->CreateTexture2D(&DSDesc,NULL,&m_pDebug_DSTex));
		DXUT_SetDebugName(m_pDebug_DSTex,"m_pDebug_DSTex");
		V_RETURN(pd3dDevice->CreateDepthStencilView(m_pDebug_DSTex,NULL,&m_pDebug_DSSV));
		DXUT_SetDebugName(m_pDebug_DSSV,"m_pDebug_DSSV");
#endif
		// rasterizer state
		D3D11_RASTERIZER_DESC rsDesc;
		rsDesc.FillMode = D3D11_FILL_WIREFRAME;
		rsDesc.CullMode = D3D11_CULL_NONE;
		rsDesc.FrontCounterClockwise = FALSE;
		rsDesc.DepthBias = 0;
		rsDesc.DepthBiasClamp = 0.0f;
		rsDesc.SlopeScaledDepthBias = 0.0f;
		rsDesc.DepthClipEnable = TRUE;
		rsDesc.ScissorEnable = FALSE;
		rsDesc.MultisampleEnable = FALSE;
		rsDesc.AntialiasedLineEnable = FALSE;
		V_RETURN(pd3dDevice->CreateRasterizerState(&rsDesc, &m_pBackFaceRS));
		DXUT_SetDebugName(m_pBackFaceRS, "m_pBackFaceRS");


		// Create the blend state
		/* use blend to achieve rendering both tFar and tNear into RT for the coming raymarching pass
		 * the alpha channel store the tFar while the red channel stores the tNear*/
		D3D11_BLEND_DESC blendDesc;
		ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;
		blendDesc.RenderTarget[0].BlendEnable = true;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;// since Red channel has tNear so we keep the Min

		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;// alpha has tFar so we keep Max
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		//blendDesc.RenderTarget[0].RenderTargetWriteMask = 0x0F ;
		V_RETURN(pd3dDevice->CreateBlendState(&blendDesc, &m_pBlendState));


		m_cKinectViewport.Width = (float)D_W;
		m_cKinectViewport.Height = (float)D_H;
		m_cKinectViewport.MinDepth = 0.0f;
		m_cKinectViewport.MaxDepth = 1.0f;
		m_cKinectViewport.TopLeftX = 0;
		m_cKinectViewport.TopLeftY = 0;

		ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
		pd3dImmediateContext->UpdateSubresource(m_pCBperCall, 0, NULL, &m_CBperCall, 0, 0);

		// Setup the camera's projection parameters
		XMVECTORF32 vecEye = { 0.0f, 0.0f, -2.0f };
		XMVECTORF32 vecAt = { 0.0f, 0.0f, -0.0f };
		m_cCamera.SetViewParams( vecEye, vecAt );
		m_cCamera.SetWindow(D_W, D_H);
		m_cCamera.SetButtonMasks(MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON);
		m_cCamera.SetRadius(2.f, 0.1f, 10.f);
		return hr;
	}

	HRESULT Resize(ID3D11Device* pd3dDevice, int iWidth, int iHeight)
	{
		HRESULT hr = S_OK;

		
		return hr;
	}

	void Destory()
	{
		SAFE_RELEASE(m_pPassVS);
		SAFE_RELEASE(m_pActiveCellAndSOGS);
		SAFE_RELEASE(m_pRaymarchGS);
		SAFE_RELEASE(m_pFarNearPS);
		SAFE_RELEASE(m_pRayCastingPS);
		SAFE_RELEASE(m_pOutVB);
		SAFE_RELEASE(m_pPassVB);
		SAFE_RELEASE(m_pOutVL);

		SAFE_RELEASE(m_pBackFaceRS);
		SAFE_RELEASE(m_pFarNearRS);

		SAFE_RELEASE(m_pBlendState);

		SAFE_RELEASE( m_pKinectOutTex[0] );
		SAFE_RELEASE( m_pKinectOutTex[1] );
		SAFE_RELEASE( m_pKinectOutTex[2] );
		SAFE_RELEASE( m_pKinectOutSRV[0] );
		SAFE_RELEASE( m_pKinectOutSRV[1] );
		SAFE_RELEASE( m_pKinectOutSRV[2] );
		SAFE_RELEASE( m_pKinectOutRTV[0] );
		SAFE_RELEASE( m_pKinectOutRTV[1] );
		SAFE_RELEASE( m_pKinectOutRTV[2] );

		SAFE_RELEASE(m_pFarNearTex);
		SAFE_RELEASE(m_pFarNearSRV);
		SAFE_RELEASE(m_pFarNearRTV);

#if DEBUG
		SAFE_RELEASE(m_pDebug_CellGS);
		SAFE_RELEASE(m_pDebug_CellPS);
		SAFE_RELEASE(m_pDebug_CellTex);
		SAFE_RELEASE(m_pDebug_CellSRV);
		SAFE_RELEASE(m_pDebug_CellRTV);

		SAFE_RELEASE(m_pDebug_DSTex);
		SAFE_RELEASE(m_pDebug_DSSV);
#endif

		SAFE_RELEASE( m_pGenSampler);
		
		SAFE_RELEASE( m_pCBperCall );
		SAFE_RELEASE( m_pCB_KinectPerFrame );
		SAFE_RELEASE( m_pCB_FreecamPerFrame );
	}

	void Release()
	{
	}

	~TSDFImages()
	{
		delete m_pGeneratedTPC;
	}

	void Update( float fElapsedTime )
	{
		m_cCamera.FrameMove( fElapsedTime );
	}

	void Get3ImgForKinect( ID3D11DeviceContext* pd3dImmediateContext )
	{
		if( m_bEmptyTSDF )
		{
			m_pTSDFVolume->Integrate(pd3dImmediateContext);
			m_bEmptyTSDF = false;
		}

		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Generate 3 img from Volume");

		// Update infor for GPU
		XMMATRIX mKinectTransform = m_pTSDFVolume->m_pInputPC->mPreFrame;
		m_pGeneratedTPC->mCurFrame = mKinectTransform;
		m_pGeneratedTPC->mCurRotation = m_pTSDFVolume->m_pInputPC->mPreRotation;
		XMVECTOR t;
		XMMATRIX view= XMMatrixInverse(&t,mKinectTransform);
		XMFLOAT4 pos = XMFLOAT4( 0, 0, 0, 1 );
		t = XMLoadFloat4( &pos );
		t = XMVector4Transform( t, mKinectTransform );
		XMStoreFloat4( &pos, t);

		// uncomment the following to allow mouth interactive kinect point of view
		/*view = m_cCamera.GetViewMatrix();
		XMStoreFloat4(&pos,m_cCamera.GetEyePt());
		mKinectTransform = XMMatrixInverse(&t,view);*/

		m_CB_KinectPerFrame.KinectTransform = XMMatrixTranspose( mKinectTransform );
		m_CB_KinectPerFrame.InvKinectTransform = XMMatrixTranspose( view );
		m_CB_KinectPerFrame.KinectPos = pos;
		m_CB_KinectPerFrame.KinectViewProj = XMMatrixTranspose(view*m_mKinectProj);

		pd3dImmediateContext->UpdateSubresource( m_pCB_KinectPerFrame, 0, NULL, &m_CB_KinectPerFrame, 0, 0 );

		// render the near_far texture for later raymarching
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Render the tNearFar");
		// Clear the render targets and depth view
		float ClearColor[4] = { 0.0f, 0.0f, 0.0f, -1.0f };
		float ClearColor1[4] = { 50.0f, 50.0f, 50.0f, -10.0f };

		pd3dImmediateContext->ClearRenderTargetView(m_pKinectOutRTV[0], ClearColor);
		pd3dImmediateContext->ClearRenderTargetView(m_pKinectOutRTV[1], ClearColor);
		pd3dImmediateContext->ClearRenderTargetView(m_pKinectOutRTV[2], ClearColor);
		pd3dImmediateContext->ClearRenderTargetView(m_pFarNearRTV, ClearColor1);
#if DEBUG
		pd3dImmediateContext->ClearDepthStencilView(m_pDebug_DSSV, D3D11_CLEAR_DEPTH, 1.0, 0);
#endif		
		pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

		pd3dImmediateContext->OMSetRenderTargets(1, &m_pFarNearRTV, NULL);
		UINT offset[1] = { 0 };
		//pd3dImmediateContext->SOSetTargets(1, &m_pOutVB, offset);
		pd3dImmediateContext->IASetInputLayout(m_pOutVL);
		UINT stride = sizeof(short);
		pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pPassVB, &stride, offset);
		pd3dImmediateContext->RSSetViewports(1, &m_cKinectViewport);
		pd3dImmediateContext->VSSetShader(m_pPassVS, NULL, 0);
		pd3dImmediateContext->GSSetShader(m_pActiveCellAndSOGS, NULL, 0);
		pd3dImmediateContext->GSSetConstantBuffers(0, 1, &m_pCBperCall);
		pd3dImmediateContext->GSSetConstantBuffers(1, 1, &m_pCB_KinectPerFrame);
		pd3dImmediateContext->GSSetShaderResources(2, 1, &m_pTSDFVolume->m_pBrickVolSRV[0]);
		pd3dImmediateContext->GSSetShaderResources(3, 1, &m_pTSDFVolume->m_pBrickVolSRV[1]);
		pd3dImmediateContext->PSSetShader(m_pFarNearPS, NULL, 0);

		ID3D11BlendState* bs;
		FLOAT blendFactor[4];
		UINT StencilRef;
		pd3dImmediateContext->OMGetBlendState(&bs,blendFactor,&StencilRef);
		pd3dImmediateContext->OMSetBlendState(m_pBlendState,NULL,0xffffffff);

		pd3dImmediateContext->Draw(VOXEL_NUM_X * VOXEL_NUM_Y * VOXEL_NUM_Z / CELLRATIO / CELLRATIO / CELLRATIO, 0);

		pd3dImmediateContext->OMSetBlendState(bs,blendFactor,StencilRef);
		SAFE_RELEASE(bs);
		DXUT_EndPerfEvent();
		// render through raymarching
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Raymarching");
		pd3dImmediateContext->GSSetShader(m_pRaymarchGS,NULL,0);
		pd3dImmediateContext->PSSetShader(m_pRayCastingPS, NULL, 0);
#if DEBUG
		pd3dImmediateContext->OMSetRenderTargets(3,m_pKinectOutRTV,m_pDebug_DSSV);
		//pd3dImmediateContext->OMSetRenderTargets(3, m_pKinectOutRTV, NULL);
#else
		pd3dImmediateContext->OMSetRenderTargets(3, m_pKinectOutRTV,NULL);
#endif
		pd3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pCBperCall);
		pd3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pCB_KinectPerFrame);
		pd3dImmediateContext->PSSetShaderResources(0, 1, &m_pTSDFVolume->m_pDWVolumeSRV);
		pd3dImmediateContext->PSSetShaderResources(1, 1, &m_pTSDFVolume->m_pColVolumeSRV);
		pd3dImmediateContext->PSSetShaderResources(4, 1, &m_pFarNearSRV);
		pd3dImmediateContext->Draw(1,0);
		DXUT_EndPerfEvent();

#if DEBUG
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR2, L"Rendering active cell grid");
		//pd3dImmediateContext->ClearRenderTargetView(m_pDebug_CellRTV, ClearColor);
		pd3dImmediateContext->GSSetShader(m_pDebug_CellGS,NULL,0);
		pd3dImmediateContext->PSSetShader(m_pDebug_CellPS, NULL, 0);
		pd3dImmediateContext->OMSetRenderTargets(1, &m_pKinectOutRTV[2], m_pDebug_DSSV);
		//pd3dImmediateContext->OMSetRenderTargets(1, &m_pKinectOutRTV[2], NULL);
		pd3dImmediateContext->Draw(VOXEL_NUM_X * VOXEL_NUM_Y * VOXEL_NUM_Z / CELLRATIO / CELLRATIO / CELLRATIO, 0);
		/*	ID3D11RasterizerState* rs;
			pd3dImmediateContext->RSGetState(&rs);
			pd3dImmediateContext->RSSetState(m_pBackFaceRS);


			pd3dImmediateContext->RSSetState(rs);
			SAFE_RELEASE(rs);*/
		DXUT_EndPerfEvent();
#endif
		ID3D11ShaderResourceView* ppSRVNULL[5] = { NULL, NULL, NULL, NULL, NULL };
		pd3dImmediateContext->PSSetShaderResources(0, 5, ppSRVNULL);
		pd3dImmediateContext->GSSetShaderResources(0, 5, ppSRVNULL);

		ID3D11RenderTargetView* ppRTVNULL[2] = {NULL,NULL};
		pd3dImmediateContext->OMSetRenderTargets(2, ppRTVNULL, NULL);
		DXUT_EndPerfEvent();

	}

	LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
	{
		m_cCamera.HandleMessages( hWnd, uMsg, wParam, lParam );

		
		switch(uMsg)
		{
		case WM_KEYDOWN:
			{
				int nKey = static_cast<int>(wParam);

				if (nKey == 'R')
				{
					m_bEmptyTSDF = true;
					m_pGeneratedTPC->reset();
				}
				break;
			}
		}

		return 0;
	}
};
