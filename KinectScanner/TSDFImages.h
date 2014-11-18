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
};

struct m_CB_TSDFImg_FreecamPerFrame
{
	XMMATRIX InvView;
	XMFLOAT4 ViewPos;
};

class TSDFImages
{
public:
	CModelViewerCamera				m_cCamera;

	ID3D11ComputeShader*			m_pKinect3ImgCS;
	ID3D11ComputeShader*			m_pFreeCamShadeCS;
	ID3D11ComputeShader*			m_pRaycastCS;

	ID3D11SamplerState*				m_pGenSampler;

	CB_TSDFImg_PerCall				m_CBperCall;
	ID3D11Buffer*					m_pCBperCall;

	m_CB_TSDFImg_KinectPerFrame		m_CB_KinectPerFrame;
	ID3D11Buffer*					m_pCB_KinectPerFrame;

	m_CB_TSDFImg_FreecamPerFrame	m_CB_FreecamPerFrame;
	ID3D11Buffer*					m_pCB_FreecamPerFrame;

	ID3D11Texture2D*				m_pKinectOutTex[3]; // 0-2 depth, normal, shaded
	ID3D11ShaderResourceView*		m_pKinectOutSRV[3];
	ID3D11UnorderedAccessView*		m_pKinectOutUAV[3];

	ID3D11Texture2D*				m_pFreeCamOutTex;
	ID3D11ShaderResourceView*		m_pFreeCamOutSRV;
	ID3D11UnorderedAccessView*		m_pFreeCamOutUAV;

	ID3D11Texture2D*				m_pRaycastOutTex;
	ID3D11ShaderResourceView*		m_pRaycastOutSRV;
	ID3D11UnorderedAccessView*		m_pRaycastOutUAV;

	VolumeTSDF*						m_pTSDFVolume;

	bool							m_bEmptyTSDF;

	TransformedPointClould*			m_pGeneratedTPC;

	TSDFImages( VolumeTSDF* pTSDFVolume, UINT RTwidth = SUB_TEXTUREWIDTH, UINT RTheight = SUB_TEXTUREHEIGHT, bool ShadeFromKinect = true )
	{
		m_pTSDFVolume = pTSDFVolume;
		m_CBperCall.TruncDist = pTSDFVolume->m_CBperCall.truncDist;
		m_CBperCall.VolumeHalfSize.x = pTSDFVolume->m_CBperCall.halfWidth * pTSDFVolume->m_CBperCall.voxelSize ;
		m_CBperCall.VolumeHalfSize.y = pTSDFVolume->m_CBperCall.halfHeight * pTSDFVolume->m_CBperCall.voxelSize ;
		m_CBperCall.VolumeHalfSize.z = pTSDFVolume->m_CBperCall.halfDepth * pTSDFVolume->m_CBperCall.voxelSize ;
		m_CBperCall.Tstep = 0.5 * m_CBperCall.TruncDist;
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
	}

