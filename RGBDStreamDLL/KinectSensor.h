#pragma once
#include <opencv2\core\core.hpp>
#include <Ole2.h>
#include <d3d11.h>
#include "DXUT.h"
#include "NuiApi.h"
#include "IRGBDStreamForOpenCV.h"
#include "IRGBDStreamForDirectX.h"
using namespace std;

class KinectSensor : public IRGBDStreamForOpenCV, public IRGBDStreamForDirectX{

public:
    static const int                    cBytesPerPixel = 4;
    static const NUI_IMAGE_RESOLUTION   cDepthResolution = NUI_IMAGE_RESOLUTION_640x480;
    static const NUI_IMAGE_RESOLUTION   cColorResolution = NUI_IMAGE_RESOLUTION_640x480;

    INuiSensor*                         m_pNuiSensor;
    HANDLE                              m_hNextDepthFrameEvent;
    HANDLE                              m_pDepthStreamHandle;
    HANDLE                              m_hNextColorFrameEvent;
    HANDLE                              m_pColorStreamHandle;

    cv::Mat                             m_matColor;
    cv::Mat                             m_matDepth;

    // Color and depth resource for Directx
    ID3D11Texture2D*                    m_pDepthTex;
    ID3D11ShaderResourceView*           m_pDepthSRV;

    ID3D11Texture2D*                    m_pColorTex;
    ID3D11ShaderResourceView*           m_pColorSRV;

    USHORT*                             m_depthD16;
    USHORT*                             m_colorInfrared;
    BYTE*                               m_colorRGBX;
    LONG*                               m_colorCoordinates;

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



	HRESULT CompileFormString(string code,
							  const D3D_SHADER_MACRO* pDefines,
							  LPCSTR pEntrypoint, LPCSTR pTarget,
							  UINT Flags1, UINT Flags2,
							  ID3DBlob** ppCode);
	// Generate shader code
	void GenShaderCode();
	// flag for shader compile
	const UINT					m_uCompileFlag;

	ID3D11VertexShader*				m_pPassVS;
	ID3D11PixelShader*				m_pPS;
	ID3D11GeometryShader*			m_pScreenQuadGS;
	ID3D11InputLayout*				m_pScreenQuadIL;
	ID3D11Buffer*					m_pScreenQuadVB;
	//For Texture output
	ID3D11Texture2D*				m_pRGBDTex;
	ID3D11ShaderResourceView*		m_pRGBDSRV;
	ID3D11RenderTargetView*			m_pRGBDRTV;
	D3D11_VIEWPORT					m_Viewport;

	// str for shader code
	std::string m_strShaderCode;



    KinectSensor();
    virtual HRESULT Initialize();
	HRESULT ToggleNearMode();
	virtual void GetColorReso( int& iColorWidth, int& iColorHeight );
	virtual void GetDepthReso( int& iDepthWidth, int& iDepthHeight );
	virtual void GetInfraredReso(int& iInfraredWidth, int& iInfraredHeight);
	// For OpenCV
    HRESULT ProcessDepth();
    HRESULT ProcessColor();
    HRESULT MapColorToDepth();
    // For OpenCV interface
    virtual void GetColorMat(cv::Mat& out);
    virtual void GetDepthMat(cv::Mat& out);
	virtual void GetInfraredMat(cv::Mat& out);
	virtual bool UpdateMats(bool defaultReg, bool color, bool depth, bool infrared);
    
    // For DirectX
    HRESULT ProcessDepth( ID3D11DeviceContext* pd3dimmediateContext );
    HRESULT ProcessColor( ID3D11DeviceContext* pd3dimmediateContext );
    HRESULT MapColorToDepth( ID3D11DeviceContext* pd3dimmediateContext );
    // For DirectX interface
    virtual HRESULT CreateResource(ID3D11Device*);
    virtual bool UpdateTextures( ID3D11DeviceContext*,
                                 bool defaultReg, bool color, bool depth, bool infrared );
    virtual void Release();
    virtual LRESULT HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual ID3D11ShaderResourceView** getColor_ppSRV();
    virtual ID3D11ShaderResourceView** getDepth_ppSRV();
	virtual ID3D11ShaderResourceView** getInfrared_ppSRV();
	virtual ID3D11ShaderResourceView** getRGBD_ppSRV();
    virtual ~KinectSensor();

};

