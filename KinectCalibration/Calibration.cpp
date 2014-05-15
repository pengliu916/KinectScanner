//--------------------------------------------------------------------------------------
// KinectScanner
//--------------------------------------------------------------------------------------
#include <thread>
#include <functional>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "DXUT.h"
#include "DXUTgui.h"

#include "TiledTextures.h"
#include "KinectSensor.h"
#include "OpenCVThread.h"
#include "ScopedThread.h"

TiledTextures               multiTexture = TiledTextures(true, SUB_TEXTUREWIDTH, SUB_TEXTUREHEIGHT );
KinectSensor                kinect = KinectSensor(true);
OpenCVThread                openCV(&kinect);

//--------------------------------------------------------------------------------------
//Initialization
//--------------------------------------------------------------------------------------
HRESULT Initial()
{
    HRESULT hr = S_OK;
    V_RETURN( kinect.InitialKinect());
    V_RETURN( multiTexture.Initial());
    return hr;
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
    V_RETURN( kinect.CreateResource(pd3dDevice));
    //int max=51;
    //for(int i=1;i<=max;i++){
    //    /*ostringstream test;
    //    test<<"return float4(1,1,1,1)*"<<(float)i/max<<";";
    //    multiTexture.AddTexture(NULL,test.str());*/
    //    multiTexture.AddTexture(&kinect.m_pColorSRV,"color = texture.Sample(samColor,input.Tex);return color;");
    //}
    multiTexture.AddTexture(&kinect.m_pColorSRV,"color = texture.Sample(samColor,input.Tex);return color;");
    multiTexture.AddTexture(&kinect.m_pDepthSRV,"int depth = texture.Load(int3(input.Tex*float2(640,480),0));\n\
                                                float fDepth = depth/8000.0f;\n\
                                                if(fDepth>4.0f || fDepth <0.3) return float4(0,0,0,0);\n\
                                                fDepth = 1.0f - fDepth / 4.0f;\n\
                                                color = float4(fDepth,fDepth,fDepth,1);return color;","<int>");

    V_RETURN( multiTexture.CreateResource( pd3dDevice));
    

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr=S_OK;
    multiTexture.Resize(pBackBufferSurfaceDesc);

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext )
{
    //kinect.ProcessColor(pd3dImmediateContext);
    //kinect.ProcessDepth(pd3dImmediateContext);
    //kinect.ProcessInfrared(pd3dImmediateContext);
    kinect.UpdateDepthTexture(pd3dImmediateContext);
    multiTexture.Render( pd3dImmediateContext );
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    cv::destroyAllWindows();
    multiTexture.Release();
    kinect.Release();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
    multiTexture.HandleMessages( hWnd, uMsg, wParam, lParam );
    kinect.HandleMessages( hWnd, uMsg, wParam, lParam );
    
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
    DXUTCreateWindow( L"ScannerCalibration" );

    Initial();
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 640, 480 );

    ScopedThread t(std::thread(std::bind(&OpenCVThread::Run,&openCV)));
    DXUTMainLoop(); // Enter into the DXUT ren  der loop

    // Perform any application-level cleanup here
    openCV.m_bTerminated = true;

    return DXUTGetExitCode();
}


