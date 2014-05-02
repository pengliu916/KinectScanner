#pragma once

#include "NuiApi.h"
#include <D3D11.h>
#include "DXUT.h"

class Scanner
{
private:
	bool		m_bPowerON;

public:
	static const int					cBytesPerPixel = 4;
	static const NUI_IMAGE_RESOLUTION	cDepthResolution = NUI_IMAGE_RESOLUTION_640x480;
	static const NUI_IMAGE_RESOLUTION	cColorResolution = NUI_IMAGE_RESOLUTION_640x480;

	INuiSensor*							m_pNuiSensor;
	HANDLE								m_hNextDepthFrameEvent;
	HANDLE								m_pDepthStreamHandle;
	HANDLE								m_hNextColorFrameEvent;
	HANDLE								m_pColorStreamHandle;

	ID3D11Texture2D*					m_pDepthTex;
	ID3D11ShaderResourceView*			m_pDepthSRV;

	ID3D11Texture2D*					m_pColorTex;
	ID3D11ShaderResourceView*			m_pColorSRV;

	USHORT*								m_depthD16;
	BYTE*								m_colorRGBX;
	LONG*								m_colorCoordinates;

	LONG								m_depthWidth;
	LONG								m_depthHeight;
	LONG								m_colorWidth;
	LONG								m_colorHeight;

	LONG								m_colorToDepthDivisor;

	bool								m_bDepthReceived;
	bool								m_bColorReceived;

	bool								m_bNearMode;

	bool								m_bPaused;
	bool								m_bUpdated;


	Scanner( bool );
	~Scanner();
	HRESULT InitialKinect();
	HRESULT CreateResource( ID3D11Device* pd3dDevice );
	HRESULT ProcessDepth( ID3D11DeviceContext* pd3dimmediateContext );
	HRESULT ProcessColor( ID3D11DeviceContext* pd3dimmediateContext );
	HRESULT MapColorToDepth( ID3D11DeviceContext* pd3dimmediateContext );
	LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
	void Release();
	void UpdateDepthTexture( ID3D11DeviceContext* pd3dimmediateContext );
	HRESULT ToggleNearMode();
};

Scanner::Scanner( bool powerOn = true )
{
	m_bPowerON = powerOn;
	if( !m_bPowerON )
		return;
	DWORD width = 0;
	DWORD height = 0;

	m_pNuiSensor = NULL;
	NuiImageResolutionToSize( cDepthResolution, width, height );
	m_depthWidth = static_cast< LONG >( width );
	m_depthHeight = static_cast< LONG >( height );

	NuiImageResolutionToSize( cColorResolution, width, height );
	m_colorWidth = static_cast< LONG >( width );
	m_colorHeight = static_cast< LONG >( height );

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

	m_depthD16 = new USHORT[m_depthWidth*m_depthHeight];
	m_colorCoordinates = new LONG[m_depthWidth*m_depthHeight * 2];
	m_colorRGBX = new BYTE[m_colorWidth*m_colorHeight*cBytesPerPixel];

	m_bNearMode = false;

	m_bPaused = false;
}

Scanner::~Scanner()
{
	if( !m_bPowerON )
		return;

	if( NULL != m_pNuiSensor )
	{
		m_pNuiSensor->NuiShutdown();
		m_pNuiSensor->Release();
	}
	CloseHandle( m_hNextDepthFrameEvent );
	CloseHandle( m_hNextColorFrameEvent );

	// done with pixel data
	delete[] m_colorRGBX;
	delete[] m_colorCoordinates;
	delete[] m_depthD16;
}

void Scanner::Release()
{
	if( !m_bPowerON )
		return;

	SAFE_RELEASE( m_pDepthTex );
	SAFE_RELEASE( m_pDepthSRV );
	SAFE_RELEASE( m_pColorTex );
	SAFE_RELEASE( m_pColorSRV );
}

