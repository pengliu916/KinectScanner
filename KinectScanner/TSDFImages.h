// Need to be improved
#pragma once
#include <D3D11.h>
#include <DirectXMath.h>
#include "DXUT.h"
#include "DXUTcamera.h"

#include "header.h"

#include "NormalGenerator.h"
#include "VolumeTSDF.h"

struct CB_TSDFImg_PerCall
{
	float TruncDist;
	float VolumeHalfSize_x;
	float VolumeHalfSize_y;
	float VolumeHalfSize_z;
	float Tstep;
	float ReversedVolumeSize_x;
	float ReversedVolumeSize_y;
	float ReversedVolumeSize_z;
	float BoxMin_x;
	float BoxMin_y;
	float BoxMin_z;
	float NIU0;
	float BoxMax_x;
	float BoxMax_y;
	float BoxMax_z;
	float NIU1;
};

struct m_CB_TSDFImg_KinectPerFrame
{
	XMFLOAT4 KinectPos;
	XMMATRIX KinectView;
	XMMATRIX KinectTransform;
};

struct m_CB_TSDFImg_FreecamPerFrame
{
	XMMATRIX WorldViewProj;
	XMFLOAT4 ViewPos;
};

class TSDFImages
{
public:
	CModelViewerCamera				m_cCamera;
	D3D11_VIEWPORT					m_cKinectViewport;
	D3D11_VIEWPORT					m_cFreeCamViewport;
	UINT							m_uRTwidth;
	UINT							m_uRTheight;

	ID3D11VertexShader*				m_pPassVS;
	ID3D11PixelShader*				m_pKinect3ImgPS;
	ID3D11PixelShader*				m_pFreeCamShadePS;
	ID3D11PixelShader*				m_pRaycastPS;
	ID3D11GeometryShader*			m_pKinectQuadGS;
	ID3D11GeometryShader*			m_pVolumeCubeGS;
	ID3D11SamplerState*				m_pGenSampler;
	ID3D11InputLayout*				m_pScreenQuadIL;
	ID3D11Buffer*					m_pGenVB;

	CB_TSDFImg_PerCall				m_CBperCall;
	ID3D11Buffer*					m_pCBperCall;

	m_CB_TSDFImg_KinectPerFrame		m_CB_KinectPerFrame;
	ID3D11Buffer*					m_pCB_KinectPerFrame;

	m_CB_TSDFImg_FreecamPerFrame	m_CB_FreecamPerFrame;
	ID3D11Buffer*					m_pCB_FreecamPerFrame;

	ID3D11Texture2D*				m_pKinectOutTex[3]; // 0-2 depth, normal, shaded
	ID3D11ShaderResourceView*		m_pKinectOutSRV[3];
	ID3D11RenderTargetView*			m_pKinectOutRTV[3];

	ID3D11Texture2D*				m_pFreeCamOutTex;
	ID3D11ShaderResourceView*		m_pFreeCamOutSRV;
	ID3D11RenderTargetView*			m_pFreeCamOutRTV;

	ID3D11Texture2D*				m_pRaycastOutTex;
	ID3D11ShaderResourceView*		m_pRaycastOutSRV;
	ID3D11RenderTargetView*			m_pRaycastOutRTV;

	VolumeTSDF*						m_pTSDFVolume;

	bool							m_bEmptyTSDF;

	TransformedPointClould*			m_pGeneratedTPC;
	NormalGenerator*				m_pNormalGenerator;