KinectSensor::KinectSensor() : m_uCompileFlag(D3DCOMPILE_ENABLE_STRICTNESS){
    DWORD width = 0;
    DWORD height = 0;

    m_pNuiSensor = NULL;
    NuiImageResolutionToSize(cDepthResolution, width, height);
    m_depthWidth = static_cast< LONG >(width);
    m_depthHeight = static_cast< LONG >(height);

    NuiImageResolutionToSize(cColorResolution, width, height);
    m_colorWidth = static_cast< LONG >(width);
    m_colorHeight = static_cast< LONG >(height);

    m_colorToDepthDivisor = m_colorWidth / m_depthWidth;

    m_pDepthTex = NULL;
    m_pDepthSRV = NULL;
    m_pColorTex = NULL;
    m_pColorSRV = NULL;

    m_bDepthReceived = false;
    m_bColorReceived = false;

    m_hNextDepthFrameEvent = INVALID_HANDLE_VALUE;
    m_pDepthStreamHandle = INVALID_HANDLE_VALUE;
    m_hNextColorFrameEvent = INVALID_HANDLE_VALUE;
    m_pColorStreamHandle = INVALID_HANDLE_VALUE;

    m_matColor.create(m_colorHeight, m_colorWidth, CV_8UC3);
    m_matDepth.create(m_depthHeight, m_depthWidth, CV_16UC1);

    m_depthD16 = new USHORT[m_depthWidth*m_depthHeight];
    m_colorInfrared = new USHORT[m_colorHeight*m_colorWidth];
    m_colorCoordinates = new LONG[m_depthWidth*m_depthHeight * 2];
    m_colorRGBX = new BYTE[m_colorWidth*m_colorHeight*cBytesPerPixel];

    m_bNearMode = false;
    m_bInfraded = false;
    m_bPaused = false;
}

KinectSensor::~KinectSensor(){
    if( NULL != m_pNuiSensor )
    {
        m_pNuiSensor->NuiShutdown();
        m_pNuiSensor->Release();
    }
    CloseHandle( m_hNextDepthFrameEvent );
    CloseHandle( m_hNextColorFrameEvent );

    // done with pixel data
    delete[] m_colorInfrared;
    delete[] m_colorRGBX;
    delete[] m_colorCoordinates;
    delete[] m_depthD16;
}


HRESULT KinectSensor::Initialize(){
    HRESULT hr = S_OK;
    INuiSensor* pNuiSensor = NULL;

    int iSensorCount = 0;
    hr = NuiGetSensorCount(&iSensorCount);
    if (FAILED(hr)) { return hr; }
    // Look at each Kinect sensor
    for (int i = 0; i < iSensorCount; ++i)
    {   // Create the sensor so we can check status, if we can't create it, move on to the next
        hr = NuiCreateSensorByIndex(i, &pNuiSensor);
        if (FAILED(hr)){ continue; }
        // Get the status of the sensor, and if connected, then we can initialize it
        hr = pNuiSensor->NuiStatus();
        if (S_OK == hr){ m_pNuiSensor = pNuiSensor; break; }
        // This sensor wasn't OK, so release it since we're not using it
        pNuiSensor->Release();
    }

    if (NULL == m_pNuiSensor){ return E_FAIL; }

    // Initialize the Kinect and specify that we'll be using depth
    hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_DEPTH);
    if (FAILED(hr)) { return hr; }

    // Create an event that will be signaled when depth data is available
    m_hNextDepthFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Open a depth image stream to receive depth frames
    hr = m_pNuiSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH, cDepthResolution,
                                          0, 2, m_hNextDepthFrameEvent, &m_pDepthStreamHandle);
    if (FAILED(hr)) { return hr; }

    // Create an event that will be signaled when color data is available
    m_hNextColorFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Open a color image stream to receive color frames
    hr = m_pNuiSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR, cColorResolution,
                                          0, 2, m_hNextColorFrameEvent, &m_pColorStreamHandle);
    if (FAILED(hr)) { return hr; }

    // Start with near mode on
    ToggleNearMode();
    return hr;
}