HRESULT Scanner::InitialKinect()
{
	HRESULT hr = S_OK;
	if( !m_bPowerON )
		return hr;

	INuiSensor* pNuiSensor = NULL;


	int iSensorCount = 0;
	hr = NuiGetSensorCount( &iSensorCount );
	if( FAILED( hr ) ) { return hr; }

	// Look at each Kinect sensor
	for( int i = 0; i < iSensorCount; ++i )
	{
		// Create the sensor so we can check status, if we can't create it, move on to the next
		hr = NuiCreateSensorByIndex( i, &pNuiSensor );
		if( FAILED( hr ) )
		{
			continue;
		}

		// Get the status of the sensor, and if connected, then we can initialize it
		hr = pNuiSensor->NuiStatus();
		if( S_OK == hr )
		{
			m_pNuiSensor = pNuiSensor;
			break;
		}

		// This sensor wasn't OK, so release it since we're not using it
		pNuiSensor->Release();
	}

	if( NULL == m_pNuiSensor )
	{
		return E_FAIL;
	}

	// Initialize the Kinect and specify that we'll be using depth
	hr = m_pNuiSensor->NuiInitialize( NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_DEPTH );
	if( FAILED( hr ) ) { return hr; }

	// Create an event that will be signaled when depth data is available
	m_hNextDepthFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	// Open a depth image stream to receive depth frames
	hr = m_pNuiSensor->NuiImageStreamOpen(
		NUI_IMAGE_TYPE_DEPTH,
		cDepthResolution,
		0,
		2,
		m_hNextDepthFrameEvent,
		&m_pDepthStreamHandle );
	if( FAILED( hr ) ) { return hr; }

	// Create an event that will be signaled when color data is available
	m_hNextColorFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	// Open a color image stream to receive color frames
	hr = m_pNuiSensor->NuiImageStreamOpen(
		NUI_IMAGE_TYPE_COLOR,
		cColorResolution,
		0,
		2,
		m_hNextColorFrameEvent,
		&m_pColorStreamHandle );
	if( FAILED( hr ) ) { return hr; }

	// Start with near mode on
	//ToggleNearMode();
	return hr;
}


HRESULT Scanner::CreateResource( ID3D11Device* pd3dDevice )
{
	HRESULT hr = S_OK;
	if( !m_bPowerON )
		return hr;


	// Create depth texture and its resource view
	D3D11_TEXTURE2D_DESC depthTexDesc = { 0 };
	depthTexDesc.Width = m_depthWidth;
	depthTexDesc.Height = m_depthHeight;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.Format = DXGI_FORMAT_R16_SINT;
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
	V_RETURN( pd3dDevice->CreateTexture2D( &colorTexDesc, NULL, &m_pColorTex ) );
	V_RETURN( pd3dDevice->CreateShaderResourceView( m_pColorTex, NULL, &m_pColorSRV ) );

	return hr;
};

HRESULT Scanner::ToggleNearMode()
{
	HRESULT hr = S_OK;
	if( !m_bPowerON )
		return hr;

	hr = E_FAIL;

	if( m_pNuiSensor )
	{
		hr = m_pNuiSensor->NuiImageStreamSetImageFrameFlags( m_pDepthStreamHandle, m_bNearMode ? 0 : NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE );

		if( SUCCEEDED( hr ) )
		{
			m_bNearMode = !m_bNearMode;
		}
	}

	return hr;
}

HRESULT Scanner::ProcessDepth( ID3D11DeviceContext* pd3dimmediateContext )
{
	HRESULT hr = S_OK;
	if( !m_bPowerON )
		return hr;

	NUI_IMAGE_FRAME	imageFrame;
	hr = m_pNuiSensor->NuiImageStreamGetNextFrame( m_pDepthStreamHandle, 0, &imageFrame );
	if( FAILED( hr ) )	 { return hr; }

	NUI_LOCKED_RECT LockedRect;
	hr = imageFrame.pFrameTexture->LockRect( 0, &LockedRect, NULL, 0 );
	if( FAILED( hr ) )	 { return hr; }

	memcpy( m_depthD16, LockedRect.pBits, LockedRect.size );

	m_bDepthReceived = true;

	hr = imageFrame.pFrameTexture->UnlockRect( 0 );
	if( FAILED( hr ) )	 { return hr; }

	hr = m_pNuiSensor->NuiImageStreamReleaseFrame( m_pDepthStreamHandle, &imageFrame );

	D3D11_MAPPED_SUBRESOURCE msT;
	hr = pd3dimmediateContext->Map( m_pDepthTex, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &msT );
	if( FAILED( hr ) )	 { return hr; }
	memcpy( msT.pData, m_depthD16, LockedRect.size );
	pd3dimmediateContext->Unmap( m_pDepthTex, NULL );

	return hr;
}

HRESULT Scanner::ProcessColor( ID3D11DeviceContext* pd3dimmediateContext )
{
	HRESULT hr = S_OK;
	if( !m_bPowerON )
		return hr;

	NUI_IMAGE_FRAME imageFrame;

	hr = m_pNuiSensor->NuiImageStreamGetNextFrame( m_pColorStreamHandle, 0, &imageFrame );
	if( FAILED( hr ) ) { return hr; }

	NUI_LOCKED_RECT LockedRect;
	hr = imageFrame.pFrameTexture->LockRect( 0, &LockedRect, NULL, 0 );
	if( FAILED( hr ) ) { return hr; }

	memcpy( m_colorRGBX, LockedRect.pBits, LockedRect.size );
	m_bColorReceived = true;

	hr = imageFrame.pFrameTexture->UnlockRect( 0 );
	if( FAILED( hr ) ) { return hr; };

	hr = m_pNuiSensor->NuiImageStreamReleaseFrame( m_pColorStreamHandle, &imageFrame );

	/*D3D11_MAPPED_SUBRESOURCE msT;
	hr = pd3dimmediateContext->Map(m_pColorTex,NULL,D3D11_MAP_WRITE_DISCARD,NULL,&msT);
	if(FAILED(hr))	 {return hr;}
	memcpy(msT.pData,m_colorRGBX,LockedRect.size);
	pd3dimmediateContext->Unmap(m_pColorTex,NULL);*/

	return hr;
}

