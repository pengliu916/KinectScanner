//--------------------------------------------------------------------------------------
// Sensor Explorer
//--------------------------------------------------------------------------------------
#include <thread>
#include "DXUT.h"
#include "DXUTgui.h"
#include <opencv2\core\core.hpp>
#include "opencv2/highgui/highgui.hpp"
#include <opencv2\video\video.hpp>

#include "TiledTextures.h"

#include "Kinect2Sensor.h"


#include "PointCloudVisualizer.h"


#define RECORD 0

using namespace std::placeholders;
using namespace cv;


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
std::string					strVideoDepthName = "Kinect2PengDepth.avi";
std::string					strVideoColorName = "Kinect2PengColor.avi";

TiledTextures				multiTexture = TiledTextures();
#if RECORD
Kinect2Sensor*				sensor = new Kinect2Sensor();
#endif
PointCloudVisualizer		PCVisual = PointCloudVisualizer();
#if RECORD
cv::VideoWriter depthWriter;
cv::VideoWriter colorWriter;
#else
cv::VideoCapture	depthReader(strVideoDepthName);
cv::VideoCapture	colorReader(strVideoColorName);
#endif
ID3D11Texture2D*			g_pCpuReadableTex;
ID3D11Texture2D*			g_pRGBDTex;
ID3D11ShaderResourceView*	g_pRGBDSRV;

Mat							matColor;
Mat							matDepth;
Mat							matConvert;

ushort*						g_sDepth;
uchar*						g_cColor;

float						g_playbackTimer=0;
//--------------------------------------------------------------------------------------
// Initialization
//--------------------------------------------------------------------------------------

void Mat16UC1toMat8UC3(cv::Mat& inMat, cv::Mat& outMat)
{
	outMat.create(inMat.size(), CV_8UC3);
	for (int y = 0; y < inMat.size().height; ++y)
	{
		cv::Vec3b* pColorRow = outMat.ptr<cv::Vec3b>(y);
		for (int x = 0; x < inMat.size().width; ++x)
		{
			short value = *inMat.ptr<short>(y, x);
			pColorRow[x] = cv::Vec3b((value & 0xFF00) >> 8, (value & 0x00FF), 0);
		}
	}
}

void Mat8UC3toMat8UC4(cv::Mat& inMat, cv::Mat& outMat)
{
	//outMat.create( inMat.size(), CV_8UC4 );
	for (int y = 0; y < inMat.size().height; ++y)
	{
		cv::Vec4b* pColorRow = outMat.ptr<cv::Vec4b>(y);
		for (int x = 0; x < inMat.size().width; ++x)
		{
			uchar* value = inMat.ptr<uchar>(y, x);
			pColorRow[x] = cv::Vec4b(value[0], value[1], value[2], 0);
		}
	}
}

void Mat8UC3toMat16UC1(cv::Mat& inMat, cv::Mat& outMat)
{
	//outMat.create( inMat.size(), CV_16UC1 );
	for (int y = 0; y < inMat.size().height; ++y)
	{
		short* pColorRow = outMat.ptr<short>(y);
		for (int x = 0; x < inMat.size().width; ++x)
		{
			uchar* value = inMat.ptr<uchar>(y, x);
			pColorRow[x] = (short)(value[0] << 8 | value[1]);
		}
	}
}