void KinectSensor::GetColorReso( int& iColorWidth, int& iColorHeight )
{
	iColorWidth = m_colorWidth;
	iColorHeight = m_colorHeight;
}

void KinectSensor::GetDepthReso( int& iDepthWidth, int& iDepthHeight )
{
	iDepthWidth = m_depthWidth;
	iDepthHeight = m_depthHeight;
}

void KinectSensor::GetInfraredReso(int& iInfraredWidth, int& iInfraredHeight)
{
}

HRESULT KinectSensor::ToggleNearMode()
{
    HRESULT hr = E_FAIL;

    if (m_pNuiSensor){
        hr = m_pNuiSensor->NuiImageStreamSetImageFrameFlags(m_pDepthStreamHandle, m_bNearMode ? 0 : NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE);
        if (SUCCEEDED(hr)){ m_bNearMode = !m_bNearMode; }
    }
    return hr;
}

// For OpenCV code
HRESULT KinectSensor::ProcessDepth()
{
    HRESULT hr = S_OK;
    NUI_IMAGE_FRAME    imageFrame;
    hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pDepthStreamHandle, 0, &imageFrame);
    if (FAILED(hr)){ return hr; }

    NUI_LOCKED_RECT LockedRect;
    hr = imageFrame.pFrameTexture->LockRect(0, &LockedRect, NULL, 0);
    if (FAILED(hr)){ return hr; }

    memcpy(m_depthD16, LockedRect.pBits, LockedRect.size);

    m_bDepthReceived = true;

    hr = imageFrame.pFrameTexture->UnlockRect(0);
    if (FAILED(hr)){ return hr; }

    hr = m_pNuiSensor->NuiImageStreamReleaseFrame(m_pDepthStreamHandle, &imageFrame);

    for (int y = 0; y < m_colorHeight; ++y){
        // Get row pointer for depth Mat
        USHORT* pDepthRow = m_matDepth.ptr<USHORT>(y);
        for (int x = 0; x < m_colorWidth; ++x){
            pDepthRow[x] = m_depthD16[y * m_colorWidth + x];
        }
    }
    return hr;
}

HRESULT KinectSensor::ProcessColor()
{
    HRESULT hr = S_OK;
    NUI_IMAGE_FRAME imageFrame;
    hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pColorStreamHandle, 0, &imageFrame);
    if (FAILED(hr)) { return hr; }

    NUI_LOCKED_RECT LockedRect;
    hr = imageFrame.pFrameTexture->LockRect(0, &LockedRect, NULL, 0);
    if (FAILED(hr)) { return hr; }
    INT pitch = LockedRect.Pitch;
    if (m_bInfraded){
        for (LONG y = 0; y < m_colorHeight; ++y){
            LONG* pDest = (LONG*)((BYTE*)m_colorRGBX + m_colorWidth * 4 * y);
            for (LONG x = 0; x < m_colorWidth; ++x){
                BYTE intensity = reinterpret_cast< USHORT* >(LockedRect.pBits)[y*m_colorWidth + x] >> 8;
                *pDest = intensity << 24;
                *pDest |= intensity << 16;
                *pDest |= intensity << 8;
                *pDest |= intensity;
                pDest++;
            }
        }
    } else{
        memcpy(m_colorRGBX, LockedRect.pBits, LockedRect.size);
    }
    m_bColorReceived = true;

    hr = imageFrame.pFrameTexture->UnlockRect(0);
    if (FAILED(hr)) { return hr; };

    hr = m_pNuiSensor->NuiImageStreamReleaseFrame(m_pColorStreamHandle, &imageFrame);

    for (int y = 0; y < m_colorHeight; ++y){
        cv::Vec3b* pColorRow = m_matColor.ptr<cv::Vec3b>(y);
        for (int x = 0; x < m_colorWidth; ++x){
            pColorRow[x] = cv::Vec3b(m_colorRGBX[y*m_colorWidth*cBytesPerPixel + x * 4 + 0],
                                     m_colorRGBX[y*m_colorWidth*cBytesPerPixel + x * 4 + 1],
                                     m_colorRGBX[y*m_colorWidth*cBytesPerPixel + x * 4 + 2]);
        }
    }
    return hr;
}

