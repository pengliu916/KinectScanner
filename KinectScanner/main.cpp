//--------------------------------------------------------------------------------------
// KinectScanner
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"

#include "header.h"
#include "TiledTextures.h"
#include "FilteredPCL.h"
#include "VolumeTSDF.h"
#include "TSDFImages.h"
#include "PoseEstimator.h"
#include "HistoPyramidMC.h"

using namespace std::placeholders;

// For generating debug info
ID3D11Debug *d3dDebug = nullptr;

CDXUTDialogResourceManager      g_DialogResourceManager;
CDXUTTextHelper*                g_pTxtHelper = NULL;
wchar_t                         g_debugLine1[100];

TiledTextures					multiTexture = TiledTextures();
FilteredPCL                     pointCloud = FilteredPCL(D_W,D_H);
VolumeTSDF                      meshVolume = VolumeTSDF(VOXEL_SIZE, VOXEL_NUM_X, VOXEL_NUM_Y, VOXEL_NUM_Z);
TSDFImages                      tsdfImgs = TSDFImages(&meshVolume);

PoseEstimator                   poseEstimator = PoseEstimator(D_W,D_H);

bool                            g_bFirstFrame = true;


D3D11_VIEWPORT                  m_RTviewport;

//--------------------------------------------------------------------------------------
//Global Variables only for test purpose..... 
//--------------------------------------------------------------------------------------

float norm ( XMVECTOR vector )
{
    XMVECTOR length = XMVector3Length ( vector );
    float distance = 0.0f;
    XMStoreFloat ( &distance, length );
    return distance;
};
//--------------------------------------------------------------------------------------
//Initialization
//--------------------------------------------------------------------------------------
HRESULT Initial()
{
    HRESULT hr = S_OK;
    V_RETURN( pointCloud.Initial() )
    V_RETURN( multiTexture.Initial() );

	// Use TSDFImages's Generated RGBD
	V_RETURN(poseEstimator.Initial(tsdfImgs.m_pGeneratedTPC, &pointCloud.m_TransformedPC));
	// Use HistoPyramidMC's Generated RGBD
	//V_RETURN(poseEstimator.Initial(histoPyraimdMC.m_pGeneratedTPC, &pointCloud.m_TransformedPC));

    multiTexture.AddTexture(poseEstimator.m_pKinectTPC->ppMeshNormalTexSRV,D_W,D_H);
	multiTexture.AddTexture(poseEstimator.m_pTsdfTPC->ppMeshNormalTexSRV, D_W, D_H);

    swprintf(g_debugLine1,100,L"Debug Line 1...");

    return hr;
}

