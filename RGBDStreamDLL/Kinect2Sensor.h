#pragma once
#include <opencv2\core\core.hpp>
#include <d3d11.h>
#include "DXUT.h"
#include <Kinect.h>
#include "IRGBDStreamForOpenCV.h"
#include "IRGBDStreamForDirectX.h"

// Safe release for interfaces
template<class Interface>
inline void SafeRelease( Interface *& pInterfaceToRelease )
{
	if( pInterfaceToRelease != NULL )
	{
		pInterfaceToRelease->Release();
		pInterfaceToRelease = NULL;
	}
};

class Kinect2Sensor : public IRGBDStreamForOpenCV, public IRGBDStreamForDirectX{
	static const int        m_cDepthWidth = 512;
	static const int        m_cDepthHeight = 424;
	static const int        m_cColorWidth = 1920;
	static const int        m_cColorHeight = 1080;
public:
	// Current Kinect
	IKinectSensor*                      m_pKinect2Sensor;
	ICoordinateMapper*                  m_pCoordinateMapper;
	ColorSpacePoint*                    m_pColorCoordinates;

	// Frame reader
	IMultiSourceFrameReader*            m_pMultiSourceFrameReader;

	cv::Mat                             m_matColor;
	cv::Mat                             m_matDepth;
	cv::Mat                             m_matInfrared;

	// Color and depth resource for Directx
	ID3D11Texture2D*                    m_pDepthTex;
	ID3D11ShaderResourceView*           m_pDepthSRV;

	ID3D11Texture2D*                    m_pColorTex;
	ID3D11ShaderResourceView*           m_pColorSRV;

	ID3D11Texture2D*                    m_pInfraredTex;
	ID3D11ShaderResourceView*           m_pInfraredSRV;

	UINT16*                             m_pDepthBuffer;
	UINT                                m_uDepthBufferSize;
	BYTE*                               m_pColorBuffer;
	UINT                                m_uColorBufferSize;
	UINT16*                             m_pInfraredBuffer;
	UINT                                m_pInfraredBufferSize;

	bool                                m_bLongExposureInfrared;
	bool                                m_bUpdated;


	Kinect2Sensor();
	virtual HRESULT Initialize();
	virtual void GetColorReso( int& iColorWidth, int& iColorHeight );
	virtual void GetDepthReso( int& iDepthWidth, int& iDepthHeight );
	virtual void GetInfraredReso(int& iInfraredWidth, int& iInfraredHeight);
	// For OpenCV
	HRESULT ProcessDepth(const UINT16* pDepthBuffer, int iDepthBufferSize);
	HRESULT ProcessColor(const BYTE* pColorBuffer, int iColorBufferSize);
	HRESULT ProcessInfrared(const UINT16* pInfraredBuffer, int iInfraredBufferSize);
	HRESULT MapColorToDepth();
	// For OpenCV interface
	virtual void GetColorMat( cv::Mat& out );
	virtual void GetDepthMat( cv::Mat& out );
	virtual void GetInfraredMat(cv::Mat& out);
	virtual bool UpdateMats(bool defaultReg, bool color, bool depth, bool infrared);

	// For DirectX
	HRESULT ProcessDepth( ID3D11DeviceContext* pd3dimmediateContext, const UINT16* pDepthBuffer, int iDepthBufferSize );
	HRESULT ProcessColor( ID3D11DeviceContext* pd3dimmediateContext, const BYTE* pColorBuffer, int iColorBufferSize );
	HRESULT ProcessInfrared( ID3D11DeviceContext* pd3dimmediateContext, const UINT16* pInfraredBuffer, int iInfraredBufferSize);
	HRESULT MapColorToDepth( ID3D11DeviceContext* pd3dimmediateContext );
	// For DirectX interface
	virtual HRESULT CreateResource( ID3D11Device* );
	virtual bool UpdateTextures( ID3D11DeviceContext*,
								 bool defaultReg, bool color, bool depth, bool infrared);
	virtual void Release();
	virtual LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
	virtual ID3D11ShaderResourceView** getColor_ppSRV();
	virtual ID3D11ShaderResourceView** getDepth_ppSRV();
	virtual ID3D11ShaderResourceView** getInfrared_ppSRV();