HRESULT KinectSensor::MapColorToDepth()
{
    HRESULT hr = S_OK;
    // Get of x, y coordinates for color in depth space
    // This will allow us to later compensate for the differences in location, angle, etc between the depth and color cameras
    m_pNuiSensor->NuiImageGetColorPixelCoordinateFrameFromDepthPixelFrameAtResolution(
        cColorResolution, cDepthResolution, m_depthWidth*m_depthHeight,
        m_depthD16, m_depthWidth*m_depthHeight * 2, m_colorCoordinates);
    // loop over each row and column of the color
    for (LONG y = 0; y < m_colorHeight; ++y){
        cv::Vec3b* pColorRow = m_matColor.ptr<cv::Vec3b>(y);
        for (LONG x = 0; x < m_colorWidth; ++x){
            // calculate index into depth array
            int depthIndex = x / m_colorToDepthDivisor + y / m_colorToDepthDivisor * m_depthWidth;

            // retrieve the depth to color mapping for the current depth pixel
            LONG colorInDepthX = m_colorCoordinates[depthIndex * 2];
            LONG colorInDepthY = m_colorCoordinates[depthIndex * 2 + 1];

            // make sure the depth pixel maps to a valid point in color space
            if (colorInDepthX >= 0 && colorInDepthX < m_colorWidth && colorInDepthY >= 0 && colorInDepthY < m_colorHeight){
                // calculate index into color array
                LONG colorIndex = colorInDepthX + colorInDepthY * m_colorWidth;

                // set source for copy to the color pixel
                BYTE* pSrc = (BYTE*)((LONG *)m_colorRGBX + colorIndex);
                pColorRow[x] = cv::Vec3b(pSrc[0], pSrc[1], pSrc[2]);
            } else{
                pColorRow[x] = cv::Vec3b(0, 0, 0);
            }
        }
    }
    return hr;
}

bool KinectSensor::UpdateMats(bool defaultReg=true, bool color=true, bool depth=true, bool infrared = false)
{
    m_bUpdated = true;

    if (m_bPaused) return false;
    bool needToMapColorToDepth = false;
    if (depth){
        if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextDepthFrameEvent, 1000)){
            // if we have received any valid new depth data we may need to draw
            if (SUCCEEDED(ProcessDepth())){ needToMapColorToDepth = true; }
        }
    }
    if (color){
        if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextColorFrameEvent, 1000)){
            // if we have received any valid new color data we may need to draw
            if (SUCCEEDED(ProcessColor())){ needToMapColorToDepth = true; }
        }
    }
    if(!(color && m_bColorReceived) || !(depth && m_bDepthReceived)){
        m_bUpdated = false;
    }
    // If we have not yet received any data for either color or depth since we started up, we shouldn't draw
    if (!m_bDepthReceived || !m_bColorReceived){
        needToMapColorToDepth = false;
    }

    if (needToMapColorToDepth && defaultReg){
        MapColorToDepth();
    }
    return m_bUpdated;
}

void KinectSensor::GetDepthMat(cv::Mat& out){
    m_matDepth.copyTo(out);
}

