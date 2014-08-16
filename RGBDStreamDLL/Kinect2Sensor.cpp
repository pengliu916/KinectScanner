#pragma once

#include "Kinect2Sensor.h"

// Safe release for interfaces
template<class Interface>
inline void SafeRelease(Interface *& pInterfaceToRelease)
{
	if (pInterfaceToRelease != NULL)
	{
		pInterfaceToRelease->Release();
		pInterfaceToRelease = NULL;
	}
}

Kinect2Sensor::Kinect2Sensor() : m_uCompileFlag(D3DCOMPILE_ENABLE_STRICTNESS){
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
	GenShaderCode();
	ID3DBlob* pVSBlob = NULL;
	V_RETURN(CompileFormString(m_strShaderCode, nullptr, "VS", "vs_5_0", m_uCompileFlag, 0, &pVSBlob));
	V_RETURN(pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pPassVS));
	DXUT_SetDebugName(m_pPassVS, "m_pPassVS");

	ID3DBlob* pGSBlob = NULL;
	V_RETURN(CompileFormString(m_strShaderCode, nullptr, "GS", "gs_5_0", m_uCompileFlag, 0, &pGSBlob));
	V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pQuadGS));
	DXUT_SetDebugName(m_pQuadGS, "m_pQuadGS");
	pGSBlob->Release();

	pGSBlob = NULL;
	V_RETURN(CompileFormString(m_strShaderCode, nullptr, "AddDepthGS", "gs_5_0", m_uCompileFlag, 0, &pGSBlob));
	V_RETURN(pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pAddDepthGS));
	DXUT_SetDebugName(m_pAddDepthGS, "m_pAddDepthGS");
	pGSBlob->Release();

	ID3DBlob* pPSBlob = NULL;
	V_RETURN(CompileFormString(m_strShaderCode, nullptr, "PS", "ps_5_0", m_uCompileFlag, 0, &pPSBlob));
	V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pFinalizedRGBDPS));
	DXUT_SetDebugName(m_pFinalizedRGBDPS, "m_pFinalizedRGBDPS");
	pPSBlob->Release();

	pPSBlob = NULL;
	V_RETURN(CompileFormString(m_strShaderCode, nullptr, "AddDepthPS", "ps_5_0", m_uCompileFlag, 0, &pPSBlob));
	V_RETURN(pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pAddDepthPS));
	DXUT_SetDebugName(m_pAddDepthPS, "m_pAddDepthPS");
	pPSBlob->Release();

	D3D11_INPUT_ELEMENT_DESC layout[] = { { "POSITION", 0, DXGI_FORMAT_R16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
	V_RETURN(pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pPassIL));
	DXUT_SetDebugName(m_pPassIL, "m_pPassIL");
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

	//Create rendertaget resource
	D3D11_TEXTURE2D_DESC	RTtextureDesc = { 0 };
	RTtextureDesc.Width = m_cDepthWidth;
	RTtextureDesc.Height = m_cDepthHeight;
	RTtextureDesc.MipLevels = 1;
	RTtextureDesc.ArraySize = 1;
	RTtextureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	RTtextureDesc.SampleDesc.Count = 1;
	RTtextureDesc.Usage = D3D11_USAGE_DEFAULT;
	RTtextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	RTtextureDesc.CPUAccessFlags = 0;
	RTtextureDesc.MiscFlags = 0;

	V_RETURN(pd3dDevice->CreateTexture2D(&RTtextureDesc, NULL, &m_pFinalizedRGBDTex));
	DXUT_SetDebugName(m_pFinalizedRGBDTex, "m_pFinalizedRGBDTex");
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pFinalizedRGBDTex, NULL, &m_pFinalizedRGBDSRV));
	DXUT_SetDebugName(m_pFinalizedRGBDSRV, "m_pFinalizedRGBDSRV");
	V_RETURN(pd3dDevice->CreateRenderTargetView(m_pFinalizedRGBDTex, NULL, &m_pFinalizedRGBDRTV));
	DXUT_SetDebugName(m_pFinalizedRGBDRTV, "m_pFinalizedRGBDRTV");

	// Create rendertarget resource for addDepth
	RTtextureDesc.Width = m_cColorWidth;
	RTtextureDesc.Height = m_cColorHeight;
	V_RETURN(pd3dDevice->CreateTexture2D(&RTtextureDesc, NULL, &m_pAddDepthTex));
	DXUT_SetDebugName(m_pAddDepthTex, "m_pAddDepthTex");
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pAddDepthTex, NULL, &m_pAddDepthSRV));
	DXUT_SetDebugName(m_pAddDepthSRV, "m_pAddDepthSRV");
	V_RETURN(pd3dDevice->CreateRenderTargetView(m_pAddDepthTex, NULL, &m_pAddDepthRTV));
	DXUT_SetDebugName(m_pAddDepthRTV, "m_pAddDepthRTV");
	//Create DepthStencil buffer and view
	D3D11_TEXTURE2D_DESC descDepth = { 0 };
	descDepth.Width = m_cDepthWidth / 2;
	descDepth.Height = m_cDepthHeight / 2;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D16_UNORM;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateTexture2D(&descDepth, NULL, &m_pAddDepthDSTex));
	DXUT_SetDebugName(m_pAddDepthDSTex, "m_pAddDepthDSTex");
	V_RETURN(pd3dDevice->CreateDepthStencilView(m_pAddDepthDSTex, NULL, &m_pAddDepthDSSRV));  // [out] Depth stencil view
	DXUT_SetDebugName(m_pAddDepthDSSRV, "m_pAddDepthDSSRV");


	m_ppInputSRV = new ID3D11ShaderResourceView*[3];
	m_ppNullSRV = new ID3D11ShaderResourceView*[3];

	for( UINT i = 0; i < 3; i++ )
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

	SAFE_RELEASE(m_pFinalizedRGBDTex);
	SAFE_RELEASE(m_pFinalizedRGBDSRV);
	SAFE_RELEASE(m_pFinalizedRGBDRTV);
	SAFE_RELEASE( m_pPassVS );
	SAFE_RELEASE( m_pQuadGS );
	SAFE_RELEASE(m_pFinalizedRGBDPS);
	SAFE_RELEASE( m_pPassIL );
	SAFE_RELEASE( m_pPassVB );
	SAFE_RELEASE( m_pNearestNeighborSS );

	SAFE_RELEASE(m_pAddDepthTex);
	SAFE_RELEASE(m_pAddDepthSRV);
	SAFE_RELEASE(m_pAddDepthRTV);
	SAFE_RELEASE(m_pAddDepthDSTex);
	SAFE_RELEASE(m_pAddDepthDSSRV);
	SAFE_RELEASE(m_pAddDepthGS);
	SAFE_RELEASE(m_pAddDepthPS);

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
HRESULT Kinect2Sensor::ProcessDepthColorMap(ID3D11DeviceContext* pd3dimmediatecontext, const ColorSpacePoint* pMapBuffer, int iMapBufferSize){
	HRESULT hr = S_OK;
	D3D11_MAPPED_SUBRESOURCE msT;
	hr = pd3dimmediatecontext->Map( m_pColLookupForDetphTex, NULL, D3D11_MAP_WRITE_DISCARD,NULL,&msT);
	memcpy( msT.pData,pMapBuffer,iMapBufferSize*sizeof(ColorSpacePoint));
	pd3dimmediatecontext->Unmap(m_pColLookupForDetphTex,NULL);
	return hr;
}