	TSDFImages( VolumeTSDF* pTSDFVolume, UINT RTwidth = SUB_TEXTUREWIDTH, UINT RTheight = SUB_TEXTUREHEIGHT )
	{
		m_pTSDFVolume = pTSDFVolume;
		m_CBperCall.TruncDist = pTSDFVolume->m_CBperCall.truncDist;
		m_CBperCall.VolumeHalfSize_x = pTSDFVolume->m_CBperCall.halfWidth * pTSDFVolume->m_CBperCall.voxelSize ;
		m_CBperCall.VolumeHalfSize_y = pTSDFVolume->m_CBperCall.halfHeight * pTSDFVolume->m_CBperCall.voxelSize ;
		m_CBperCall.VolumeHalfSize_z = pTSDFVolume->m_CBperCall.halfDepth * pTSDFVolume->m_CBperCall.voxelSize ;
		m_CBperCall.Tstep = 0.5 * m_CBperCall.TruncDist;
		m_CBperCall.ReversedVolumeSize_x = 1.0f / ( m_CBperCall.VolumeHalfSize_x * 2.0f );
		m_CBperCall.ReversedVolumeSize_y = 1.0f / ( m_CBperCall.VolumeHalfSize_y * 2.0f );
		m_CBperCall.ReversedVolumeSize_z = 1.0f / ( m_CBperCall.VolumeHalfSize_z * 2.0f );
		m_CBperCall.BoxMin_x = -m_CBperCall.VolumeHalfSize_x;
		m_CBperCall.BoxMin_y = -m_CBperCall.VolumeHalfSize_y;
		m_CBperCall.BoxMin_z = -m_CBperCall.VolumeHalfSize_z;
		m_CBperCall.BoxMax_x = m_CBperCall.VolumeHalfSize_x;
		m_CBperCall.BoxMax_y = m_CBperCall.VolumeHalfSize_y;
		m_CBperCall.BoxMax_z = m_CBperCall.VolumeHalfSize_z;

		m_uRTwidth = RTwidth;
		m_uRTheight = RTheight;

		m_bEmptyTSDF = true;
		
		m_pGeneratedTPC = new TransformedPointClould();
		m_pNormalGenerator = new NormalGenerator();
		
		m_pGeneratedTPC->ppMeshRGBZTexSRV = &m_pKinectOutSRV[0];
		//m_pGeneratedTPC->ppMeshNormalTexSRV = &m_pKinectOutSRV[1];
		m_pGeneratedTPC->ppMeshNormalTexSRV = &m_pNormalGenerator->m_pOutSRV;
	}