void KinectSensor::GetColorMat(cv::Mat& out){
    m_matColor.copyTo(out);
}

void KinectSensor::GetInfraredMat( cv::Mat& out ){
	m_matDepth.copyTo( out );
}

HRESULT KinectSensor::CreateResource(ID3D11Device* pd3dDevice){
    HRESULT hr = S_OK;



	GenShaderCode();
	ID3DBlob* pVSBlob = NULL;
	V_RETURN(CompileFormString(m_strShaderCode, nullptr, "VS", "vs_5_0", m_uCompileFlag, 0, &pVSBlob));
	V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pPassVS));
	DXUT_SetDebugName(m_pPassVS, "m_pPassVS");

	ID3DBlob* pGSBlob = NULL;
	V_RETURN(CompileFormString(m_strShaderCode, nullptr, "GS", "gs_5_0", m_uCompileFlag, 0, &pGSBlob));
	V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pScreenQuadGS));
	DXUT_SetDebugName(m_pScreenQuadGS, "m_pScreenQuadGS");
	pGSBlob->Release();

	ID3DBlob* pPSBlob = NULL;
	V_RETURN(CompileFormString(m_strShaderCode, nullptr, "PS", "ps_5_0", m_uCompileFlag, 0, &pPSBlob));
	V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPS));
	DXUT_SetDebugName(m_pPS, "m_pPS");
	pPSBlob->Release();

	D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
	V_RETURN(pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pScreenQuadIL));
	DXUT_SetDebugName(m_pScreenQuadIL, "m_pScreenQuadIL");
	pVSBlob->Release();


	// Create the vertex buffer
	D3D11_BUFFER_DESC bd = { 0 };
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(short);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bd, NULL, &m_pScreenQuadVB));
	DXUT_SetDebugName(m_pScreenQuadVB, "m_pScreenQuadVB");

    // Create depth texture and its resource view
    D3D11_TEXTURE2D_DESC depthTexDesc = { 0 };
    depthTexDesc.Width = m_depthWidth;
    depthTexDesc.Height = m_depthHeight;
    depthTexDesc.MipLevels = 1;
    depthTexDesc.ArraySize = 1;
    depthTexDesc.Format = DXGI_FORMAT_R16_UINT;
    depthTexDesc.SampleDesc.Count = 1;
    depthTexDesc.SampleDesc.Quality = 0;
    depthTexDesc.Usage = D3D11_USAGE_DYNAMIC;
    depthTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    depthTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    depthTexDesc.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateTexture2D( &depthTexDesc, NULL, &m_pDepthTex ));
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pDepthTex, NULL, &m_pDepthSRV ));

    // Create color texture and its resource view
    D3D11_TEXTURE2D_DESC colorTexDesc = { 0 };
    colorTexDesc.Width = m_colorWidth;
    colorTexDesc.Height = m_colorHeight;
    colorTexDesc.MipLevels = 1;
    colorTexDesc.ArraySize = 1;
    colorTexDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    colorTexDesc.SampleDesc.Count = 1;
    colorTexDesc.SampleDesc.Quality = 0;
    colorTexDesc.Usage = D3D11_USAGE_DYNAMIC;
    colorTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    colorTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    colorTexDesc.MiscFlags = 0;
    V_RETURN( pd3dDevice->CreateTexture2D( &colorTexDesc, NULL, &m_pColorTex ));
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pColorTex, NULL, &m_pColorSRV ));


	// Create rendertarget resource

	D3D11_TEXTURE2D_DESC	RTtextureDesc = { 0 };
	RTtextureDesc.Width = m_depthWidth;
	RTtextureDesc.Height = m_depthHeight;
	RTtextureDesc.MipLevels = 1;
	RTtextureDesc.ArraySize = 1;
	RTtextureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	RTtextureDesc.SampleDesc.Count = 1;
	RTtextureDesc.Usage = D3D11_USAGE_DEFAULT;
	RTtextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	RTtextureDesc.CPUAccessFlags = 0;
	RTtextureDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateTexture2D(&RTtextureDesc, NULL, &m_pRGBDTex));
	V_RETURN(pd3dDevice->CreateRenderTargetView(m_pRGBDTex, NULL, &m_pRGBDRTV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pRGBDTex, NULL, &m_pRGBDSRV));

	m_Viewport.Width = (float)m_depthWidth;
	m_Viewport.Height = (float)m_depthHeight;
	m_Viewport.MinDepth = 0.0f;
	m_Viewport.MaxDepth = 1.0f;
	m_Viewport.TopLeftX = 0;
	m_Viewport.TopLeftY = 0;


    return hr;
}