	virtual ~Kinect2Sensor();

};

Kinect2Sensor::Kinect2Sensor(){
	DWORD width = 0;
	DWORD height = 0;

	m_pKinect2Sensor = NULL;
	m_pCoordinateMapper = NULL;
	m_pColorCoordinates = NULL;

	m_pMultiSourceFrameReader = NULL;

	m_pDepthTex = NULL;
	m_pDepthSRV = NULL;
	m_pColorTex = NULL;
	m_pColorSRV = NULL;
	m_pInfraredTex = NULL;
	m_pInfraredSRV = NULL;

	m_matColor.create( m_cColorHeight, m_cColorWidth, CV_8UC3 );
	m_matDepth.create( m_cDepthHeight, m_cDepthWidth, CV_16UC1 );
	m_matInfrared.create( m_cDepthHeight, m_cDepthWidth, CV_16UC1 );

	m_pColorCoordinates = new ColorSpacePoint[m_cDepthWidth * m_cDepthHeight];
	m_pColorBuffer = new BYTE[m_cColorWidth * m_cColorHeight*4];

	m_bLongExposureInfrared = false;
}

Kinect2Sensor::~Kinect2Sensor(){
	if( m_pColorCoordinates ){
		delete[] m_pColorCoordinates;
		m_pColorCoordinates = NULL;
	}
	if( m_pColorBuffer ){
		delete[] m_pColorBuffer;
		m_pColorBuffer = NULL;
	}
	// done with frame reader
	SafeRelease( m_pMultiSourceFrameReader );

	// done with coordinate mapper
	SafeRelease( m_pCoordinateMapper );

	// close the Kinect Sensor
	if( m_pKinect2Sensor )
	{
		m_pKinect2Sensor->Close();
	}

	SafeRelease( m_pKinect2Sensor );
}


HRESULT Kinect2Sensor::Initialize(){

	HRESULT hr;

	hr = GetDefaultKinectSensor( &m_pKinect2Sensor );
	if( FAILED( hr ) ){return hr;}

	if( m_pKinect2Sensor ){
		// Initialize the Kinect and get coordinate mapper and the frame reader
		if( SUCCEEDED( hr ) ) hr = m_pKinect2Sensor->get_CoordinateMapper( &m_pCoordinateMapper );

		hr = m_pKinect2Sensor->Open();

		if( SUCCEEDED( hr ) ){
			if( m_bLongExposureInfrared)hr = m_pKinect2Sensor->OpenMultiSourceFrameReader(
				FrameSourceTypes::FrameSourceTypes_Depth | FrameSourceTypes::FrameSourceTypes_Color | FrameSourceTypes::FrameSourceTypes_LongExposureInfrared,
				&m_pMultiSourceFrameReader );
			else hr = m_pKinect2Sensor->OpenMultiSourceFrameReader(
				FrameSourceTypes::FrameSourceTypes_Depth | FrameSourceTypes::FrameSourceTypes_Color | FrameSourceTypes::FrameSourceTypes_Infrared,
				&m_pMultiSourceFrameReader);
		}
	}

	if( !m_pKinect2Sensor || FAILED( hr ) )
	{
		return E_FAIL;
	}

	return hr;
}

void Kinect2Sensor::GetColorReso( int& iColorWidth, int& iColorHeight )
{
	iColorWidth = m_cColorWidth;
	iColorHeight = m_cColorHeight;
}

void Kinect2Sensor::GetDepthReso( int& iDepthWidth, int& iDepthHeight )
{
	iDepthWidth = m_cDepthWidth;
	iDepthHeight = m_cDepthHeight;
}

void Kinect2Sensor::GetInfraredReso(int& iInfraredWidth, int& iInfraredHeight)
{
	iInfraredWidth = m_cDepthWidth;
	iInfraredHeight = m_cDepthHeight;
}

// For OpenCV code
HRESULT Kinect2Sensor::ProcessDepth(const UINT16* pDepthBuffer, int iDepthBufferSize)
{
	HRESULT hr = S_OK;
	for( UINT y = 0; y < m_cDepthHeight; ++y ){
		// Get row pointer for depth Mat
		USHORT* pDepthRow = m_matDepth.ptr<USHORT>( y );
		for( UINT x = 0; x < m_cDepthWidth; ++x ){
			pDepthRow[x] = pDepthBuffer[y * m_cDepthWidth + x];
		}
	}
	return hr;
}