	HRESULT CreateResource( ID3D11Device* pd3dDevice )
	{
		HRESULT hr = S_OK;

		ID3DBlob* pVSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "VS", "vs_5_0", COMPILE_FLAG, 0, &pVSBlob));
		V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),NULL,&m_pPassVS));
		DXUT_SetDebugName(m_pPassVS,"m_pPassVS");

		ID3DBlob* pPSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "PS_KinectView", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pKinect3ImgPS));
		DXUT_SetDebugName(m_pKinect3ImgPS, "m_pKinect3ImgPS");
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "PS_FreeView", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pFreeCamShadePS));
		DXUT_SetDebugName(m_pFreeCamShadePS, "m_pFreeCamShadePS");
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "PS_Raycast", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob));
		V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pRaycastPS));
		DXUT_SetDebugName(m_pRaycastPS, "m_pRaycastPS");
		pPSBlob->Release();

		ID3DBlob* pGSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "GS_KinectQuad", "gs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pKinectQuadGS));
		DXUT_SetDebugName(m_pKinectQuadGS, "m_pKinectQuadGS");
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "GS_VolumeCube", "gs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pGSBlob));
		V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pVolumeCubeGS));
		DXUT_SetDebugName(m_pVolumeCubeGS, "m_pVolumeCubeGS");
		pGSBlob->Release();

		D3D11_INPUT_ELEMENT_DESC inputLayout[]=
		{{ "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}};
		V_RETURN(pd3dDevice->CreateInputLayout(inputLayout,ARRAYSIZE(inputLayout),pVSBlob->GetBufferPointer(),pVSBlob->GetBufferSize(),&m_pScreenQuadIL));
		pVSBlob->Release();

		D3D11_BUFFER_DESC bd;
		ZeroMemory( &bd, sizeof(bd) );
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof( short );
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		V_RETURN(pd3dDevice->CreateBuffer(&bd,NULL,&m_pGenVB));

		// Create the constant buffers
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
		sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;		// FIX_ME
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP       ;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP       ;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP       ;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(pd3dDevice->CreateSamplerState( &sampDesc, &m_pGenSampler ));

		// Create resource for freeCam
		D3D11_TEXTURE2D_DESC	TEXDesc;
		ZeroMemory( &TEXDesc, sizeof(TEXDesc));
		TEXDesc.Width = m_uRTwidth;
		TEXDesc.Height = m_uRTheight;
		TEXDesc.MipLevels = 1;
		TEXDesc.ArraySize = 1;
		TEXDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		TEXDesc.SampleDesc.Count = 1;
		TEXDesc.Usage = D3D11_USAGE_DEFAULT;
		TEXDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		TEXDesc.CPUAccessFlags = 0;
		TEXDesc.MiscFlags = 0;
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pFreeCamOutTex));
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pRaycastOutTex));
		TEXDesc.Width = D_W;
		TEXDesc.Height = D_H;
		TEXDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pKinectOutTex[0])); // RGBZ tex
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pKinectOutTex[1])); // Normal tex
		TEXDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pKinectOutTex[2])); // Shaded tex

		D3D11_RENDER_TARGET_VIEW_DESC		RTVDesc;
		ZeroMemory( &RTVDesc, sizeof(RTVDesc));
		RTVDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		RTVDesc.Texture2D.MipSlice = 0;
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pFreeCamOutTex, &RTVDesc,&m_pFreeCamOutRTV));
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pRaycastOutTex, &RTVDesc,&m_pRaycastOutRTV));
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pKinectOutTex[2], &RTVDesc,&m_pKinectOutRTV[2]));
		RTVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pKinectOutTex[0], &RTVDesc,&m_pKinectOutRTV[0]));
		V_RETURN(pd3dDevice->CreateRenderTargetView(m_pKinectOutTex[1], &RTVDesc,&m_pKinectOutRTV[1]));


		D3D11_SHADER_RESOURCE_VIEW_DESC		SRVDesc;
		ZeroMemory( &SRVDesc, sizeof(SRVDesc));
		SRVDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = 1;
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pFreeCamOutTex, &SRVDesc, &m_pFreeCamOutSRV));
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pRaycastOutTex, &SRVDesc, &m_pRaycastOutSRV));
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pKinectOutTex[2], &SRVDesc, &m_pKinectOutSRV[2]));
		SRVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pKinectOutTex[0], &SRVDesc, &m_pKinectOutSRV[0]));
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pKinectOutTex[1], &SRVDesc, &m_pKinectOutSRV[1]));

		m_cFreeCamViewport.Width = (float)m_uRTwidth ;
		m_cFreeCamViewport.Height = (float)m_uRTheight;
		m_cFreeCamViewport.MinDepth = 0.0f;
		m_cFreeCamViewport.MaxDepth = 1.0f;
		m_cFreeCamViewport.TopLeftX = 0;
		m_cFreeCamViewport.TopLeftY = 0;

		m_cKinectViewport.Width = (float)D_W;
		m_cKinectViewport.Height = (float)D_H;
		m_cKinectViewport.MinDepth = 0.0f;
		m_cKinectViewport.MaxDepth = 1.0f;
		m_cKinectViewport.TopLeftX = 0;
		m_cKinectViewport.TopLeftY = 0;
		m_pNormalGenerator->CreateResource( pd3dDevice, &m_pKinectOutSRV[0] );


		ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
		pd3dImmediateContext->UpdateSubresource( m_pCBperCall, 0, NULL, &m_CBperCall, 0, 0 );

		// Setup the camera's projection parameters
		XMVECTORF32 vecEye = { 0.0f, 0.0f, -2.0f };
		XMVECTORF32 vecAt = { 0.0f, 0.0f, -0.0f };
		m_cCamera.SetViewParams( vecEye, vecAt );
		float fAspectRatio = m_uRTwidth / ( FLOAT )m_uRTheight;
		m_cCamera.SetProjParams( XM_PI / 180.f*70.f, fAspectRatio, 0.1f, 20.0f );
		m_cCamera.SetWindow(m_uRTwidth,m_uRTheight );
		m_cCamera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );
		m_cCamera.SetRadius(2.f, 0.1f, 10.f);

		return hr;
	}

	void Release()
	{
		SAFE_RELEASE( m_pPassVS );
		SAFE_RELEASE( m_pKinectQuadGS );
		SAFE_RELEASE( m_pVolumeCubeGS );
		SAFE_RELEASE( m_pKinect3ImgPS );
		SAFE_RELEASE( m_pFreeCamShadePS );
		SAFE_RELEASE( m_pRaycastPS );

		SAFE_RELEASE( m_pGenSampler);

		SAFE_RELEASE( m_pScreenQuadIL);
		SAFE_RELEASE( m_pGenVB);
		
		SAFE_RELEASE( m_pCBperCall );
		SAFE_RELEASE( m_pCB_KinectPerFrame );
		SAFE_RELEASE( m_pCB_FreecamPerFrame );

		SAFE_RELEASE( m_pKinectOutTex[0] );
		SAFE_RELEASE( m_pKinectOutTex[1] );
		SAFE_RELEASE( m_pKinectOutTex[2] );
		SAFE_RELEASE( m_pKinectOutSRV[0] );
		SAFE_RELEASE( m_pKinectOutSRV[1] );
		SAFE_RELEASE( m_pKinectOutSRV[2] );
		SAFE_RELEASE( m_pKinectOutRTV[0] );
		SAFE_RELEASE( m_pKinectOutRTV[1] );
		SAFE_RELEASE( m_pKinectOutRTV[2] );

		SAFE_RELEASE( m_pRaycastOutTex );
		SAFE_RELEASE( m_pRaycastOutSRV );
		SAFE_RELEASE( m_pRaycastOutRTV );

		SAFE_RELEASE( m_pFreeCamOutTex );
		SAFE_RELEASE( m_pFreeCamOutSRV );
		SAFE_RELEASE( m_pFreeCamOutRTV );

		m_pNormalGenerator->Release();
	}

	~TSDFImages()
	{
		delete m_pGeneratedTPC;
		delete m_pNormalGenerator;
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
		pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

		float ClearColor[4] = { 0.0f, 0.0f, 0.0f, -1.0f };
		pd3dImmediateContext->ClearRenderTargetView( m_pKinectOutRTV[0], ClearColor );
		pd3dImmediateContext->ClearRenderTargetView( m_pKinectOutRTV[1], ClearColor );
		ClearColor[3] = 0.0f;
		pd3dImmediateContext->ClearRenderTargetView( m_pKinectOutRTV[2], ClearColor );



		XMMATRIX mKinectTransform = m_pTSDFVolume->m_pInputPC->mPreFrame;
		m_pGeneratedTPC->mCurFrame = mKinectTransform;
		m_pGeneratedTPC->mCurRotation = m_pTSDFVolume->m_pInputPC->mPreRotation;
		XMVECTOR t;
		XMMATRIX view= XMMatrixInverse(&t,mKinectTransform);
		XMFLOAT4 pos = XMFLOAT4( 0, 0, 0, 1 );
		t = XMLoadFloat4( &pos );
		t = XMVector4Transform( t, mKinectTransform );
		XMStoreFloat4( &pos, t);

		m_CB_KinectPerFrame.KinectTransform = XMMatrixTranspose( mKinectTransform );
		m_CB_KinectPerFrame.KinectView = XMMatrixTranspose( view );
		m_CB_KinectPerFrame.KinectPos = pos;
		

		// REMOVE_ME
		/*XMMATRIX m_View = m_cCamera.GetViewMatrix();
		m_CB_KinectPerFrame.KinectView = XMMatrixTranspose(m_View);
		view = XMMatrixInverse(&t, m_View);
		m_CB_KinectPerFrame.KinectTransform = XMMatrixTranspose(view);
		pos = XMFLOAT4(0, 0, 0, 1);
		t = XMLoadFloat4(&pos);
		t = XMVector4Transform(t, m_View);
		XMStoreFloat4(&pos, t);
		XMStoreFloat4(&m_CB_KinectPerFrame.KinectPos, m_cCamera.GetEyePt());*/



		pd3dImmediateContext->UpdateSubresource( m_pCB_KinectPerFrame, 0, NULL, &m_CB_KinectPerFrame, 0, 0 );
		pd3dImmediateContext->OMSetRenderTargets( 3, m_pKinectOutRTV, NULL );
		pd3dImmediateContext->IASetInputLayout(m_pScreenQuadIL);
		UINT stride = sizeof( short );
		UINT offset = 0;
		pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pGenVB, &stride, &offset );
		pd3dImmediateContext->RSSetViewports( 1, &m_cKinectViewport );
		pd3dImmediateContext->VSSetShader( m_pPassVS, NULL, 0 );
		pd3dImmediateContext->GSSetShader( m_pKinectQuadGS, NULL, 0);
		pd3dImmediateContext->GSSetConstantBuffers( 0, 1, &m_pCBperCall );
		pd3dImmediateContext->PSSetShader( m_pKinect3ImgPS, NULL, 0 );
		pd3dImmediateContext->PSSetConstantBuffers( 0, 1, &m_pCBperCall );
		pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pCB_KinectPerFrame );
		pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pGenSampler );
		pd3dImmediateContext->PSSetShaderResources( 0, 1, &m_pTSDFVolume->m_pDWVolumeSRV);
		pd3dImmediateContext->PSSetShaderResources( 1, 1, &m_pTSDFVolume->m_pColVolumeSRV);

		pd3dImmediateContext->Draw( 1, 0 );

		ID3D11ShaderResourceView* ppSRVNULL[3] = { NULL,NULL,NULL };
		pd3dImmediateContext->PSSetShaderResources( 0, 3, ppSRVNULL);
		DXUT_EndPerfEvent();

		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Generate Normal Map");
		m_pNormalGenerator->SetupPipeline(pd3dImmediateContext);
		m_pNormalGenerator->ProcessImage(pd3dImmediateContext);
		DXUT_EndPerfEvent();

	}

	void GetRaycastImg( ID3D11DeviceContext* pd3dImmediateContext, bool phong = true )
	{

		if (m_bEmptyTSDF)
		{
			m_pTSDFVolume->Integrate(pd3dImmediateContext);
			m_bEmptyTSDF = false;
		}
		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Raycasting from free cam");
		float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		if( phong ){ 
			pd3dImmediateContext->ClearRenderTargetView( m_pFreeCamOutRTV, ClearColor ); 
		} else { 
			pd3dImmediateContext->ClearRenderTargetView( m_pRaycastOutRTV, ClearColor ); 
		}
		pd3dImmediateContext->ClearRenderTargetView( m_pRaycastOutRTV, ClearColor );

		pd3dImmediateContext->RSSetViewports( 1, &m_cFreeCamViewport );
		XMMATRIX m_Proj = m_cCamera.GetProjMatrix();
		XMMATRIX m_View = m_cCamera.GetViewMatrix();
		XMMATRIX m_World = m_cCamera.GetWorldMatrix();
		m_CB_FreecamPerFrame.WorldViewProj = XMMatrixTranspose( m_World*m_View*m_Proj );
		XMStoreFloat4( &m_CB_FreecamPerFrame.ViewPos, m_cCamera.GetEyePt() );
		pd3dImmediateContext->UpdateSubresource( m_pCB_FreecamPerFrame, 0, NULL, &m_CB_FreecamPerFrame, 0, 0 );
		if( phong ) { pd3dImmediateContext->OMSetRenderTargets( 1, &m_pFreeCamOutRTV, NULL ); } 
		else { pd3dImmediateContext->OMSetRenderTargets( 1, &m_pRaycastOutRTV, NULL ); }
		pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		pd3dImmediateContext->IASetInputLayout(m_pScreenQuadIL);
		UINT stride = sizeof( short );
		UINT offset = 0;
		pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pGenVB, &stride, &offset );
		pd3dImmediateContext->VSSetShader( m_pPassVS, NULL, 0 );
		pd3dImmediateContext->GSSetShader( m_pVolumeCubeGS, NULL, 0 );
		pd3dImmediateContext->GSSetConstantBuffers( 0, 1, &m_pCBperCall );
		pd3dImmediateContext->GSSetConstantBuffers( 2, 1, &m_pCB_FreecamPerFrame );
		if( phong ){ pd3dImmediateContext->PSSetShader( m_pFreeCamShadePS, NULL, 0 ); } 
		else{ pd3dImmediateContext->PSSetShader( m_pRaycastPS, NULL, 0 ); }
		pd3dImmediateContext->PSSetConstantBuffers( 0, 1, &m_pCBperCall );
		pd3dImmediateContext->PSSetConstantBuffers( 2, 1, &m_pCB_FreecamPerFrame );
		pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pGenSampler );
		pd3dImmediateContext->PSSetShaderResources( 0, 1, &m_pTSDFVolume->m_pDWVolumeSRV );
		pd3dImmediateContext->PSSetShaderResources( 1, 1, &m_pTSDFVolume->m_pColVolumeSRV );

		pd3dImmediateContext->Draw( 1, 0 );

		ID3D11ShaderResourceView* ppSRVNULL[3] = { NULL, NULL, NULL };
		pd3dImmediateContext->PSSetShaderResources( 0, 3, ppSRVNULL );
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