//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetForegroundColor( XMFLOAT4( 1.0f, 0.0f, 0.0f, 1.0f ) );
    //g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->SetInsertionPos( 2, 10 );
    g_pTxtHelper->DrawTextLine(g_debugLine1);
    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    multiTexture.ModifyDeviceSettings( pDeviceSettings );
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr = S_OK;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 2 );

    V_RETURN( pointCloud.CreateResource( pd3dDevice ));
    V_RETURN( meshVolume.CreateResource(pd3dDevice,&pointCloud.m_TransformedPC));
    V_RETURN( tsdfImgs.CreateResource(pd3dDevice));

	V_RETURN(poseEstimator.CreateResource(pd3dDevice));
    V_RETURN( multiTexture.CreateResource( pd3dDevice) );

	// Setup the debug layer
	
	if (SUCCEEDED(pd3dDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug)))
	{
		ID3D11InfoQueue *d3dInfoQueue = nullptr;
		if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue)))
		{
#ifdef _DEBUG
			d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
			d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
#endif

			D3D11_MESSAGE_ID hide[] =
			{
				D3D11_MESSAGE_ID_DEVICE_DRAW_VERTEX_BUFFER_TOO_SMALL,
				// Add more message IDs here as needed
			};

			D3D11_INFO_QUEUE_FILTER filter;
			memset(&filter, 0, sizeof(filter));
			filter.DenyList.NumIDs = _countof(hide);
			filter.DenyList.pIDList = hide;
			d3dInfoQueue->AddStorageFilterEntries(&filter);
			d3dInfoQueue->Release();
		}
		d3dDebug->Release();
	}
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr=S_OK;
    ID3D11DeviceContext* pd3dimmediateContext;
    pd3dDevice->GetImmediateContext(&pd3dimmediateContext);

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

	multiTexture.Resize(pd3dDevice, pBackBufferSurfaceDesc);
    poseEstimator.Resize(pd3dimmediateContext);
    SAFE_RELEASE(pd3dimmediateContext);

	int winWidth = pBackBufferSurfaceDesc->Width;
	int winHeight = pBackBufferSurfaceDesc->Height;

	m_RTviewport.Width = (float)winWidth;
	m_RTviewport.Height = (float)winHeight;
	m_RTviewport.MinDepth = 0.0f;
	m_RTviewport.MaxDepth = 1.0f;
	m_RTviewport.TopLeftX = 0;
	m_RTviewport.TopLeftY = 0;
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
	// whether to get the next frame or not

	// Update the virtual cam of the model viewer of HistoPyramidMC
	//histoPyraimdMC.Update(fElapsedTime);
	tsdfImgs.Update(fElapsedTime);
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext )
{
	if(g_bFirstFrame){
		// Get new depth and color frmae from RGBD sensor
		g_bFirstFrame = false;
		pointCloud.Render(pd3dImmediateContext);
		tsdfImgs.Get3ImgForKinect ( pd3dImmediateContext );
	}
	for(int i=0;i<15;i++) poseEstimator.Processing ( pd3dImmediateContext );

	swprintf(g_debugLine1, 100, L"% 6.5f % 6.5f % 6.5f % 6.5f % 6.5f % 6.5f % 6.5f",
			 poseEstimator.m_Reduction.m_structSum.f4Data0.x, 
			 poseEstimator.m_Reduction.m_structSum.f4Data1.x, 
			 poseEstimator.m_Reduction.m_structSum.f4Data2.x, 
			 poseEstimator.m_Reduction.m_structSum.f4Data3.x, 
			 poseEstimator.m_Reduction.m_structSum.f4Data4.x, 
			 poseEstimator.m_Reduction.m_structSum.f4Data5.x,
			 poseEstimator.m_Reduction.m_structSum.f4Data6.x);
	//swprintf(g_debugLine1, 100, L"%-8s x:% 6.5f y:% 6.5f z:% 6.5f", L"tran:", poseEstimator.m_fXoffset, poseEstimator.m_fYoffset, poseEstimator.m_fZoffset);
	//swprintf(g_debugLine2, 100, L"%-8s a:% 6.5f b:% 6.5f g:% 6.5f", L"rotate:", poseEstimator.m_fAlpha, poseEstimator.m_fBeta, poseEstimator.m_fGamma);

    //swprintf(g_debugLine3,100,L"Track success");

	ID3D11RenderTargetView* rtv= DXUTGetD3D11RenderTargetView();
	float ClearColor[4] = { 0.f, 0.f, 0.f, 0.0f };
	pd3dImmediateContext->ClearRenderTargetView(rtv, ClearColor);

    multiTexture.Render( pd3dImmediateContext );

	// Render the text
	pd3dImmediateContext->OMSetRenderTargets(1, &rtv, NULL); 
	pd3dImmediateContext->RSSetViewports(1, &m_RTviewport);

    RenderText();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
	tsdfImgs.Release();
	//histoPyraimdMC.Release();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    multiTexture.Release();
    pointCloud.Release();
    tsdfImgs.Destory();
    meshVolume.Release();
    poseEstimator.Release();

	//histoPyraimdMC.Destory();

    
    g_DialogResourceManager.OnD3D11DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

	if (d3dDebug != nullptr)
	{
		d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
		d3dDebug = nullptr;
	}

}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
	tsdfImgs.HandleMessages(hWnd, uMsg, wParam, lParam);
    multiTexture.HandleMessages( hWnd, uMsg, wParam, lParam );
    pointCloud.HandleMessages( hWnd, uMsg, wParam, lParam );
    poseEstimator.HandleMessages( hWnd, uMsg, wParam, lParam );
	meshVolume.HandleMessages(hWnd, uMsg, wParam, lParam);
    
    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
                       bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta,
                       int xPos, int yPos, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved( void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackMouse( OnMouse );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackDeviceRemoved( OnDeviceRemoved );

    // Set the D3D11 DXUT callbacks. Remove these sets if the app doesn't need to support D3D11
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    // Perform any application-level initialization here

    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"KinectScanner" );

    Initial();

    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1000, 600 );
    DXUTMainLoop(); // Enter into the DXUT ren  der loop

    // Perform any application-level cleanup here

    return DXUTGetExitCode();
}