HRESULT Kinect2Sensor::ProcessColor(const BYTE* pColorBuffer, int iColorBufferSize)
{
	HRESULT hr = S_OK;
	for( UINT y = 0; y < m_cColorHeight; ++y ){
		cv::Vec3b* pColorRow = m_matColor.ptr<cv::Vec3b>( y );
		for( UINT x = 0; x < m_cColorWidth; ++x ){
			pColorRow[x] = cv::Vec3b( pColorBuffer[y*m_cColorWidth*4 + x * 4 + 0],
									  pColorBuffer[y*m_cColorWidth*4 + x * 4 + 1],
									  pColorBuffer[y*m_cColorWidth*4 + x * 4 + 2] );
		}
	}
	return hr;
}

HRESULT Kinect2Sensor::ProcessInfrared(const UINT16* pInfraredBuffer, int iInfraredBufferSize)
{
	HRESULT hr = S_OK;
	for( UINT y = 0; y < m_cDepthHeight; ++y ){
		// Get row pointer for depth Mat
		USHORT* pDepthRow = m_matInfrared.ptr<USHORT>( y );
		for( UINT x = 0; x < m_cDepthWidth; ++x ){
			pDepthRow[x] = pInfraredBuffer[y * m_cDepthWidth + x];
		}
	}
	return hr;
}
HRESULT Kinect2Sensor::MapColorToDepth()
{
	HRESULT hr = S_OK;
	return hr;
}