HRESULT Scanner::MapColorToDepth( ID3D11DeviceContext* pd3dimmediateContext )
{
	HRESULT hr = S_OK;
	if( !m_bPowerON )
		return hr;


	// Get of x, y coordinates for color in depth space
	// This will allow us to later compensate for the differences in location, angle, etc between the depth and color cameras
	m_pNuiSensor->NuiImageGetColorPixelCoordinateFrameFromDepthPixelFrameAtResolution(
		cColorResolution,
		cDepthResolution,
		m_depthWidth*m_depthHeight,
		m_depthD16,
		m_depthWidth*m_depthHeight * 2,
		m_colorCoordinates
		);

	// copy to our d3d 11 color texture
	D3D11_MAPPED_SUBRESOURCE msT;
	hr = pd3dimmediateContext->Map( m_pColorTex, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &msT );
	if( FAILED( hr ) ) { return hr; }

	// loop over each row and column of the color
	for( LONG y = 0; y < m_colorHeight; ++y )
	{
		LONG* pDest = ( LONG* )( ( BYTE* )msT.pData + msT.RowPitch * y );
		for( LONG x = 0; x < m_colorWidth; ++x )
		{
			// calculate index into depth array
			int depthIndex = x / m_colorToDepthDivisor + y / m_colorToDepthDivisor * m_depthWidth;

			// retrieve the depth to color mapping for the current depth pixel
			LONG colorInDepthX = m_colorCoordinates[depthIndex * 2];
			LONG colorInDepthY = m_colorCoordinates[depthIndex * 2 + 1];

			// make sure the depth pixel maps to a valid point in color space
			if( colorInDepthX >= 0 && colorInDepthX < m_colorWidth && colorInDepthY >= 0 && colorInDepthY < m_colorHeight )
			{
				// calculate index into color array
				LONG colorIndex = colorInDepthX + colorInDepthY * m_colorWidth;

				// set source for copy to the color pixel
				LONG* pSrc = ( LONG * )m_colorRGBX + colorIndex;
				*pDest = *pSrc;
			} else
			{
				*pDest = 0;
			}

			pDest++;
		}
	}

	pd3dimmediateContext->Unmap( m_pColorTex, NULL );

	return hr;
}

void Scanner::UpdateDepthTexture( ID3D11DeviceContext* pd3dimmediateContext )
{
	m_bUpdated = false;
	if( !m_bPowerON )
		return;

	if( m_bPaused )
		return;
	bool needToMapColorToDepth = false;

	if( WAIT_OBJECT_0 == WaitForSingleObject( m_hNextDepthFrameEvent, 0 ) )
	{
		// if we have received any valid new depth data we may need to draw
		if( SUCCEEDED( ProcessDepth( pd3dimmediateContext ) ) )
		{
			needToMapColorToDepth = true;
			m_bUpdated = true;
		}
	}

	if( WAIT_OBJECT_0 == WaitForSingleObject( m_hNextColorFrameEvent, 0 ) )
	{
		// if we have received any valid new color data we may need to draw
		if( SUCCEEDED( ProcessColor( pd3dimmediateContext ) ) )
		{
			needToMapColorToDepth = true;
		}
	}

	// If we have not yet received any data for either color or depth since we started up, we shouldn't draw
	if( !m_bDepthReceived || !m_bColorReceived )
	{
		needToMapColorToDepth = false;
	}

	if( needToMapColorToDepth )
	{
		MapColorToDepth( pd3dimmediateContext );
	}
}

LRESULT Scanner::HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if( !m_bPowerON )
		return 0;

	UNREFERENCED_PARAMETER( lParam );
	UNREFERENCED_PARAMETER( hWnd );

	switch( uMsg )
	{
	case WM_KEYDOWN:
	{
		int nKey = static_cast< int >( wParam );

		if( nKey == 'N' )
		{
			this->ToggleNearMode();
		} else if( nKey == 'P' )
		{
			this->m_bPaused = !this->m_bPaused;
		}
		break;
	}
	}

	return 0;
}