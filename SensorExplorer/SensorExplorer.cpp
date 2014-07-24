//--------------------------------------------------------------------------------------
// Sensor Explorer
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"

#include "TiledTextures.h"

#include "IRGBDStreamForDirectX.h"
//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
TiledTextures				multiTexture = TiledTextures();
IRGBDStreamForDirectX*		kinect = DirectXStreamFactory::createFromKinect2();
//--------------------------------------------------------------------------------------
// Initialization
//--------------------------------------------------------------------------------------
HRESULT Initial()
{
	HRESULT hr = S_OK;
	V_RETURN(multiTexture.Initial());
	V_RETURN(kinect->Initialize());
	multiTexture.AddTexture(kinect->getColor_ppSRV(), 1920, 1080);
	multiTexture.AddTexture(kinect->getDepth_ppSRV(), 512, 424, "int2 texCoord=input.Tex*float2(512,424);\n\
														   	uint depth=texture.Load(int3(texCoord,0));\n\
															uint codedDepth = depth%512;\n\
															return float4(0,codedDepth/512.f,0,0);","<uint>");
	multiTexture.AddTexture( kinect->getInfrared_ppSRV(), 512, 424, "int2 texCoord=input.Tex*float2(512,424);\n\
															uint infrared=texture.Load(int3(texCoord,0));\n\
															float norInfrared = pow(infrared/65535.f,0.32);\n\
															return float4(norInfrared,norInfrared,norInfrared,0);", "<uint>" );
	return hr;
}

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
									  DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext)
{
	return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext)
{
	multiTexture.ModifyDeviceSettings(pDeviceSettings);
	return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
									 void* pUserContext)
{
	HRESULT hr = S_OK;
	V_RETURN(kinect->CreateResource(pd3dDevice));
	
	V_RETURN(multiTexture.CreateResource(pd3dDevice));
	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
										 const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
	HRESULT hr = S_OK;
	multiTexture.Resize(pBackBufferSurfaceDesc);
	return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
								 double fTime, float fElapsedTime, void* pUserContext)
{
	kinect->UpdateTextures(pd3dImmediateContext);
	multiTexture.Render(pd3dImmediateContext);
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
	multiTexture.Release();
	kinect->Release();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
						 bool* pbNoFurtherProcessing, void* pUserContext)
{
	multiTexture.HandleMessages(hWnd, uMsg, wParam, lParam);
	return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
{
}


//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
					  bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta,
					  int xPos, int yPos, void* pUserContext)
{
}


//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved(void* pUserContext)
{
	return true;
}


//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// DXUT will create and use the best device (either D3D9 or D3D11) 
	// that is available on the system depending on which D3D callbacks are set below

	// Set general DXUT callbacks
	DXUTSetCallbackFrameMove(OnFrameMove);
	DXUTSetCallbackKeyboard(OnKeyboard);
	DXUTSetCallbackMouse(OnMouse);
	DXUTSetCallbackMsgProc(MsgProc);
	DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
	DXUTSetCallbackDeviceRemoved(OnDeviceRemoved);

	// Set the D3D11 DXUT callbacks. Remove these sets if the app doesn't need to support D3D11
	DXUTSetCallbackD3D11DeviceAcceptable(IsD3D11DeviceAcceptable);
	DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
	DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
	DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
	DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
	DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);

	// Perform any application-level initialization here

	DXUTInit(true, true, NULL); // Parse the command line, show msgboxes on error, no extra command line params
	DXUTSetCursorSettings(true, true); // Show the cursor and clip it when in full screen
	DXUTCreateWindow(L"SensorExplorer");

	Initial();

	DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1280, 800);
	DXUTMainLoop(); // Enter into the DXUT ren  der loop

	// Perform any application-level cleanup here

	return DXUTGetExitCode();
}