void KinectSensor::Release(){
    SAFE_RELEASE( m_pDepthTex );
    SAFE_RELEASE( m_pDepthSRV );
    SAFE_RELEASE( m_pColorTex );
    SAFE_RELEASE( m_pColorSRV );



	SAFE_RELEASE(m_pRGBDTex);
	SAFE_RELEASE(m_pRGBDRTV);
	SAFE_RELEASE(m_pRGBDSRV);

	SAFE_RELEASE(m_pPassVS);
	SAFE_RELEASE(m_pPS);
	SAFE_RELEASE(m_pScreenQuadGS);
	SAFE_RELEASE(m_pScreenQuadIL);
	SAFE_RELEASE(m_pScreenQuadVB);


}

HRESULT KinectSensor::ProcessDepth( ID3D11DeviceContext* pd3dimmediateContext ){
    HRESULT hr = S_OK;
    NUI_IMAGE_FRAME	imageFrame;
    hr = m_pNuiSensor->NuiImageStreamGetNextFrame( m_pDepthStreamHandle, 0, &imageFrame );
    if( FAILED( hr )){ return hr; }

    NUI_LOCKED_RECT LockedRect;
    hr = imageFrame.pFrameTexture->LockRect( 0, &LockedRect, NULL, 0 );
    if( FAILED( hr )){ return hr; }

    memcpy( m_depthD16, LockedRect.pBits, LockedRect.size );

    m_bDepthReceived = true;

    hr = imageFrame.pFrameTexture->UnlockRect( 0 );
    if( FAILED( hr )){ return hr; }

    hr = m_pNuiSensor->NuiImageStreamReleaseFrame( m_pDepthStreamHandle, &imageFrame );

    D3D11_MAPPED_SUBRESOURCE msT;
    hr = pd3dimmediateContext->Map( m_pDepthTex, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &msT );
    if( FAILED( hr ))	 { return hr; }
    memcpy( msT.pData, m_depthD16, LockedRect.size );
    pd3dimmediateContext->Unmap( m_pDepthTex, NULL );

    return hr;
}

HRESULT KinectSensor::ProcessColor( ID3D11DeviceContext* pd3dimmediateContext ){
    HRESULT hr = S_OK;
    NUI_IMAGE_FRAME imageFrame;
    hr = m_pNuiSensor->NuiImageStreamGetNextFrame( m_pColorStreamHandle, 0, &imageFrame );
    if( FAILED( hr )){ return hr; }

    NUI_LOCKED_RECT LockedRect;
    hr = imageFrame.pFrameTexture->LockRect( 0, &LockedRect, NULL, 0 );
    if( FAILED( hr )){ return hr; }

    memcpy( m_colorRGBX, LockedRect.pBits, LockedRect.size );
    m_bColorReceived = true;

    hr = imageFrame.pFrameTexture->UnlockRect( 0 );
    if( FAILED( hr )){ return hr; };

    hr = m_pNuiSensor->NuiImageStreamReleaseFrame( m_pColorStreamHandle, &imageFrame );

    D3D11_MAPPED_SUBRESOURCE msT;
    hr = pd3dimmediateContext->Map(m_pColorTex,NULL,D3D11_MAP_WRITE_DISCARD,NULL,&msT);
    if(FAILED(hr))	 {return hr;}
    memcpy(msT.pData,m_colorRGBX,LockedRect.size);
    pd3dimmediateContext->Unmap(m_pColorTex,NULL);

    return hr;
}