bool Kinect2Sensor::UpdateTextures( ID3D11DeviceContext* pd3dimmediateContext, bool defaultReg = true, 
								   bool color = true, bool depth = true, bool infrared = false ){
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
void Kinect2Sensor::GenShaderCode(){
	stringstream shaderCode;
	shaderCode << "Texture2D<float4> txColor : register( t0 );\n";
	shaderCode << "Texture2D<uint> txDepth : register( t1 );\n";
	shaderCode << "Texture2D<float2> txMap : register( t2 );\n";
	shaderCode << "SamplerState samColor : register( s0 );\n";
	shaderCode << "static const float4 k =" << KINECT2_DEPTH_K << ";\n";
	shaderCode << "static const float2 p =" << KINECT2_DEPTH_P << ";\n";
	shaderCode << "static const float2 f =" << KINECT2_DEPTH_F << ";\n";
	shaderCode << "static const float2 c =" << KINECT2_DEPTH_C << ";\n";
	shaderCode << "static const float2 reso =float2(" << m_cDepthWidth << "," << m_cDepthHeight << ");\n";
	shaderCode << "static const matrix e =" << KINECT2_DEPTH2COL_M << ";\n";
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
	shaderCode << "	output.Tex = float2(reso.x,0);   \n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "	output.Pos = float4(-1.0f, -1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = float2(0, reso.y);   \n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "	output.Pos = float4(1.0f, -1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = reso;\n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "}\n";

	shaderCode << "[maxvertexcount(1)]\n";
	shaderCode << "void AddDepthGS(point GS_INPUT particles[1], uint primID : SV_PrimitiveID, inout PointStream<PS_INPUT> triStream){   \n";
	shaderCode << "	PS_INPUT output;\n";
	shaderCode << "	output.Pos = float4(-1.0f, 1.0f, 0.01f, 1.0f);   \n";
	shaderCode << "	output.Tex = float2(0,0);\n";
	shaderCode << "	triStream.Append(output);\n";
	shaderCode << "}\n";

	shaderCode << "float4 PS(PS_INPUT input) : SV_Target{ \n";
	shaderCode << "	float2 idx = (input.Tex - c)/f;   \n";
	shaderCode << "   \n";
	shaderCode << "	float r2 = dot(idx, idx);\n";
	shaderCode << "	float r4 = r2 * r2;\n";
	shaderCode << "	float r6 = r2 * r4;\n";
	shaderCode << "   \n";
	shaderCode << "	float2 nidx = idx*(1.f + k.x*r2 + k.y*r4 +k.z*r6) + 2.f*p*idx.x*idx.y + p.yx*(r2 + 2.f*idx*idx); \n";
	shaderCode << "	nidx = nidx * f + c;   \n";
	shaderCode << " float2 mappedIdx = txMap.Load(int3(nidx,0));\n";
	shaderCode << " //float2 mappedIdx = txMap.Sample(samColor,nidx / float2(512,424))/float2(1920,1080);\n";
	shaderCode << "	float4 col = txColor.Load(int3(mappedIdx,0));\n";
	shaderCode << "	//float4 col = txColor.Sample(samColor, mappedIdx);\n";
	shaderCode << " col.w = txDepth.Load( int3(nidx,0) ) / 1000.f;\n";
	shaderCode << " return col;\n";
	shaderCode << "}\n";
	shaderCode << "// PS for add depth to color texture to avoid color contamination\n";
	shaderCode << "float4 AddDepthPS(PS_INPUT input) : SV_Target{ \n";
	shaderCode << "	float2 idx = (input.Tex - c)/f;   \n";
	shaderCode << "   \n";
	shaderCode << "	float r2 = dot(idx, idx);\n";
	shaderCode << "	float r4 = r2 * r2;\n";
	shaderCode << "	float r6 = r2 * r4;\n";
	shaderCode << "   \n";
	shaderCode << "	float2 nidx = idx*(1.f + k.x*r2 + k.y*r4 +k.z*r6) + 2.f*p*idx.x*idx.y + p.yx*(r2 + 2.f*idx*idx); \n";
	shaderCode << "	//float4 pos = \n";
	shaderCode << "	nidx = nidx * f + c;   \n";
	shaderCode << " float2 mappedIdx = txMap.Load(int3(nidx,0));\n";
	shaderCode << " //float2 mappedIdx = txMap.Sample(samColor,nidx / float2(512,424))/float2(1920,1080);\n";
	shaderCode << "	float4 col = txColor.Load(int3(mappedIdx,0));\n";
	shaderCode << "	//float4 col = txColor.Sample(samColor, mappedIdx);\n";
	shaderCode << " col.w = txDepth.Load( int3(nidx,0) ) / 1000.f;\n";
	shaderCode << " return col;\n";
	shaderCode << "}\n";
	m_strShaderCode = shaderCode.str();
}
HRESULT Kinect2Sensor::CompileFormString(string code,
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

void Kinect2Sensor::ProcessFinalizedRGBD(ID3D11DeviceContext* pd3dimmediatecontext){
	pd3dimmediatecontext->OMSetRenderTargets(1, &m_pFinalizedRGBDRTV, NULL);
	pd3dimmediatecontext->IASetInputLayout( m_pPassIL );
	pd3dimmediatecontext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
	UINT stride = 0;
	UINT offset = 0;
	pd3dimmediatecontext->IASetVertexBuffers( 0, 1, &m_pPassVB, &stride, &offset );
	pd3dimmediatecontext->VSSetShader( m_pPassVS, NULL, 0 );
	pd3dimmediatecontext->GSSetShader( m_pQuadGS, NULL, 0 );
	pd3dimmediatecontext->PSSetShader(m_pFinalizedRGBDPS, NULL, 0);

	m_ppInputSRV[0] = m_pColorSRV;
	m_ppInputSRV[1] = m_pDepthSRV;
	m_ppInputSRV[2] = m_pColLookupForDepthSRV;

	pd3dimmediatecontext->PSSetShaderResources( 0, 3, m_ppInputSRV );
	pd3dimmediatecontext->PSSetSamplers( 0, 1, &m_pNearestNeighborSS );
	pd3dimmediatecontext->RSSetViewports( 1, &m_RTviewport );

	pd3dimmediatecontext->Draw(1,0);
	pd3dimmediatecontext->PSSetShaderResources( 0, 3, m_ppNullSRV );
}
