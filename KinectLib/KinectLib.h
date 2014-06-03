#pragma once

#ifdef KINECTLIB_EXPORTS
#define KINECTLIB_API __declspec(dllexport) 
#else
#define KINECTLIB_API __declspec(dllimport) 
#endif

#include <opencv2/core/core.hpp>
#include <Windows.h>
#include <Ole2.h>
#include "NuiApi.h"
#include <d3d11.h>
#include "DXUT.h"

namespace KinectLib{
    class KinectSensor
    {
    public:
        // Const settings for kinect
        static const int                    cBytesPerPixel = 4;
        static const NUI_IMAGE_RESOLUTION   cDepthResolution = NUI_IMAGE_RESOLUTION_640x480;
        static const NUI_IMAGE_RESOLUTION   cColorResolution = NUI_IMAGE_RESOLUTION_640x480;
        // Pointer to kinect sensor, and variable for win event message
        INuiSensor*                         m_pNuiSensor;
        HANDLE                              m_hNextDepthFrameEvent;
        HANDLE                              m_pDepthStreamHandle;
        HANDLE                              m_hNextColorFrameEvent;
        HANDLE                              m_pColorStreamHandle;
        // Color and depth resource for Directx
        ID3D11Texture2D*                    m_pDepthTex;
        ID3D11ShaderResourceView*           m_pDepthSRV;

        ID3D11Texture2D*                    m_pColorTex;
        ID3D11ShaderResourceView*           m_pColorSRV;
        // Color and depth resource for OpenCV
        cv::Mat                             m_matColor;
        cv::Mat                             m_matDepth;

        USHORT*                             m_depthD16; // 1 channel short data array for depth data
        USHORT*                             m_colorInfrared; // 1 channel short data for infra data 
        BYTE*                               m_colorRGBX; // 4 channel byte data for color(1 idle channel
        LONG*                               m_colorCoordinates; // index lookup table for default Depth color registration
        
        LONG                                m_depthWidth; 
        LONG                                m_depthHeight;
        LONG                                m_colorWidth;
        LONG                                m_colorHeight;

        LONG                                m_colorToDepthDivisor;

        bool                                m_bDepthReceived;
        bool                                m_bColorReceived;

        bool                                m_bNearMode;

        bool                                m_bPaused;
        bool                                m_bUpdated;
        bool                                m_bInfraded;
        
        // General functions
        KINECTLIB_API KinectSensor();
        KINECTLIB_API ~KinectSensor();
        HRESULT KINECTLIB_API InitialKinect();
        HRESULT KINECTLIB_API ToggleNearMode();
        // Functions for OpenCV
        HRESULT KINECTLIB_API ProcessDepth();
        HRESULT KINECTLIB_API ProcessColor();
        HRESULT KINECTLIB_API MapColorToDepth();
        HRESULT KINECTLIB_API UpdateMats(bool defaultRegistration = true);
        // Functions for DirectX
        HRESULT KINECTLIB_API CreateResource(ID3D11Device* pd3dDevice);
        HRESULT KINECTLIB_API ProcessDepth(ID3D11DeviceContext* pd3dimmediateContext);
        HRESULT KINECTLIB_API ProcessColor(ID3D11DeviceContext* pd3dimmediateContext);
        HRESULT KINECTLIB_API MapColorToDepth(ID3D11DeviceContext* pd3dimmediateContext);
        HRESULT KINECTLIB_API UpdateTextures(ID3D11DeviceContext* pd3dimmediateContext, bool defaultReg = true);
        LRESULT KINECTLIB_API HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        void    KINECTLIB_API Release();

    };
};