bool Kinect2Sensor::UpdateMats( bool defaultReg = true, bool color = true, bool depth = true, bool infrared = false )
{
	m_bUpdated = true;
	bool needToMapColorToDepth = true;
	if( !m_pMultiSourceFrameReader )
	{ return false; }

	IMultiSourceFrame* pMultiSourceFrame = NULL;
	IDepthFrame* pDepthFrame = NULL;
	IColorFrame* pColorFrame = NULL;
	IInfraredFrame* pInfraredFrame = NULL;
	ILongExposureInfraredFrame* pLongExposureInfraredFrame = NULL;

	HRESULT hr = m_pMultiSourceFrameReader->AcquireLatestFrame( &pMultiSourceFrame );

	if( depth && SUCCEEDED( hr ) ){
		IDepthFrameReference* pDepthFrameReference = NULL;
		hr = pMultiSourceFrame->get_DepthFrameReference( &pDepthFrameReference );
		if( SUCCEEDED( hr ) ) hr = pDepthFrameReference->AcquireFrame( &pDepthFrame );
		SafeRelease( pDepthFrameReference );
	}

	if( color && SUCCEEDED( hr ) ){
		IColorFrameReference* pColorFrameReference = NULL;
		hr = pMultiSourceFrame->get_ColorFrameReference( &pColorFrameReference );
		if( SUCCEEDED( hr ) ) hr = pColorFrameReference->AcquireFrame( &pColorFrame );
		SafeRelease( pColorFrameReference );
	}

	if( infrared && SUCCEEDED( hr )){
		if(m_bLongExposureInfrared){
			ILongExposureInfraredFrameReference* pLongExposureInfraredFrameReference = NULL;
			hr = pMultiSourceFrame->get_LongExposureInfraredFrameReference(&pLongExposureInfraredFrameReference);
			if (SUCCEEDED(hr)) hr = pLongExposureInfraredFrameReference->AcquireFrame(&pLongExposureInfraredFrame);
		} else{
			IInfraredFrameReference* pInfraredFrameReference = NULL;
			hr = pMultiSourceFrame->get_InfraredFrameReference(&pInfraredFrameReference);
			if (SUCCEEDED(hr)) hr = pInfraredFrameReference->AcquireFrame(&pInfraredFrame);
		}
	}
	if( SUCCEEDED( hr ) )
	{
		if( depth ){
			IFrameDescription* pDepthFrameDescription = NULL;
			int nDepthWidth = 0;
			int nDepthHeight = 0;
			UINT nDepthBufferSize = 0;
			UINT16 *pDepthBuffer = NULL;

			// get depth frame data
			if( SUCCEEDED( hr ) ) hr = pDepthFrame->get_FrameDescription( &pDepthFrameDescription );
			if( SUCCEEDED( hr ) ) hr = pDepthFrameDescription->get_Width( &nDepthWidth );
			if( SUCCEEDED( hr ) ) hr = pDepthFrameDescription->get_Height( &nDepthHeight );
			if( SUCCEEDED( hr ) ) hr = pDepthFrame->AccessUnderlyingBuffer( &nDepthBufferSize, &pDepthBuffer );
			if( SUCCEEDED( hr ) ) ProcessDepth( pDepthBuffer, nDepthBufferSize );
			SafeRelease( pDepthFrameDescription );
		}
		if( color ){
			IFrameDescription* pColorFrameDescription = NULL;
			int nColorWidth = 0;
			int nColorHeight = 0;
			ColorImageFormat imageFormat = ColorImageFormat_None;
			UINT nColorBufferSize = 0;
			BYTE *pColorBuffer = NULL;

			// get color frame data
			if( SUCCEEDED( hr ) ) hr = pColorFrame->get_FrameDescription( &pColorFrameDescription );
			if( SUCCEEDED( hr ) ) hr = pColorFrameDescription->get_Width( &nColorWidth );
			if( SUCCEEDED( hr ) ) hr = pColorFrameDescription->get_Height( &nColorHeight );
			if( SUCCEEDED( hr ) ) hr = pColorFrame->get_RawColorImageFormat( &imageFormat );
			if( SUCCEEDED( hr ) )
			{
				if( imageFormat == ColorImageFormat_Bgra ){
					hr = pColorFrame->AccessRawUnderlyingBuffer( &nColorBufferSize, &pColorBuffer );
				} else if( m_pColorBuffer ){
					pColorBuffer = m_pColorBuffer;
					nColorBufferSize = m_cColorWidth * m_cColorHeight * 4 * sizeof( BYTE );
					hr = pColorFrame->CopyConvertedFrameDataToArray( nColorBufferSize, pColorBuffer, ColorImageFormat_Bgra );
				} else{
					hr = E_FAIL;
				}
			}
			if( SUCCEEDED( hr ) ) ProcessColor( pColorBuffer, nColorBufferSize );
			SafeRelease( pColorFrameDescription );
		}
		if( infrared ){
			IFrameDescription* pInfraredFrameDescription = NULL;
			int nInfraredWidth = 0;
			int nInfraredHeight = 0;
			UINT nInfraredBufferSize = 0;
			UINT16 *pInfraredBuffer = NULL;

			// get Infrared frame data
			if(m_bLongExposureInfrared){
				if (SUCCEEDED(hr)) hr = pLongExposureInfraredFrame->get_FrameDescription(&pInfraredFrameDescription);
			} else{
				if( SUCCEEDED( hr ) ) hr = pInfraredFrame->get_FrameDescription( &pInfraredFrameDescription );
			}
			if( SUCCEEDED( hr ) ) hr = pInfraredFrameDescription->get_Width( &nInfraredWidth );
			if( SUCCEEDED( hr ) ) hr = pInfraredFrameDescription->get_Height( &nInfraredHeight );
			if(m_bLongExposureInfrared){
				if (SUCCEEDED(hr)) hr = pLongExposureInfraredFrame->AccessUnderlyingBuffer(&nInfraredBufferSize, &pInfraredBuffer);
			} else{
				if( SUCCEEDED( hr ) ) hr = pInfraredFrame->AccessUnderlyingBuffer( &nInfraredBufferSize, &pInfraredBuffer );
			}
			if( SUCCEEDED( hr ) ) ProcessInfrared( pInfraredBuffer, nInfraredBufferSize );
			SafeRelease( pInfraredFrameDescription );
		}
	}

	SafeRelease( pDepthFrame );
	SafeRelease( pColorFrame );
	SafeRelease(pInfraredFrame);
	SafeRelease(pLongExposureInfraredFrame);
	SafeRelease( pMultiSourceFrame );

	if( !SUCCEEDED( hr ) ){
		m_bUpdated = false;
		needToMapColorToDepth = false;
	}
	if( defaultReg && needToMapColorToDepth ){
		MapColorToDepth( );
	}
	return m_bUpdated;
}

