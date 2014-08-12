/* Helper class for Kinect2, include functions to communicate
 * with DirectX and OpenCV.
 * Requires DirectX11 OpenCV 1.x
 * author Peng Liu <peng.liu916@gmail.com>
 */
#pragma once
#include <opencv2\core\core.hpp>
#include <d3d11.h>
#include "DXUT.h"
#include "SDKmisc.h"
#include <Kinect.h>
#include <iostream>
#include "IRGBDStreamForOpenCV.h"
#include "IRGBDStreamForDirectX.h"

#define COMPILE_FLAG D3DCOMPILE_ENABLE_STRICTNESS
using namespace std;

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

	static const float      m_fDistorParams[5];

public:
	// Current Kinect
	IKinectSensor*                      m_pKinect2Sensor;
	ICoordinateMapper*                  m_pCoordinateMapper;

	// Frame reader
	IMultiSourceFrameReader*            m_pMultiSourceFrameReader;



	UINT16*                             m_pDepthBuffer;
	UINT                                m_uDepthBufferSize;
	BYTE*                               m_pColorBuffer;
	UINT                                m_uColorBufferSize;
	UINT16*                             m_pInfraredBuffer;
	UINT                                m_pInfraredBufferSize;

	bool                                m_bLongExposureInfrared;
	bool                                m_bUpdated;

	// For OpenCV Interface
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

	ID3D11Texture2D*                    m_pColLookupForDetphTex;
	ID3D11ShaderResourceView*           m_pColLookupForDepthSRV;

	ID3D11Texture2D*                    m_pUndsitortedRGBDTex;
	ID3D11ShaderResourceView*           m_pUndsitortedRGBDSRV;
	ID3D11RenderTargetView*             m_pUndsitortedRGBDRTV;

	// DirectX resource for undistorted rgbd texture
	ID3D11VertexShader*                 m_pPassVS;
	ID3D11GeometryShader*               m_pQuadGS;
	ID3D11PixelShader*                  m_pTexPS;
	ID3D11InputLayout*                  m_pPassIL;
	ID3D11Buffer*                       m_pPassVB;
	ID3D11SamplerState*                 m_pNearestNeighborSS;
	D3D11_VIEWPORT                      m_RTviewport;

	ID3D11ShaderResourceView**          m_ppInputSRV;
	ID3D11ShaderResourceView**          m_ppNullSRV;

	string                              m_strShaderCode;

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


	// For Testing not in the IRGBDStreamForDirectX
	ColorSpacePoint*                    m_pColorCoordinates;
	virtual ID3D11ShaderResourceView** getColorLookup_ppSRV();
	HRESULT ProcessDepthColorMap(ID3D11DeviceContext* pd3dimmediatecontext, const ColorSpacePoint* pMapBuffer, int iMapBufferSize);
	string GenShaderCode();
	void ProcessUndistortedRGBD(ID3D11DeviceContext* pd3dimmediatecontext);

};
const float Kinect2Sensor::m_fDistorParams[]= { 8.5154194367447283e-002, -2.6706920315697513e-001,
												3.0158565240179448e-003, -1.3463984002856915e-003,
												9.9116038520186175e-002 };
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