HRESULT Initial()
{
	HRESULT hr = S_OK;
	V_RETURN(multiTexture.Initial());
	string strPScode;
#if RECORD
	V_RETURN(sensor->Initialize());
	// Original color img
	multiTexture.AddTexture(sensor->getColor_ppSRV(), 1920, 1080);

	// Original infrared img
	strPScode = "\
	int2 texCoord=input.Tex*float2(512,424);\n\
	uint infrared=texture.Load(int3(texCoord,0));\n\
	float norInfrared = pow(infrared/65535.f,0.32);\n\
	return float4(norInfrared,norInfrared,norInfrared,0);";
	multiTexture.AddTexture( sensor->getInfrared_ppSRV(), 512, 424, strPScode, "<uint>" );

	// Color contamination free intermmediate RGBD
	multiTexture.AddTexture(sensor->getRGBD_ppSRV(), 512, 424);

	// Color Contamination free RGBD
	//multiTexture.AddTexture(&kinect->m_pColRemovalRGBDSRV, 512, 424);

	multiTexture.AddTexture(&PCVisual.m_pOutSRV, 640, 480, "", "<float4>",
							std::bind(&PointCloudVisualizer::Resize, &PCVisual, _1, _2, _3),
							std::bind(&PointCloudVisualizer::HandleMessages, &PCVisual, _1, _2, _3, _4));



	depthWriter.open(strVideoDepthName, CV_FOURCC('L', 'A', 'G', 'S'), 30, cv::Size(512, 424));
	colorWriter.open(strVideoColorName, CV_FOURCC('L', 'A', 'G', 'S'), 30, cv::Size(512, 424));
	if (!depthWriter.isOpened() || !colorWriter.isOpened()){
		cout<<"Cannot open video file to write."<<endl;
		exit(1);
	}
#else
	// Color contamination free intermmediate RGBD
	multiTexture.AddTexture(&g_pRGBDSRV, 512, 424);

	// Color Contamination free RGBD
	//multiTexture.AddTexture(&kinect->m_pColRemovalRGBDSRV, 512, 424);

	multiTexture.AddTexture(&PCVisual.m_pOutSRV, 640, 480, "", "<float4>",
							std::bind(&PointCloudVisualizer::Resize, &PCVisual, _1, _2, _3),
							std::bind(&PointCloudVisualizer::HandleMessages, &PCVisual, _1, _2, _3, _4));


#endif
	g_sDepth = new ushort[512*424];
	g_cColor = new uchar[512*424*3];

	matColor.create(424,512,CV_8UC3);
	matDepth.create(424,512,CV_16UC1);
	matConvert.create(424,512,CV_8UC3);
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

	V_RETURN(multiTexture.CreateResource(pd3dDevice));
#if RECORD
	V_RETURN(sensor->CreateResource(pd3dDevice));
	V_RETURN(PCVisual.CreateResource(pd3dDevice, sensor->getRGBD_ppSRV(), 512, 424, true,
		XMFLOAT2(-3.6172484623525816e+002, -3.6121411187495357e+002),
		XMFLOAT2(2.5681568188916287e+002, 2.0554866916495337e+002)));
#else
	V_RETURN(PCVisual.CreateResource(pd3dDevice, &g_pRGBDSRV, 512, 424, true,
		XMFLOAT2(-3.6172484623525816e+002, -3.6121411187495357e+002),
		XMFLOAT2(2.5681568188916287e+002, 2.0554866916495337e+002)));
#endif
	// Create depth texture and its resource view
	D3D11_TEXTURE2D_DESC depthTexDesc = { 0 };
	depthTexDesc.Width = 512;
	depthTexDesc.Height = 424;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Usage = D3D11_USAGE_STAGING;
	depthTexDesc.BindFlags = 0;
	depthTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	depthTexDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateTexture2D(&depthTexDesc, NULL, &g_pCpuReadableTex));

	// Create infrared texture and its resource view
	D3D11_TEXTURE2D_DESC rgbdTexDesc = { 0 };
	rgbdTexDesc.Width = 512;
	rgbdTexDesc.Height = 424;
	rgbdTexDesc.MipLevels = 1;
	rgbdTexDesc.ArraySize = 1;
	rgbdTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	rgbdTexDesc.SampleDesc.Count = 1;
	rgbdTexDesc.SampleDesc.Quality = 0;
	rgbdTexDesc.Usage = D3D11_USAGE_DYNAMIC;
	rgbdTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	rgbdTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	rgbdTexDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateTexture2D(&rgbdTexDesc, NULL, &g_pRGBDTex));
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_pRGBDTex, NULL, &g_pRGBDSRV));

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
										 const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
	HRESULT hr = S_OK;
	multiTexture.Resize(pd3dDevice, pBackBufferSurfaceDesc);
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
#if RECORD
	if(sensor->UpdateTextures(pd3dImmediateContext,true,true,true,true)){
	pd3dImmediateContext->CopyResource(g_pCpuReadableTex,sensor->m_pAddDepthTex);
	D3D11_MAPPED_SUBRESOURCE resource;
	pd3dImmediateContext->Map(g_pCpuReadableTex,0,D3D11_MAP_READ,0,&resource);
	float* pData = (float*)resource.pData;
	for(int i=0,j=0,k=0;i<512*424*4;i+=4){
		g_cColor[j++] = pData[i+2] * 255;
		g_cColor[j++] = pData[i+1] * 255;
		g_cColor[j++] = pData[i] * 255;
		g_sDepth[k++] = (ushort)(pData[i+3]<=0?0:pData[i+3]*1000);
	}
	pd3dImmediateContext->Unmap(g_pCpuReadableTex,0);
	matColor = cv::Mat(424,512,CV_8UC3,g_cColor);
	colorWriter << matColor;
	matDepth = cv::Mat(424,512,CV_16UC1,g_sDepth);
	Mat16UC1toMat8UC3(matDepth,matConvert);
	imshow("Converted",matConvert);
	depthWriter << matConvert;

	}
#else
	g_playbackTimer+=fElapsedTime;
	if (g_playbackTimer > 0.2){
		g_playbackTimer = 0;
		depthReader >> matConvert;
		colorReader >> matColor;

		if (matDepth.data && matColor.data){
			imshow("coded", matConvert);
			Mat8UC3toMat16UC1(matConvert, matDepth);
			imshow("depth", matDepth);
			D3D11_MAPPED_SUBRESOURCE resource;
			pd3dImmediateContext->Map(g_pRGBDTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
			float* data = (float*)resource.pData;
			UINT pos = 0;
			unsigned char* ptrColor = (unsigned char*)(matColor.data);
			ushort* ptrDepth = (ushort*)(matDepth.data);
			for (int i = 0; i < matDepth.rows; i++){
				for (int j = 0; j < matDepth.cols; j++){
					data[pos++] = ptrColor[matColor.step*i + j*3 + 2] / 255.f;
					data[pos++] = ptrColor[matColor.step*i + j*3 + 1] / 255.f;
					data[pos++] = ptrColor[matColor.step*i + j*3 + 0] / 255.f;
					data[pos++] = ptrDepth[512*i+j] /1000.f;
				}
			}
			pd3dImmediateContext->Unmap(g_pRGBDTex, 0);
		}
	}
#endif
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
#if RECORD
	sensor->Release();
	depthWriter.release();
	colorWriter.release();
#endif
	PCVisual.Destory();
	SAFE_RELEASE(g_pCpuReadableTex);
	SAFE_RELEASE(g_pRGBDSRV);
	SAFE_RELEASE(g_pRGBDTex);
	delete g_cColor;
	delete g_sDepth;
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


