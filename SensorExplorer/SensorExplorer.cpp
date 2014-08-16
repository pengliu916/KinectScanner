//--------------------------------------------------------------------------------------
// Sensor Explorer
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"

#include "TiledTextures.h"

#include "IRGBDStreamForDirectX.h"
#include "RGBDStreamDLL\Kinect2Sensor.h"


#include "PointCloudVisualizer.h"
#include "Undistortion.h"
#include "DepthColorRegistration.h"

using namespace std::placeholders;
//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
TiledTextures				multiTexture = TiledTextures();
IRGBDStreamForDirectX*		sensor = DirectXStreamFactory::createFromKinect2();
Kinect2Sensor*				kinect = dynamic_cast<Kinect2Sensor*>(sensor);

PointCloudVisualizer		PCVisual = PointCloudVisualizer();
Undistortion				UndistortDepth = Undistortion();
Undistortion				UndistortColor = Undistortion();
DepthColorRegistration		Register = DepthColorRegistration();
//--------------------------------------------------------------------------------------
// Initialization
//--------------------------------------------------------------------------------------
HRESULT Initial()
{
	HRESULT hr = S_OK;
	V_RETURN(multiTexture.Initial());
	V_RETURN(sensor->Initialize());
	string strPScode;
	// Original color img
	/*multiTexture.AddTexture(sensor->getColor_ppSRV(), 1920, 1080);*/

	// Undistort color img
	/*multiTexture.AddTexture(&UndistortColor.m_pOutSRV, 1920, 1080);*/

	// Original depth img
	/*strPScode = "\
		int2 texCoord=input.Tex*float2(512,424);\n\
		uint depth=texture.Load(int3(texCoord,0));\n\
		uint codedDepth = depth%512;\n\
		return float4(0,codedDepth/512.f,0,0);";
	multiTexture.AddTexture(sensor->getDepth_ppSRV(), 512, 424, strPScode,"<uint>");*/

	// Original infrared img
	/*strPScode = "\
		int2 texCoord=input.Tex*float2(512,424);\n\
		uint infrared=texture.Load(int3(texCoord,0));\n\
		float norInfrared = pow(infrared/65535.f,0.32);\n\
		return float4(norInfrared,norInfrared,norInfrared,0);";
	multiTexture.AddTexture( sensor->getInfrared_ppSRV(), 512, 424, strPScode, "<uint>" );*/
	
	// Undistort infrared img
	/*strPScode = "\
		int2 texCoord=input.Tex*float2(512,424);\n\
		uint infrared=texture.Load(int3(texCoord,0));\n\
		float norInfrared = pow(infrared/65535.f,0.32);\n\
		return float4(norInfrared,norInfrared,norInfrared,0);";
	multiTexture.AddTexture(&UndistortDepth.m_pOutSRV, 512, 424, strPScode, "<uint>");*/
	
	// Temp factory depth rgb look up table
	//strPScode = "\
	//	float2 rawData = texture.Sample(samColor,input.Tex)/float2(1920,1080);\n\
	//	return textures_0.SampleLevel(samColor,rawData,0);\n\
	//	//return float4(rawData,0,0);";
	//multiTexture.AddTexture( kinect->getColorLookup_ppSRV(), 512, 424,strPScode,"<float2>");

	// Undistorted RGBD with factory registration
	multiTexture.AddTexture(&kinect->m_pFinalizedRGBDSRV, 512, 424);

	//// Undistorted RGBD
	//multiTexture.AddTexture( &Register.m_pOutSRV, 512,424);

	//// Temp difference of kinect->m_pUndistortedRGBDSRV and Register.m_pOutSRV
	//strPScode = "\
	//	return abs(textures_0.SampleLevel(samColor,input.Tex,0) - textures_1.SampleLevel(samColor,input.Tex,0));";
	//multiTexture.AddTexture(nullptr, 512, 424, strPScode, "<float4>");
	 
	multiTexture.AddTexture(&PCVisual.m_pOutSRV, 640, 480, "", "<float4>", 
							std::bind(&PointCloudVisualizer::Resize,&PCVisual,_1,_2,_3),
							std::bind(&PointCloudVisualizer::HandleMessages,&PCVisual,_1,_2,_3,_4));

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
	V_RETURN(sensor->CreateResource(pd3dDevice));
	
	V_RETURN(multiTexture.CreateResource(pd3dDevice));

	//// Undistort color img
	//V_RETURN(UndistortColor.CreateResource(pd3dDevice,sensor->getColor_ppSRV(),1920,1080,true,
	//	XMFLOAT4(3.6544660501191997e-002, -4.0396988083370507e-002, -3.7317470747517489e-004, 0),
	//	XMFLOAT2(-1.8854329047101378e-003, -9.4736354256989353e-004),
	//	XMFLOAT2(-1.0445762291264687e+003, -1.0431358406353502e+003),
	//	XMFLOAT2(9.6773564641816870e+002, 5.4625864676602384e+002)));

	//// Undistort infrared img
	//V_RETURN(UndistortDepth.CreateResource(pd3dDevice, sensor->getDepth_ppSRV(), 512, 424, false,
	//	XMFLOAT4(8.5760834831004232e-002, -2.5095061439661859e-001, 7.7320261202870319e-002, 0),
	//	XMFLOAT2(-1.2027173455808845e-003, -3.0661909113997252e-004),
	//	XMFLOAT2(-3.6172484623525816e+002, -3.6121411187495357e+002),
	//	XMFLOAT2(2.5681568188916287e+002, 2.0554866916495337e+002)));

	/*V_RETURN(Register.CreateResource(pd3dDevice,sensor->getDepth_ppSRV(),XMUINT2(512,424),
										XMFLOAT2(-3.6172484623525816e+002, -3.6121411187495357e+002),
										XMFLOAT2(2.5681568188916287e+002, 2.0554866916495337e+002),
										sensor->getColor_ppSRV(),XMUINT2(1920,1080),
										XMFLOAT2(-1.0445762291264687e+003, -1.0431358406353502e+003),
										XMFLOAT2(9.6773564641816870e+002, 5.4625864676602384e+002),
										XMMATRIX(9.9999873760963776e-001, -1.4388716022985504e-003, -6.7411248534099118e-004,0,
												1.4410927879956242e-003, 9.9999349625375888e-001, 3.3061611817869897e-003, 0,
												6.6935095964733239e-004, -3.3071284667619051e-003, 9.9999430741909578e-001, 0,
												5.2117998649087637e-002, -5.5534117814779317e-004, -2.4816473876739821e-004, 1)));
*/
	/*V_RETURN(Register.CreateResource(pd3dDevice, &UndistortDepth.m_pOutSRV, XMUINT2(512, 424),
		XMFLOAT2(-3.6172484623525816e+002, -3.6121411187495357e+002),
		XMFLOAT2(2.5681568188916287e+002, 2.0554866916495337e+002),
		&UndistortColor.m_pOutSRV, XMUINT2(1920, 1080),
		XMFLOAT2(-1.0445762291264687e+003, -1.0431358406353502e+003),
		XMFLOAT2(9.6773564641816870e+002, 5.4625864676602384e+002),
		XMMATRIX(9.9999873760963776e-001, -1.4388716022985504e-003, -6.7411248534099118e-004, 0,
		1.4410927879956242e-003, 9.9999349625375888e-001, 3.3061611817869897e-003, 0,
		6.6935095964733239e-004, -3.3071284667619051e-003, 9.9999430741909578e-001, 0,
		5.2117998649087637e-002, -5.5534117814779317e-004, -2.4816473876739821e-004, 1)));*/

	V_RETURN(PCVisual.CreateResource(pd3dDevice, &kinect->m_pFinalizedRGBDSRV, 512, 424, true,
										XMFLOAT2(-3.6172484623525816e+002, -3.6121411187495357e+002),
										XMFLOAT2(2.5681568188916287e+002, 2.0554866916495337e+002)));
	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
										 const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
	HRESULT hr = S_OK;
	multiTexture.Resize(pd3dDevice,pBackBufferSurfaceDesc);
	return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
	PCVisual.Update(fElapsedTime);
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
								 double fTime, float fElapsedTime, void* pUserContext)
{
	sensor->UpdateTextures(pd3dImmediateContext);
	kinect->ProcessFinalizedRGBD(pd3dImmediateContext);
	//UndistortDepth.Render(pd3dImmediateContext);
	//UndistortColor.Render(pd3dImmediateContext);
	//Register.Render(pd3dImmediateContext);
	PCVisual.Render(pd3dImmediateContext);


	multiTexture.Render(pd3dImmediateContext);
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
	
	PCVisual.Release();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
	multiTexture.Release();
	sensor->Release();
	//UndistortDepth.Destory();
	//UndistortColor.Destory();
	//Register.Destory();
	PCVisual.Destory();
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