// General interface
IRGBDStreamForOpenCV* OpenCVStreamFactory::createFromKinect2(){
	return new Kinect2Sensor();
}
IRGBDStreamForDirectX* DirectXStreamFactory::createFromKinect2(){
	return new Kinect2Sensor();
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

// For OpenCV interface
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
bool Kinect2Sensor::UpdateMats( bool defaultReg = true, bool color = true, bool depth = true, bool infrared = true )
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
	return SUCCEEDED( hr );
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

// For DirectX interface
HRESULT Kinect2Sensor::CreateResource( ID3D11Device* pd3dDevice ){
	HRESULT hr = S_OK;



	ID3DBlob* pVSBlob = NULL;
	wstring filename = L"..\\RGBDStreamDLL\\UndistortedMerge.fx";

	V_RETURN( DXUTCompileFromFile( filename.c_str(), nullptr, "VS", "vs_5_0", COMPILE_FLAG, 0, &pVSBlob ) );
	V_RETURN( pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pPassVS ) );
	DXUT_SetDebugName( m_pPassVS, "m_pPassVS" );

	ID3DBlob* pGSBlob = NULL;
	V_RETURN( DXUTCompileFromFile( filename.c_str(), nullptr, "GS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob ) );
	V_RETURN( pd3dDevice->CreateGeometryShader( pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pQuadGS ) );
	DXUT_SetDebugName( m_pQuadGS, "m_pQuadGS" );
	pGSBlob->Release();

	ID3DBlob* pPSBlob = NULL;
	V_RETURN( DXUTCompileFromFile( filename.c_str(), nullptr, "PS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pTexPS ) );
	DXUT_SetDebugName( m_pTexPS, "m_pTexPS" );
	pPSBlob->Release();

	D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
	V_RETURN( pd3dDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pPassIL ) );
	DXUT_SetDebugName( m_pPassIL, "m_pPassIL" );
	pVSBlob->Release();

	// Create the vertex buffer
	D3D11_BUFFER_DESC bd = { 0 };
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof( short );
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	V_RETURN( pd3dDevice->CreateBuffer( &bd, NULL, &m_pPassVB ) );
	DXUT_SetDebugName( m_pPassVB, "m_pPassVB" );





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
	DXUT_SetDebugName( m_pDepthTex, "m_pDepthTex" );
	DXUT_SetDebugName( m_pDepthSRV, "m_pDepthSRV" );

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
	DXUT_SetDebugName( m_pColorTex, "m_pColorTex" );
	DXUT_SetDebugName( m_pColorSRV, "m_pColorSRV" );

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
	DXUT_SetDebugName( m_pInfraredTex, "m_pInfraredTex" );
	DXUT_SetDebugName( m_pInfraredSRV, "m_pInfraredSRV" );

	// Create depth color mapping texture and its resource view
	D3D11_TEXTURE2D_DESC depthColorMappingDesc = {0};
	depthColorMappingDesc.Width = m_cDepthWidth;
	depthColorMappingDesc.Height = m_cDepthHeight;
	depthColorMappingDesc.MipLevels = 1;
	depthColorMappingDesc.ArraySize = 1;
	depthColorMappingDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	depthColorMappingDesc.SampleDesc.Count = 1;
	depthColorMappingDesc.SampleDesc.Quality = 0;
	depthColorMappingDesc.Usage = D3D11_USAGE_DYNAMIC;
	depthColorMappingDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	depthColorMappingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	depthColorMappingDesc.MiscFlags = 0;
	V_RETURN( pd3dDevice->CreateTexture2D( &depthColorMappingDesc, NULL, &m_pColLookupForDetphTex));
	V_RETURN( pd3dDevice->CreateShaderResourceView( m_pColLookupForDetphTex, NULL,&m_pColLookupForDepthSRV));
	DXUT_SetDebugName( m_pColLookupForDetphTex, "m_pColLookupForDetphTex" );
	DXUT_SetDebugName( m_pColLookupForDepthSRV, "m_pColLookupForDepthSRV" );

	// Create depth color mapping texture and its resource view
	D3D11_TEXTURE2D_DESC undistortedRGBDDesc = { 0 };
	undistortedRGBDDesc.Width = m_cDepthWidth;
	undistortedRGBDDesc.Height = m_cDepthHeight;
	undistortedRGBDDesc.MipLevels = 1;
	undistortedRGBDDesc.ArraySize = 1;
	undistortedRGBDDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	undistortedRGBDDesc.SampleDesc.Count = 1;
	undistortedRGBDDesc.SampleDesc.Quality = 0;
	undistortedRGBDDesc.Usage = D3D11_USAGE_DEFAULT;
	undistortedRGBDDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	undistortedRGBDDesc.CPUAccessFlags = 0;
	undistortedRGBDDesc.MiscFlags = 0;
	V_RETURN( pd3dDevice->CreateTexture2D( &undistortedRGBDDesc, NULL, &m_pUndsitortedRGBDTex ) );
	V_RETURN( pd3dDevice->CreateShaderResourceView( m_pUndsitortedRGBDTex, NULL, &m_pUndsitortedRGBDSRV ) );
	V_RETURN( pd3dDevice->CreateRenderTargetView( m_pUndsitortedRGBDTex, NULL, &m_pUndsitortedRGBDRTV ) );
	DXUT_SetDebugName( m_pUndsitortedRGBDTex, "m_pUndsitortedRGBDTex" );
	DXUT_SetDebugName( m_pUndsitortedRGBDSRV, "m_pUndsitortedRGBDSRV" );
	DXUT_SetDebugName( m_pUndsitortedRGBDRTV, "m_pUndsitortedRGBDRTV" );


	m_RTviewport.Width = ( float )m_cDepthWidth;
	m_RTviewport.Height = ( float )m_cDepthHeight;
	m_RTviewport.MinDepth = 0.0f;
	m_RTviewport.MaxDepth = 1.0f;
	m_RTviewport.TopLeftX = 0;
	m_RTviewport.TopLeftY = 0;

	// Create the sample state
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory( &sampDesc, sizeof( sampDesc ) );
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN( pd3dDevice->CreateSamplerState( &sampDesc, &m_pNearestNeighborSS ) );
	DXUT_SetDebugName( m_pNearestNeighborSS, "m_pNearestNeighborSS" );



	m_ppInputSRV = new ID3D11ShaderResourceView*[4];
	m_ppNullSRV = new ID3D11ShaderResourceView*[4];

	for( UINT i = 0; i < 4; i++ )
		m_ppNullSRV[i] = NULL;


	return hr;
}
void Kinect2Sensor::Release(){
	SAFE_RELEASE( m_pDepthTex );
	SAFE_RELEASE( m_pDepthSRV );
	SAFE_RELEASE( m_pColorTex );
	SAFE_RELEASE( m_pColorSRV );
	SAFE_RELEASE( m_pInfraredTex );
	SAFE_RELEASE( m_pInfraredSRV );
	SAFE_RELEASE( m_pColLookupForDetphTex );
	SAFE_RELEASE( m_pColLookupForDepthSRV );

	SAFE_RELEASE( m_pUndsitortedRGBDTex );
	SAFE_RELEASE( m_pUndsitortedRGBDSRV );
	SAFE_RELEASE( m_pUndsitortedRGBDRTV );
	SAFE_RELEASE( m_pPassVS );
	SAFE_RELEASE( m_pQuadGS );
	SAFE_RELEASE( m_pTexPS );
	SAFE_RELEASE( m_pPassIL );
	SAFE_RELEASE( m_pPassVB );
	SAFE_RELEASE( m_pNearestNeighborSS );
	delete[] m_ppInputSRV;
	delete[] m_ppNullSRV;

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
bool Kinect2Sensor::UpdateTextures( ID3D11DeviceContext* pd3dimmediateContext, bool defaultReg = true, bool color = true, bool depth = true, bool infrared = true ){
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
		IFrameDescription* pDepthFrameDescription = NULL;
		int nDepthWidth = 0;
		int nDepthHeight = 0;
		UINT nDepthBufferSize = 0;
		UINT16 *pDepthBuffer = NULL;

		IFrameDescription* pColorFrameDescription = NULL;
		int nColorWidth = 0;
		int nColorHeight = 0;
		ColorImageFormat imageFormat = ColorImageFormat_None;
		UINT nColorBufferSize = 0;
		BYTE *pColorBuffer = NULL;

		IFrameDescription* pInfraredFrameDescription = NULL;
		int nInfraredWidth = 0;
		int nInfraredHeight = 0;
		UINT nInfraredBufferSize = 0;
		UINT16 *pInfraredBuffer = NULL;

		if( depth ){
			// get depth frame data
			if( SUCCEEDED( hr ) ) hr = pDepthFrame->get_FrameDescription( &pDepthFrameDescription );
			if( SUCCEEDED( hr ) ) hr = pDepthFrameDescription->get_Width( &nDepthWidth );
			if( SUCCEEDED( hr ) ) hr = pDepthFrameDescription->get_Height( &nDepthHeight );
			if( SUCCEEDED( hr ) ) hr = pDepthFrame->AccessUnderlyingBuffer( &nDepthBufferSize, &pDepthBuffer );
			if( SUCCEEDED( hr ) ) ProcessDepth( pd3dimmediateContext, pDepthBuffer, nDepthBufferSize );
			SafeRelease( pDepthFrameDescription );
		}
		if( color ){
			// get color frame data
			if( SUCCEEDED( hr ) ) hr = pColorFrame->get_FrameDescription( &pColorFrameDescription );
			if( SUCCEEDED( hr ) ) hr = pColorFrameDescription->get_Width( &nColorWidth );
			if( SUCCEEDED( hr ) ) hr = pColorFrameDescription->get_Height( &nColorHeight );
			if( SUCCEEDED( hr ) ) hr = pColorFrame->get_RawColorImageFormat( &imageFormat );
			if( SUCCEEDED( hr ) ){
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
			// get Infrared frame data
			if( SUCCEEDED( hr ) ) hr = pInfraredFrame->get_FrameDescription( &pInfraredFrameDescription );
			if( SUCCEEDED( hr ) ) hr = pInfraredFrameDescription->get_Width( &nInfraredWidth );
			if( SUCCEEDED( hr ) ) hr = pInfraredFrameDescription->get_Height( &nInfraredHeight );
			if( SUCCEEDED( hr ) ) hr = pInfraredFrame->AccessUnderlyingBuffer( &nInfraredBufferSize, &pInfraredBuffer );
			if( SUCCEEDED( hr ) ) ProcessInfrared( pd3dimmediateContext, pInfraredBuffer, nInfraredBufferSize );
			SafeRelease( pInfraredFrameDescription );
		}
		if( defaultReg && m_pCoordinateMapper && 
			pDepthBuffer && nDepthHeight == m_cDepthHeight && nDepthWidth == m_cDepthWidth &&
			pColorBuffer && nColorHeight == m_cColorHeight && nColorWidth == m_cColorWidth){
			hr = m_pCoordinateMapper->MapDepthFrameToColorSpace( nDepthHeight*nDepthWidth, pDepthBuffer, nDepthHeight*nDepthWidth, m_pColorCoordinates );
			if(SUCCEEDED(hr)) ProcessDepthColorMap(pd3dimmediateContext,m_pColorCoordinates,m_cDepthHeight*m_cDepthWidth);
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

// For testing
ID3D11ShaderResourceView** Kinect2Sensor::getColorLookup_ppSRV(){
	return &m_pColLookupForDepthSRV;
}
HRESULT Kinect2Sensor::ProcessDepthColorMap(ID3D11DeviceContext* pd3dimmediatecontext, const ColorSpacePoint* pMapBuffer, int iMapBufferSize){
	HRESULT hr = S_OK;
	D3D11_MAPPED_SUBRESOURCE msT;
	hr = pd3dimmediatecontext->Map( m_pColLookupForDetphTex, NULL, D3D11_MAP_WRITE_DISCARD,NULL,&msT);
	memcpy( msT.pData,pMapBuffer,iMapBufferSize*sizeof(ColorSpacePoint));
	pd3dimmediatecontext->Unmap(m_pColLookupForDetphTex,NULL);
	return hr;
}
string Kinect2Sensor::GenShaderCode(){
	string shaderCode="\
Texture2D<float4> colorTex;\n\
Texture2D<uint> depthTex;\n\
Texture2D<uint> mapTex;\n\
SamplerState samColor : register(s0);\n\
struct GS_INPUT{};\n\
struct PS_INPUT{\n\
	float4 Pos : SV_POSITION;\n\
	float2 Tex : TEXCOORD0;\n\
}\n\
GS_INPUT VS(){\n\
	GS_INPUT output = (GS_INPUT)0;\n\
	return output;\n\
}\n\
[maxvertexcount(4)]\n\
void GS( point GS_INPUT particles[1], inout TriangleStream<PS_INPUT> triStream ){\n\
	PS_INPUT output;\n\
	output.Pos = float4( -1.0f, 1.0f, 0.01f, 1.0f );\n\
	output.Tex = float2( 0.0f, 0.0f );\n\
	triStream.Append( output );\n\
	output.Pos = float4( -1.0f, -1.0f, 0.01f, 1.0f );\n\
	output.Tex = float2( 0.0f, 1.0f );\n\
	triStream.Append( output );\n\
	output.Pos = float4( 1.0f, 1.0f, 0.01f, 1.0f );\n\
	output.Tex = float2( 1.0f, 0.0f );\n\
	triStream.Append( output );\n\
	output.Pos = float4( 1.0f, -1.0f, 0.01f, 1.0f );\n\
	output.Tex = float2( 1.0f, 1.0f );\n\
	triStream.Append( output );\n\
}\n\
float4 PS(PS_INPUT input) : SV_Target{\n\
	float4 color = colorTex.Sample(samColor,input.Tex);\n\
	return color;\n\
}\n";
	return shaderCode;
}
void Kinect2Sensor::ProcessUndistortedRGBD(ID3D11DeviceContext* pd3dimmediatecontext){
	pd3dimmediatecontext->OMSetRenderTargets( 1, &m_pUndsitortedRGBDRTV, NULL );
	pd3dimmediatecontext->IASetInputLayout( m_pPassIL );
	pd3dimmediatecontext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
	UINT stride = 0;
	UINT offset = 0;
	pd3dimmediatecontext->IASetVertexBuffers( 0, 1, &m_pPassVB, &stride, &offset );
	pd3dimmediatecontext->VSSetShader( m_pPassVS, NULL, 0 );
	pd3dimmediatecontext->GSSetShader( m_pQuadGS, NULL, 0 );
	pd3dimmediatecontext->PSSetShader( m_pTexPS, NULL, 0 );

	m_ppInputSRV[0] = m_pColorSRV;
	m_ppInputSRV[1] = m_pColLookupForDepthSRV;
	m_ppInputSRV[2] = m_pDepthSRV;
	m_ppInputSRV[3] = m_pInfraredSRV;

	pd3dimmediatecontext->PSSetShaderResources( 0, 4, m_ppInputSRV );
	pd3dimmediatecontext->PSSetSamplers( 0, 1, &m_pNearestNeighborSS );
	pd3dimmediatecontext->RSSetViewports( 1, &m_RTviewport );

	pd3dimmediatecontext->Draw(1,0);
	pd3dimmediatecontext->PSSetShaderResources( 0, 4, m_ppNullSRV );
}