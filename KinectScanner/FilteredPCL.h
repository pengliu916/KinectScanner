#pragma once

#include <D3D11.h>
#include <DirectXMath.h>

#include "DXUT.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "ScreenGrab.h"

#include "header.h"

#include "TransPCLs.h"
#include "RGBDBilateralFilter.h"
#include "NormalGenerator.h"

#include "RGBDStreamDLL\IRGBDStreamForDirectX.h"
//using namespace KinectLib;

class FilteredPCL
{
public:

    ID3D11ShaderResourceView**		m_ppDepthSRV;
	ID3D11ShaderResourceView**		m_ppColorSRV;
	ID3D11ShaderResourceView**		m_ppRGBDSRV;

    IRGBDStreamForDirectX*          m_kinect;
    RGBDBilateralFilter*			m_pBilteralFilter;
    NormalGenerator*				m_pNormalGenerator;

    TransformedPointClould			m_TransformedPC;
    UINT							m_uRTwidth;
    UINT							m_uRTheight;
    bool			m_bShoot;
    bool			m_bUpdated;

    FilteredPCL(int width, int height)
    {
        m_bShoot = false;
        m_bUpdated = false;

        m_uRTwidth = width;
        m_uRTheight = height;

		m_pBilteralFilter = new RGBDBilateralFilter(width,height);
		m_pNormalGenerator = new NormalGenerator(width,height);
#if USING_KINECT
#if KINECT2
		m_kinect = DirectXStreamFactory::createFromKinect2();
#else
		m_kinect = DirectXStreamFactory::createFromKinect();
#endif
#else
#if PASSIVE_STREAM
		m_kinect = DirectXStreamFactory::createFromPassiveVideo();
#else
        m_kinect = DirectXStreamFactory::createFromVideo();
#endif
#endif
    }

	~FilteredPCL()
	{
		SAFE_RELEASE(m_pBilteralFilter);
		SAFE_RELEASE(m_pNormalGenerator);
	}

    HRESULT Initial()
    {
        HRESULT hr=S_OK;
        hr = m_kinect->Initialize();

		m_ppDepthSRV = m_kinect->getDepth_ppSRV();
		m_ppColorSRV = m_kinect->getColor_ppSRV();
		m_ppRGBDSRV = m_kinect->getRGBD_ppSRV();


		m_TransformedPC.ppMeshRGBZTexSRV = &m_pBilteralFilter->m_pOutSRV;
		m_TransformedPC.ppMeshRawRGBZTexSRV = m_ppRGBDSRV;
		m_TransformedPC.ppMeshNormalTexSRV = &m_pNormalGenerator->m_pOutSRV;
        return hr;
    }

    HRESULT CreateResource(ID3D11Device* pd3dDevice)
    {
        HRESULT hr=S_OK;

        V_RETURN(m_kinect->CreateResource(pd3dDevice));

        ID3D11DeviceContext* immediateContext;
        pd3dDevice->GetImmediateContext(&immediateContext);

        // Set rasterizer state to disable backface culling
        D3D11_RASTERIZER_DESC rasterDesc;
        rasterDesc.FillMode = D3D11_FILL_SOLID;
        rasterDesc.CullMode = D3D11_CULL_NONE;
        rasterDesc.FrontCounterClockwise = true;
        rasterDesc.DepthBias = false;
        rasterDesc.DepthBiasClamp = 0;
        rasterDesc.SlopeScaledDepthBias = 0;
        rasterDesc.DepthClipEnable = false;
        rasterDesc.ScissorEnable = false;
        rasterDesc.MultisampleEnable = false;
        rasterDesc.AntialiasedLineEnable = false;

        ID3D11RasterizerState* pState = NULL;

        V_RETURN(pd3dDevice->CreateRasterizerState(&rasterDesc, &pState));

        immediateContext->RSSetState(pState);

        SAFE_RELEASE(pState);
        SAFE_RELEASE(immediateContext);

		m_pBilteralFilter->CreateResource(pd3dDevice, m_ppRGBDSRV);
        m_pNormalGenerator->CreateResource(pd3dDevice,&m_pBilteralFilter->m_pOutSRV);

        m_TransformedPC.reset();
        
        return hr;
}

   
    void Render(ID3D11DeviceContext* pd3dimmediateContext)
    {
        m_bUpdated = m_kinect->UpdateTextures(pd3dimmediateContext);
        //m_kinect.UpdateDepthTexture(pd3dimmediateContext);
        //m_bUpdated = m_kinect.m_bDepthReceived;

        if(m_bUpdated)
        {
           
            //pd3dimmediateContext->OMSetRenderTargets(1,&m_pOutRTV,NULL);
            //if( m_bShoot )
            //{
            //	m_bShoot = false;
            //	//D3DX11SaveTextureToFile(pd3dimmediateContext,m_pOutTex,D3DX11_IFF_DDS,L"Frame.dds");

            //}
			DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Filter the RGBD and Creat Normal Map");
			m_pBilteralFilter->SetupPipeline(pd3dimmediateContext);
            m_pBilteralFilter->ProcessImage(pd3dimmediateContext);
            m_pNormalGenerator->ProcessImage(pd3dimmediateContext);
			DXUT_EndPerfEvent();
		}
    }

    void Release()
    {
        m_kinect->Release();

        m_pNormalGenerator->Release();
        m_pBilteralFilter->Release();
        
    }

    LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        m_kinect->HandleMessages(hWnd,uMsg,wParam,lParam);
        m_pBilteralFilter->HandleMessages(hWnd,uMsg,wParam,lParam);
        switch(uMsg)
        {
        case WM_KEYDOWN:
            {
                int nKey = static_cast<int>(wParam);

                /*if( nKey == 'F' )
                {
                    m_bShoot = true;
                }*/

                if (nKey == 'S')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(0,0,-SMALL_OFFSET / 5.0f));
                }
                else if (nKey == 'W')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(0,0,SMALL_OFFSET / 5.0f));
                }
                else if (nKey == 'A')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(-SMALL_OFFSET / 5.0f,0,0));
                }
                else if (nKey == 'D')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(SMALL_OFFSET / 5.0f,0,0));
                }
                else if (nKey == 'Q')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(0,SMALL_OFFSET / 5.0f,0));
                }
                else if (nKey == 'E')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixTranslation(0,-SMALL_OFFSET / 5.0f,0));
                }

                else if (nKey == 'Y')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(SMALL_ROTATE / 5.0f,0,0));
                }
                else if (nKey == 'H')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(-SMALL_ROTATE / 5.0f,0,0));
                }
                else if (nKey == 'G')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(0,SMALL_ROTATE / 5.0f,0));
                }
                else if (nKey == 'J')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(0,-SMALL_ROTATE / 5.0f,0));
                }
                else if (nKey == 'T')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(0,0,SMALL_ROTATE / 5.0f));
                }
                else if (nKey == 'U')
                {
                    m_TransformedPC.mModelM_now = XMMatrixMultiply(m_TransformedPC.mModelM_now,XMMatrixRotationRollPitchYaw(0,0,-SMALL_ROTATE / 5.0f));
                }

                else if (nKey == 'R')
                {
                    m_TransformedPC.reset();
                }
                break;
            }
        }

        return 0;
    }
};