void Kinect2Sensor::GetDepthMat( cv::Mat& out ){
	m_matDepth.copyTo( out );
}

void Kinect2Sensor::GetColorMat( cv::Mat& out ){
	m_matColor.copyTo( out );
}

void Kinect2Sensor::GetInfraredMat( cv::Mat& out ){
	m_matInfrared.copyTo( out );
}

HRESULT Kinect2Sensor::CreateResource( ID3D11Device* pd3dDevice ){
	HRESULT hr = S_OK;

	// Create depth texture and its resource view
	D3D11_TEXTURE2D_DESC depthTexDesc = { 0 };
	depthTexDesc.Width = m_cDepthWidth;
	depthTexDesc.Height = m_cDepthHeight;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.Format = DXGI_FORMAT_R16_UINT;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Usage = D3D11_USAGE_DYNAMIC;
	depthTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	depthTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	depthTexDesc.MiscFlags = 0;
	V_RETURN( pd3dDevice->CreateTexture2D( &depthTexDesc, NULL, &m_pDepthTex ) );
	V_RETURN( pd3dDevice->CreateShaderResourceView( m_pDepthTex, NULL, &m_pDepthSRV ) );

	// Create color texture and its resource view
	D3D11_TEXTURE2D_DESC colorTexDesc = { 0 };
	colorTexDesc.Width = m_cColorWidth;
	colorTexDesc.Height = m_cColorHeight;
	colorTexDesc.MipLevels = 1;
	colorTexDesc.ArraySize = 1;
	colorTexDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	colorTexDesc.SampleDesc.Count = 1;
	colorTexDesc.SampleDesc.Quality = 0;
	colorTexDesc.Usage = D3D11_USAGE_DYNAMIC;
	colorTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	colorTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	colorTexDesc.MiscFlags = 0;
	V_RETURN( pd3dDevice->CreateTexture2D( &colorTexDesc, NULL, &m_pColorTex ) );
	V_RETURN( pd3dDevice->CreateShaderResourceView( m_pColorTex, NULL, &m_pColorSRV ) );

	// Create infrared texture and its resource view
	D3D11_TEXTURE2D_DESC infraredTexDesc = { 0 };
	infraredTexDesc.Width = m_cDepthWidth;
	infraredTexDesc.Height = m_cDepthHeight;
	infraredTexDesc.MipLevels = 1;
	infraredTexDesc.ArraySize = 1;
	infraredTexDesc.Format = DXGI_FORMAT_R16_UINT;
	infraredTexDesc.SampleDesc.Count = 1;
	infraredTexDesc.SampleDesc.Quality = 0;
	infraredTexDesc.Usage = D3D11_USAGE_DYNAMIC;
	infraredTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	infraredTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	infraredTexDesc.MiscFlags = 0;
	V_RETURN( pd3dDevice->CreateTexture2D( &infraredTexDesc, NULL, &m_pInfraredTex ) );
	V_RETURN( pd3dDevice->CreateShaderResourceView( m_pInfraredTex, NULL, &m_pInfraredSRV ) );

	return hr;
}


void Kinect2Sensor::Release(){
	SAFE_RELEASE( m_pDepthTex );
	SAFE_RELEASE( m_pDepthSRV );
	SAFE_RELEASE( m_pColorTex );
	SAFE_RELEASE( m_pColorSRV );
	SAFE_RELEASE( m_pInfraredTex );
	SAFE_RELEASE( m_pInfraredSRV );
}

HRESULT Kinect2Sensor::ProcessDepth( ID3D11DeviceContext* pd3dimmediateContext, const UINT16* pDepthBuffer, int iDepthBufferSize ){
	HRESULT hr = S_OK;
	D3D11_MAPPED_SUBRESOURCE msT;
	hr = pd3dimmediateContext->Map( m_pDepthTex, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &msT );
	if( FAILED( hr ) )	 { return hr; }
	// SDK said iDepthBufferSize indicates the size in byte, but it turns out is the size of UINT16, so multiply by 2
	memcpy( msT.pData, pDepthBuffer, iDepthBufferSize*2 );
	pd3dimmediateContext->Unmap( m_pDepthTex, NULL );

	return hr;
}