HRESULT KinectSensor::MapColorToDepth( ID3D11DeviceContext* pd3dimmediateContext ){
    HRESULT hr = S_OK;
    // Get of x, y coordinates for color in depth space
    // This will allow us to later compensate for the differences in location, angle, etc between the depth and color cameras
    m_pNuiSensor->NuiImageGetColorPixelCoordinateFrameFromDepthPixelFrameAtResolution(
        cColorResolution,cDepthResolution,m_depthWidth*m_depthHeight,
        m_depthD16,m_depthWidth*m_depthHeight * 2,m_colorCoordinates);

    // copy to our d3d 11 color texture
    D3D11_MAPPED_SUBRESOURCE msT;
    hr = pd3dimmediateContext->Map( m_pColorTex, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &msT );
    if( FAILED( hr )){ return hr; }

    // loop over each row and column of the color
    for( LONG y = 0; y < m_colorHeight; ++y ){
        LONG* pDest = ( LONG* )(( BYTE* )msT.pData + msT.RowPitch * y );
        for( LONG x = 0; x < m_colorWidth; ++x ){
            // calculate index into depth array
            int depthIndex = x / m_colorToDepthDivisor + y / m_colorToDepthDivisor * m_depthWidth;

            // retrieve the depth to color mapping for the current depth pixel
            LONG colorInDepthX = m_colorCoordinates[depthIndex * 2];
            LONG colorInDepthY = m_colorCoordinates[depthIndex * 2 + 1];

            // make sure the depth pixel maps to a valid point in color space
            if( colorInDepthX >= 0 && colorInDepthX < m_colorWidth && colorInDepthY >= 0 && colorInDepthY < m_colorHeight ){
                // calculate index into color array
                LONG colorIndex = colorInDepthX + colorInDepthY * m_colorWidth;

                // set source for copy to the color pixel
                LONG* pSrc = ( LONG * )m_colorRGBX + colorIndex;
                *pDest = *pSrc;
            } else{
                *pDest = 0;
            }
            pDest++;
        }
    }
    pd3dimmediateContext->Unmap( m_pColorTex, NULL );



	pd3dimmediateContext->IASetInputLayout(m_pScreenQuadIL);
	pd3dimmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	UINT stride = 0;
	UINT offset = 0;
	pd3dimmediateContext->IASetVertexBuffers(0, 1, &m_pScreenQuadVB, &stride, &offset);
	pd3dimmediateContext->VSSetShader(m_pPassVS, NULL, 0);
	pd3dimmediateContext->GSSetShader(m_pScreenQuadGS, NULL, 0);
	pd3dimmediateContext->PSSetShader(m_pPS, NULL, 0);
	pd3dimmediateContext->OMSetRenderTargets(1, &m_pRGBDRTV, NULL);
	pd3dimmediateContext->PSSetShaderResources(0, 1, &m_pColorSRV);
	pd3dimmediateContext->PSSetShaderResources(1, 1, &m_pDepthSRV);
	pd3dimmediateContext->RSSetViewports(1, &m_Viewport);
	pd3dimmediateContext->Draw(1, 0);

    return hr;
}

bool KinectSensor::UpdateTextures( ID3D11DeviceContext* pd3dimmediateContext, bool defaultReg=true, bool color=true, bool depth=true, bool infrared = false ){
    m_bUpdated = true;

    if (m_bPaused) return false;
    bool needToMapColorToDepth = true;
    if (depth){
        if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextDepthFrameEvent, 0)){
            // if we have received any valid new depth data we may need to draw
            if (SUCCEEDED(ProcessDepth(pd3dimmediateContext))){ needToMapColorToDepth = true; }
        }
    }
    if (color){
        if (WAIT_OBJECT_0 == WaitForSingleObject(m_hNextColorFrameEvent, 0)){
            // if we have received any valid new color data we may need to draw
            if (SUCCEEDED(ProcessColor(pd3dimmediateContext))){ needToMapColorToDepth = true; }
        }
    }
    if(!(color && m_bColorReceived) || !(depth && m_bDepthReceived)){
        m_bUpdated = false;
    }
    // If we have not yet received any data for either color or depth since we started up, we shouldn't draw
    if (!m_bDepthReceived || !m_bColorReceived){
        needToMapColorToDepth = false;
    }

    if (defaultReg && needToMapColorToDepth){
        MapColorToDepth(pd3dimmediateContext);
    }
    return m_bUpdated;
}