	HRESULT CreateResource( ID3D11Device* pd3dDevice )
	{
		HRESULT hr = S_OK;

		ID3DBlob* pCSBlob = NULL;
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "CS_KinectView", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), NULL, &m_pKinect3ImgCS));
		DXUT_SetDebugName(m_pKinect3ImgCS, "m_pKinect3ImgCS");
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "CS_FreeView", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), NULL, &m_pFreeCamShadeCS));
		DXUT_SetDebugName(m_pFreeCamShadeCS, "m_pFreeCamShadeCS");
		V_RETURN(DXUTCompileFromFile(L"TSDFImages.fx", nullptr, "CS_Raycast", "cs_5_0", COMPILE_FLAG, 0, &pCSBlob));
		V_RETURN(pd3dDevice->CreateComputeShader(pCSBlob->GetBufferPointer(), pCSBlob->GetBufferSize(), NULL, &m_pRaycastCS));
		DXUT_SetDebugName(m_pRaycastCS, "m_pRaycastCS");
		pCSBlob->Release();


		// Create the constant buffers
		D3D11_BUFFER_DESC bd;
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

		// Create resource for freeCam
		D3D11_TEXTURE2D_DESC	TEXDesc;
		ZeroMemory( &TEXDesc, sizeof(TEXDesc));
		TEXDesc.MipLevels = 1;
		TEXDesc.ArraySize = 1;
		TEXDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		TEXDesc.SampleDesc.Count = 1;
		TEXDesc.Usage = D3D11_USAGE_DEFAULT;
		TEXDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		TEXDesc.CPUAccessFlags = 0;
		TEXDesc.MiscFlags = 0;
		TEXDesc.Width = D_W;
		TEXDesc.Height = D_H;
		TEXDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pKinectOutTex[0])); DXUT_SetDebugName(m_pKinectOutTex[0], "m_pKinectOutTex[0]"); // RGBZ tex
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pKinectOutTex[1])); DXUT_SetDebugName(m_pKinectOutTex[1], "m_pKinectOutTex[1]");// Normal tex
		TEXDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pKinectOutTex[2])); DXUT_SetDebugName(m_pKinectOutTex[2], "m_pKinectOutTex[2]");// Shaded tex

		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pKinectOutTex[0], nullptr, &m_pKinectOutUAV[0])); DXUT_SetDebugName(m_pKinectOutUAV[0], "m_pKinectOutUAV[0]");
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pKinectOutTex[1], nullptr, &m_pKinectOutUAV[1])); DXUT_SetDebugName(m_pKinectOutUAV[1], "m_pKinectOutUAV[1]");
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pKinectOutTex[2], nullptr, &m_pKinectOutUAV[2])); DXUT_SetDebugName(m_pKinectOutUAV[2], "m_pKinectOutUAV[2]");

		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pKinectOutTex[0], nullptr, &m_pKinectOutSRV[0])); DXUT_SetDebugName(m_pKinectOutSRV[0], "m_pKinectOutSRV[0]");
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pKinectOutTex[1], nullptr, &m_pKinectOutSRV[1])); DXUT_SetDebugName(m_pKinectOutSRV[1], "m_pKinectOutSRV[1]");
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pKinectOutTex[2], nullptr, &m_pKinectOutSRV[2])); DXUT_SetDebugName(m_pKinectOutSRV[2], "m_pKinectOutSRV[2]");

		ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
		pd3dImmediateContext->UpdateSubresource( m_pCBperCall, 0, NULL, &m_CBperCall, 0, 0 );
		//SAFE_RELEASE(pd3dImmediateContext);

		// Setup the camera's projection parameters
		XMVECTORF32 vecEye = { 0.0f, 0.0f, -2.0f };
		XMVECTORF32 vecAt = { 0.0f, 0.0f, -0.0f };
		m_cCamera.SetViewParams( vecEye, vecAt );

		return hr;
	}

	HRESULT Resize(ID3D11Device* pd3dDevice, int iWidth, int iHeight)
	{
		HRESULT hr = S_OK;

		// Create resource for freeCam
		D3D11_TEXTURE2D_DESC	TEXDesc;
		ZeroMemory(&TEXDesc, sizeof(TEXDesc));
		TEXDesc.Width = iWidth;
		TEXDesc.Height = iHeight;
		TEXDesc.MipLevels = 1;
		TEXDesc.ArraySize = 1;
		TEXDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		TEXDesc.SampleDesc.Count = 1;
		TEXDesc.Usage = D3D11_USAGE_DEFAULT;
		TEXDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		TEXDesc.CPUAccessFlags = 0;
		TEXDesc.MiscFlags = 0;
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pFreeCamOutTex));
		V_RETURN(pd3dDevice->CreateTexture2D(&TEXDesc, NULL, &m_pRaycastOutTex));

		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pFreeCamOutTex, nullptr, &m_pFreeCamOutUAV)); DXUT_SetDebugName(m_pFreeCamOutUAV, "m_pFreeCamOutUAV");
		V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pRaycastOutTex, nullptr, &m_pRaycastOutUAV)); DXUT_SetDebugName(m_pRaycastOutUAV, "m_pRaycastOutUAV");

		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pFreeCamOutTex, nullptr, &m_pFreeCamOutSRV)); DXUT_SetDebugName(m_pFreeCamOutSRV, "m_pFreeCamOutSRV");
		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pRaycastOutTex, nullptr, &m_pRaycastOutSRV)); DXUT_SetDebugName(m_pRaycastOutSRV, "m_pRaycastOutSRV");
		
		// Setup the camera's projection parameters
		float fAspectRatio = iWidth / (FLOAT)iHeight;
		m_cCamera.SetProjParams(XM_PI / 180.f*90.f, fAspectRatio, 0.01f, 20.0f);
		m_cCamera.SetWindow(iWidth, iHeight);
		m_cCamera.SetButtonMasks(MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON);
		m_cCamera.SetRadius(2.f, 0.1f, 10.f);

		// Update parameters related to back buffer
		m_CBperCall.RTreso.x = (float)iWidth;
		m_CBperCall.RTreso.y = (float)iHeight;

		// Submit to GPU
		ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
		pd3dImmediateContext->UpdateSubresource(m_pCBperCall, 0, NULL, &m_CBperCall, 0, 0);
		//SAFE_RELEASE(pd3dImmediateContext);

		return hr;
	}

	void Destory()
	{
		SAFE_RELEASE( m_pKinect3ImgCS );
		SAFE_RELEASE( m_pFreeCamShadeCS );
		SAFE_RELEASE( m_pRaycastCS );

		SAFE_RELEASE( m_pGenSampler);
		
		SAFE_RELEASE( m_pCBperCall );
		SAFE_RELEASE( m_pCB_KinectPerFrame );
		SAFE_RELEASE( m_pCB_FreecamPerFrame );

		SAFE_RELEASE( m_pKinectOutTex[0] );
		SAFE_RELEASE( m_pKinectOutTex[1] );
		SAFE_RELEASE( m_pKinectOutTex[2] );
		SAFE_RELEASE( m_pKinectOutSRV[0] );
		SAFE_RELEASE( m_pKinectOutSRV[1] );
		SAFE_RELEASE( m_pKinectOutSRV[2] );
		SAFE_RELEASE( m_pKinectOutUAV[0] );
		SAFE_RELEASE( m_pKinectOutUAV[1] );
		SAFE_RELEASE( m_pKinectOutUAV[2] );
	}

	void Release()
	{
		SAFE_RELEASE( m_pRaycastOutTex );
		SAFE_RELEASE( m_pRaycastOutSRV );
		SAFE_RELEASE( m_pRaycastOutUAV );

		SAFE_RELEASE( m_pFreeCamOutTex );
		SAFE_RELEASE( m_pFreeCamOutSRV );
		SAFE_RELEASE( m_pFreeCamOutUAV );
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

		float ClearColor[4] = { 0.0f, 0.0f, 0.0f, -1.0f };

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
		m_CB_KinectPerFrame.InvKinectTransform = XMMatrixTranspose( view );
		m_CB_KinectPerFrame.KinectPos = pos;

		pd3dImmediateContext->UpdateSubresource( m_pCB_KinectPerFrame, 0, NULL, &m_CB_KinectPerFrame, 0, 0 );
		pd3dImmediateContext->CSSetShader( m_pKinect3ImgCS, NULL, 0 );
		pd3dImmediateContext->CSSetConstantBuffers( 0, 1, &m_pCBperCall );
		pd3dImmediateContext->CSSetConstantBuffers( 1, 1, &m_pCB_KinectPerFrame );
		pd3dImmediateContext->CSSetSamplers( 0, 1, &m_pGenSampler );
		pd3dImmediateContext->CSSetShaderResources( 0, 1, &m_pTSDFVolume->m_pDWVolumeSRV);
		pd3dImmediateContext->CSSetShaderResources( 1, 1, &m_pTSDFVolume->m_pColVolumeSRV);
		UINT initCounts = 0;
		pd3dImmediateContext->CSSetUnorderedAccessViews(0,3,m_pKinectOutUAV,&initCounts);
		pd3dImmediateContext->Dispatch(ceil(D_W / (float)THREAD2D_X), ceil(D_H / (float)THREAD2D_Y), 1);

		ID3D11ShaderResourceView* ppSRVNULL[2] = { NULL,NULL };
		pd3dImmediateContext->CSSetShaderResources( 0, 2, ppSRVNULL);

		ID3D11UnorderedAccessView* ppUAViewNULL[3] = { NULL, NULL, NULL };
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 3, ppUAViewNULL, &initCounts);
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
		XMMATRIX m_View = m_cCamera.GetViewMatrix();
		XMMATRIX m_World = m_cCamera.GetWorldMatrix();
		m_CB_FreecamPerFrame.InvView = XMMatrixTranspose( XMMatrixInverse(NULL,m_View) );
		XMStoreFloat4( &m_CB_FreecamPerFrame.ViewPos, m_cCamera.GetEyePt() );
		pd3dImmediateContext->UpdateSubresource( m_pCB_FreecamPerFrame, 0, NULL, &m_CB_FreecamPerFrame, 0, 0 );

		UINT initCounts = 0;
		if (phong) { pd3dImmediateContext->CSSetUnorderedAccessViews(3, 1, &m_pFreeCamOutUAV, &initCounts); }
		else { pd3dImmediateContext->CSSetUnorderedAccessViews(4, 1, &m_pRaycastOutUAV, &initCounts); }
		pd3dImmediateContext->CSSetConstantBuffers( 0, 1, &m_pCBperCall );
		pd3dImmediateContext->CSSetConstantBuffers( 2, 1, &m_pCB_FreecamPerFrame );
		if( phong ){ pd3dImmediateContext->CSSetShader( m_pFreeCamShadeCS, NULL, 0 ); } 
		else{ pd3dImmediateContext->CSSetShader( m_pRaycastCS, NULL, 0 ); }
		pd3dImmediateContext->CSSetConstantBuffers( 0, 1, &m_pCBperCall );
		pd3dImmediateContext->CSSetConstantBuffers( 2, 1, &m_pCB_FreecamPerFrame );
		pd3dImmediateContext->CSSetSamplers( 0, 1, &m_pGenSampler );
		pd3dImmediateContext->CSSetShaderResources( 0, 1, &m_pTSDFVolume->m_pDWVolumeSRV );
		pd3dImmediateContext->CSSetShaderResources( 1, 1, &m_pTSDFVolume->m_pColVolumeSRV );

		pd3dImmediateContext->Dispatch(ceil(m_CBperCall.RTreso.x / (float)THREAD2D_X), ceil(m_CBperCall.RTreso.y / (float)THREAD2D_Y), 1);

		ID3D11ShaderResourceView* ppSRVNULL[2] = { NULL, NULL };
		pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRVNULL);

		ID3D11UnorderedAccessView* ppUAViewNULL[4] = { NULL, NULL, NULL, NULL };
		pd3dImmediateContext->CSSetUnorderedAccessViews(0, 4, ppUAViewNULL, &initCounts);
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