HRESULT Kinect2Sensor::ProcessColor( ID3D11DeviceContext* pd3dimmediateContext, const BYTE* pColorBuffer, int iColorBufferSize ){
	HRESULT hr = S_OK;
	D3D11_MAPPED_SUBRESOURCE msT;
	hr = pd3dimmediateContext->Map( m_pColorTex, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &msT );
	if( FAILED( hr ) )	 { return hr; }
	memcpy( msT.pData, pColorBuffer, iColorBufferSize );
	pd3dimmediateContext->Unmap( m_pColorTex, NULL );

	return hr;
}

HRESULT Kinect2Sensor::ProcessInfrared( ID3D11DeviceContext* pd3dimmediateContext, const UINT16* pInfraredBuffer, int iInfraredBufferSize ){
	HRESULT hr = S_OK;
	D3D11_MAPPED_SUBRESOURCE msT;
	hr = pd3dimmediateContext->Map( m_pInfraredTex, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &msT );
	if( FAILED( hr ) )	 { return hr; }
	// SDK said iInfraredBufferSize indicates the size in byte, but it turns out is the size of UINT16, so multiply by 2
	memcpy( msT.pData, pInfraredBuffer, iInfraredBufferSize * 2 );
	pd3dimmediateContext->Unmap( m_pInfraredTex, NULL );

	return hr;
}

HRESULT Kinect2Sensor::MapColorToDepth( ID3D11DeviceContext* pd3dimmediateContext ){
	HRESULT hr = S_OK;
	
	return hr;
}