LRESULT KinectSensor::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    UNREFERENCED_PARAMETER(hWnd);

    switch (uMsg)
    {
    case WM_KEYDOWN:
        int nKey = static_cast< int >(wParam);

        if (nKey == 'N')
        {
            this->ToggleNearMode();
        } else if (nKey == 'P'){
            this->m_bPaused = !this->m_bPaused;
        } else if (nKey == 'I'){
            m_bInfraded = !m_bInfraded;
            if (m_bInfraded){
                m_pNuiSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR_INFRARED,cColorResolution,0,2,
                                                  m_hNextColorFrameEvent,&m_pColorStreamHandle);
            } else{
                m_pNuiSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR,cColorResolution,0,2,
                                                  m_hNextColorFrameEvent,&m_pColorStreamHandle);
            }
        }
        break;
    }
    return 0;
}

ID3D11ShaderResourceView** KinectSensor::getColor_ppSRV(){
    return &m_pColorSRV;
}

ID3D11ShaderResourceView** KinectSensor::getDepth_ppSRV(){
    return &m_pDepthSRV;
}

ID3D11ShaderResourceView** KinectSensor::getInfrared_ppSRV(){
	return &m_pDepthSRV;
}





ID3D11ShaderResourceView** KinectSensor::getRGBD_ppSRV(){
	return &m_pRGBDSRV;
}

HRESULT KinectSensor::CompileFormString(string code,
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

void KinectSensor::GenShaderCode(){
	std::stringstream shaderCode;
	shaderCode << "Texture2D<float4> txColor : register( t0 );\n";
	shaderCode << "Texture2D<uint> txDepth : register( t1 );\n";
	shaderCode << "static const float2 reso =float2(" << m_depthWidth << "," << m_depthHeight << ");\n";
	shaderCode << "struct GS_INPUT{};\n";
	shaderCode << "struct PS_INPUT{   \n";
	shaderCode << "	float4 Pos : SV_POSITION;\n";
	shaderCode << "	float2 Tex : TEXCOORD0;   \n";
	shaderCode << "};\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "// Pass Through Vertex Shader, used in pass 0, pass 1 and pass 2\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "GS_INPUT VS(){   \n";
	shaderCode << "	GS_INPUT output = (GS_INPUT)0;   \n";
	shaderCode << "	return output;   \n";
	shaderCode << "}\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "// Quad Geometry Shader, used in pass 0 and pass 2\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
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
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "// Undistorted Merge Pixel Shader, \n";
	shaderCode << "// generate undistorted RGBD texture, used in pass 0\n";
	shaderCode << "//--------------------------------------------------------------------------------------\n";
	shaderCode << "float4 PS(PS_INPUT input) : SV_Target{ \n";
	shaderCode << "	float4 col = txColor.Load(int3(input.Tex,0));\n";
	shaderCode << " col.w = txDepth.Load( int3(input.Tex,0) ) / 8000.f;\n";
	shaderCode << " return col;\n";
	shaderCode << "}\n";
	m_strShaderCode = shaderCode.str();
}


IRGBDStreamForOpenCV* OpenCVStreamFactory::createFromKinect(){
    return new KinectSensor();
}

IRGBDStreamForDirectX* DirectXStreamFactory::createFromKinect(){
    return new KinectSensor();
}