bool Kinect2Sensor::UpdateTextures( ID3D11DeviceContext* pd3dimmediateContext, bool defaultReg = true, bool color = true, bool depth = true, bool infrared = false ){
	m_bUpdated = true;
	bool needToMapColorToDepth = true;
	if (!m_pMultiSourceFrameReader)
	{ return false;}

	IMultiSourceFrame* pMultiSourceFrame = NULL;
	IDepthFrame* pDepthFrame = NULL;
	IColorFrame* pColorFrame = NULL;
	IInfraredFrame* pInfraredFrame = NULL;

	HRESULT hr = m_pMultiSourceFrameReader->AcquireLatestFrame( &pMultiSourceFrame );

	if( depth && SUCCEEDED( hr ) ){
		IDepthFrameReference* pDepthFrameReference = NULL;
		hr = pMultiSourceFrame->get_DepthFrameReference( &pDepthFrameReference );
		if( SUCCEEDED( hr ) ) hr = pDepthFrameReference->AcquireFrame( &pDepthFrame );
		SafeRelease( pDepthFrameReference );
	}

	if( color && SUCCEEDED( hr ) ){
		IColorFrameReference* pColorFrameReference = NULL;
		hr = pMultiSourceFrame->get_ColorFrameReference( &pColorFrameReference );
		if( SUCCEEDED( hr ) ) hr = pColorFrameReference->AcquireFrame( &pColorFrame );
		SafeRelease( pColorFrameReference );
	}

	if( infrared && SUCCEEDED( hr ) ){
		IInfraredFrameReference* pInfraredFrameReference = NULL;
		hr = pMultiSourceFrame->get_InfraredFrameReference( &pInfraredFrameReference );
		if( SUCCEEDED( hr ) ) hr = pInfraredFrameReference->AcquireFrame( &pInfraredFrame );
	}
	if( SUCCEEDED( hr ) )
	{
		if( depth ){
			IFrameDescription* pDepthFrameDescription = NULL;
			int nDepthWidth = 0;
			int nDepthHeight = 0;
			UINT nDepthBufferSize = 0;
			UINT16 *pDepthBuffer = NULL;

			// get depth frame data
			if( SUCCEEDED( hr ) ) hr = pDepthFrame->get_FrameDescription( &pDepthFrameDescription );
			if( SUCCEEDED( hr ) ) hr = pDepthFrameDescription->get_Width( &nDepthWidth );
			if( SUCCEEDED( hr ) ) hr = pDepthFrameDescription->get_Height( &nDepthHeight );
			if( SUCCEEDED( hr ) ) hr = pDepthFrame->AccessUnderlyingBuffer( &nDepthBufferSize, &pDepthBuffer );
			if( SUCCEEDED( hr ) ) ProcessDepth( pd3dimmediateContext, pDepthBuffer, nDepthBufferSize );
			SafeRelease( pDepthFrameDescription );
		}
		if( color ){
			IFrameDescription* pColorFrameDescription = NULL;
			int nColorWidth = 0;
			int nColorHeight = 0;
			ColorImageFormat imageFormat = ColorImageFormat_None;
			UINT nColorBufferSize = 0;
			BYTE *pColorBuffer = NULL;

			// get color frame data
			if( SUCCEEDED( hr ) ) hr = pColorFrame->get_FrameDescription( &pColorFrameDescription );
			if( SUCCEEDED( hr ) ) hr = pColorFrameDescription->get_Width( &nColorWidth );
			if( SUCCEEDED( hr ) ) hr = pColorFrameDescription->get_Height( &nColorHeight );
			if( SUCCEEDED( hr ) ) hr = pColorFrame->get_RawColorImageFormat( &imageFormat );
			if( SUCCEEDED( hr ) )
			{
				if( imageFormat == ColorImageFormat_Bgra ){
					hr = pColorFrame->AccessRawUnderlyingBuffer( &nColorBufferSize, &pColorBuffer );
				} else if( m_pColorBuffer ){
					pColorBuffer = m_pColorBuffer;
					nColorBufferSize = m_cColorWidth * m_cColorHeight * 4 * sizeof( BYTE );
					hr = pColorFrame->CopyConvertedFrameDataToArray( nColorBufferSize, pColorBuffer, ColorImageFormat_Bgra );
				} else{
					hr = E_FAIL;
				}
			}
			if( SUCCEEDED( hr ) ) ProcessColor( pd3dimmediateContext, pColorBuffer, nColorBufferSize );
			SafeRelease( pColorFrameDescription );
		}
		if( infrared ){
			IFrameDescription* pInfraredFrameDescription = NULL;
			int nInfraredWidth = 0;
			int nInfraredHeight = 0;
			UINT nInfraredBufferSize = 0;
			UINT16 *pInfraredBuffer = NULL;

			// get Infrared frame data
			if( SUCCEEDED( hr ) ) hr = pInfraredFrame->get_FrameDescription( &pInfraredFrameDescription );
			if( SUCCEEDED( hr ) ) hr = pInfraredFrameDescription->get_Width( &nInfraredWidth );
			if( SUCCEEDED( hr ) ) hr = pInfraredFrameDescription->get_Height( &nInfraredHeight );
			if( SUCCEEDED( hr ) ) hr = pInfraredFrame->AccessUnderlyingBuffer( &nInfraredBufferSize, &pInfraredBuffer );
			if( SUCCEEDED( hr ) ) ProcessInfrared( pd3dimmediateContext, pInfraredBuffer, nInfraredBufferSize );
			SafeRelease( pInfraredFrameDescription );
		}
	}

	SafeRelease( pDepthFrame );
	SafeRelease( pColorFrame );
	SafeRelease( pInfraredFrame );
	SafeRelease( pMultiSourceFrame );

	if( !SUCCEEDED( hr ) ){
		m_bUpdated = false;
		needToMapColorToDepth = false;
	}
	if( defaultReg && needToMapColorToDepth ){
		MapColorToDepth();
	}
	return m_bUpdated;
}


LRESULT Kinect2Sensor::HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	UNREFERENCED_PARAMETER( lParam );
	UNREFERENCED_PARAMETER( hWnd );

	switch( uMsg )
	{
	case WM_KEYDOWN:
		int nKey = static_cast< int >( wParam );
		break;
	}
	return 0;
}

ID3D11ShaderResourceView** Kinect2Sensor::getColor_ppSRV(){
	return &m_pColorSRV;
}

ID3D11ShaderResourceView** Kinect2Sensor::getDepth_ppSRV(){
	return &m_pDepthSRV;
}

ID3D11ShaderResourceView** Kinect2Sensor::getInfrared_ppSRV(){
	return &m_pInfraredSRV;
}

IRGBDStreamForOpenCV* OpenCVStreamFactory::createFromKinect2(){
	return new Kinect2Sensor();
}

IRGBDStreamForDirectX* DirectXStreamFactory::createFromKinect2(){
	return new Kinect2Sensor();
}