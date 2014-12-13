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
wchar_t                         g_debugLine2[100];
wchar_t                         g_debugLine3[100];

TiledTextures					multiTexture = TiledTextures();
FilteredPCL                     pointCloud = FilteredPCL(D_W, D_H);
VolumeTSDF                      meshVolume = VolumeTSDF(VOXEL_SIZE, VOXEL_NUM_X, VOXEL_NUM_Y, VOXEL_NUM_Z);
TSDFImages                      tsdfImgs = TSDFImages(&meshVolume);
PoseEstimator                   poseEstimator = PoseEstimator(D_W, D_H);
//HistoPyramidMC					histoPyraimdMC = HistoPyramidMC(&meshVolume);
bool                            g_bFirstFrame = true;
//--------------------------------------------------------------------------------------
//Global Variables only for test purpose..... 
//--------------------------------------------------------------------------------------

bool g_bGetNextFrame = false;
bool g_bStepMode = STEP_MODE;

float norm(XMVECTOR vector)
{
	XMVECTOR length = XMVector3Length(vector);
	float distance = 0.0f;
	XMStoreFloat(&distance, length);
	return distance;
};
//--------------------------------------------------------------------------------------
//Initialization
//--------------------------------------------------------------------------------------
HRESULT Initial()
{
	HRESULT hr = S_OK;
	V_RETURN(pointCloud.Initial())
		V_RETURN(multiTexture.Initial());
	// Use TSDFImages's Generated RGBD
	V_RETURN(poseEstimator.Initial(tsdfImgs.m_pGeneratedTPC, &pointCloud.m_TransformedPC));
	// Use HistoPyramidMC's Generated RGBD
	//V_RETURN(poseEstimator.Initial(histoPyraimdMC.m_pGeneratedTPC, &pointCloud.m_TransformedPC));

	multiTexture.AddTexture(pointCloud.m_ppRGBDSRV, D_W, D_H);
	multiTexture.AddTexture(&tsdfImgs.m_pKinectOutSRV[2], D_W, D_H);
	multiTexture.AddTexture(&tsdfImgs.m_pFarNearSRV, D_W, D_H,
							"float4 result = texture.Load(int3(input.Tex*float2(512,424),0));\n\
							 if(result.r>20 && result.a<0) color = float4(0,0,0,0);\n\
							 else color = result.a*0.5f*float4(1,1,1,1);\n\
							 return color;\n");
	multiTexture.AddTexture(&tsdfImgs.m_pFarNearSRV, D_W, D_H,
							"float4 result = texture.Load(int3(input.Tex*float2(512,424),0));\n\
							 if(result.r>20 && result.a<0) color = float4(0,0,0,0);\n\
							 else color = result.r*0.5f*float4(1,1,1,1);\n\
							 return color;\n");
	multiTexture.AddTexture(&tsdfImgs.m_pFarNearSRV, D_W, D_H,
							"float4 result = texture.Load(int3(input.Tex*float2(512,424),0));\n\
							 if(result.r>20 && result.a<0) color = float4(0,0,0,0);\n\
							 else color = abs(result.a - result.r)*0.5f*float4(1,1,1,1);\n\
							 return color;\n");
	multiTexture.AddTexture(tsdfImgs.m_pGeneratedTPC->ppMeshRGBZTexSRV, D_W, D_H, "", "<float4>",
							nullptr, std::bind(&TSDFImages::HandleMessages, &tsdfImgs, _1, _2, _3, _4));
	multiTexture.AddTexture(poseEstimator.m_pKinectTPC->ppMeshNormalTexSRV, D_W, D_H);
	multiTexture.AddTexture(tsdfImgs.m_pGeneratedTPC->ppMeshNormalTexSRV, D_W, D_H);
	/*multiTexture.AddTexture(&histoPyraimdMC.m_pOutSRV,640,480,"","<float4>",
	std::bind(&HistoPyramidMC::Resize,&histoPyraimdMC,_1,_2,_3),
	std::bind(&HistoPyramidMC::HandleMessages,&histoPyraimdMC,_1,_2,_3,_4));
	*/
	swprintf(g_debugLine1, 100, L"Debug Line 1...");
	swprintf(g_debugLine2, 100, L"Debug Line 2...");
	swprintf(g_debugLine3, 100, L"Debug Line 3...");

	return hr;
}

//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
	g_pTxtHelper->Begin();
	g_pTxtHelper->SetForegroundColor(XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f));
	//g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
	g_pTxtHelper->SetInsertionPos(2, 10);
	g_pTxtHelper->DrawTextLine(g_debugLine1);
	g_pTxtHelper->SetInsertionPos(2, 30);
	g_pTxtHelper->DrawTextLine(g_debugLine2);
	g_pTxtHelper->SetInsertionPos(2, 50);
	g_pTxtHelper->DrawTextLine(g_debugLine3);
	g_pTxtHelper->End();
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

	ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
	V_RETURN(g_DialogResourceManager.OnD3D11CreateDevice(pd3dDevice, pd3dImmediateContext));
	g_pTxtHelper = new CDXUTTextHelper(pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 2);

	V_RETURN(pointCloud.CreateResource(pd3dDevice));
	V_RETURN(meshVolume.CreateResource(pd3dDevice, &pointCloud.m_TransformedPC));
	V_RETURN(tsdfImgs.CreateResource(pd3dDevice));
	V_RETURN(poseEstimator.CreateResource(pd3dDevice));
	//V_RETURN(histoPyraimdMC.CreateResource(pd3dDevice,meshVolume.m_pColVolumeSRV,meshVolume.m_pDWVolumeSRV));
	V_RETURN(multiTexture.CreateResource(pd3dDevice));

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
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
										 const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
	HRESULT hr = S_OK;
	ID3D11DeviceContext* pd3dimmediateContext;
	pd3dDevice->GetImmediateContext(&pd3dimmediateContext);

	V_RETURN(g_DialogResourceManager.OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc));

	multiTexture.Resize(pd3dDevice, pBackBufferSurfaceDesc);

	SAFE_RELEASE(pd3dimmediateContext);
	return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
	// whether to get the next frame or not
	if (!g_bStepMode) g_bGetNextFrame = true;

	// Update the virtual cam of the model viewer of HistoPyramidMC
	//histoPyraimdMC.Update(fElapsedTime);
	tsdfImgs.Update(fElapsedTime);
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
								 double fTime, float fElapsedTime, void* pUserContext)
{
	if (g_bGetNextFrame){
		g_bGetNextFrame = false;

		bool bTracked = false;

		// Get new depth and color frmae from RGBD sensor
		pointCloud.Render(pd3dImmediateContext);

		// Only run the algorithm if we get new data
		if (pointCloud.m_bUpdated){
			XMVECTOR vectorR = XMVectorZero();
			XMVECTOR vectorT = XMVectorZero();

			int iIterationCount = 0;

			do{
				pointCloud.m_TransformedPC.mPreFrame = pointCloud.m_TransformedPC.mCurFrame;
				pointCloud.m_TransformedPC.mPreRotation = pointCloud.m_TransformedPC.mCurRotation;

				// Get RGBD and Normal data from TSDF with new pose 
				tsdfImgs.Get3ImgForKinect(pd3dImmediateContext);

				// Find transformation matrix, return false if can't find one
				bTracked = poseEstimator.Processing(pd3dImmediateContext);

				if (bTracked){
					// Update the Sensor Pose info
					pointCloud.m_TransformedPC.vRotation += poseEstimator.m_vIncRotate;
					XMMATRIX Ri = XMMatrixRotationRollPitchYawFromVector(poseEstimator.m_vIncRotate);
					pointCloud.m_TransformedPC.vTranslation = XMVector3Transform(pointCloud.m_TransformedPC.vTranslation, Ri) + poseEstimator.m_vIncTran;

					// Apply the new Sensor Pose
					pointCloud.m_TransformedPC.mCurRotation *= XMMatrixRotationRollPitchYawFromVector(poseEstimator.m_vIncRotate);
					pointCloud.m_TransformedPC.mCurFrame = pointCloud.m_TransformedPC.mCurRotation * XMMatrixTranslationFromVector(pointCloud.m_TransformedPC.vTranslation);

					// Keep track of the incremental transformation for this data frame
					vectorR += poseEstimator.m_vIncRotate;
					vectorT += poseEstimator.m_vIncTran;

					// Increase the iteration counter for termination condition checking
					iIterationCount++;
				} else if (poseEstimator.m_bSigularMatrix){
					//g_bStepMode = true;
					swprintf(g_debugLine3, 100, L"Track failed!!  Singular Matrix");
				}
			} while (bTracked && iIterationCount<15);
			if (bTracked) {
				float rnorm = norm(vectorR);
				float tnorm = norm(vectorT);

				XMFLOAT3 r_overall;
				XMFLOAT3 t_overall;
				XMStoreFloat3(&r_overall, (pointCloud.m_TransformedPC.vRotation) * 180.0f / XM_PI);
				XMStoreFloat3(&t_overall, (pointCloud.m_TransformedPC.vTranslation) * 100.0f);
				swprintf(g_debugLine1, 100, L"%-8s x:% 6.5f y:% 6.5f z:% 6.5f", L"tran:", t_overall.x, t_overall.y, t_overall.z);
				swprintf(g_debugLine2, 100, L"%-8s a:% 6.5f b:% 6.5f g:% 6.5f", L"rotate:", r_overall.x, r_overall.y, r_overall.z);

				swprintf(g_debugLine3, 100, L"Track success  % 6.5f", tnorm + rnorm);
				if (rnorm + tnorm > 0.001f) meshVolume.Integrate(pd3dImmediateContext);
			}
			// If this is the first frame, then update the TSDF anyway
			if (g_bFirstFrame){
				g_bFirstFrame = false;
				meshVolume.Integrate(pd3dImmediateContext);
			}
		}
	}


	// Render the in-process mesh
	//histoPyraimdMC.Render(pd3dImmediateContext,false);

	// Render all sub texture to screen
	multiTexture.Render(pd3dImmediateContext);

	// Render the text
	//RenderText();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
	g_DialogResourceManager.OnD3D11ReleasingSwapChain();
	//histoPyraimdMC.Release();
	tsdfImgs.Release();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
	multiTexture.Release();
	pointCloud.Release();
	meshVolume.Release();
	poseEstimator.Release();
	tsdfImgs.Destory();
	//histoPyraimdMC.Destory();


	g_DialogResourceManager.OnD3D11DestroyDevice();
	CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
	DXUTGetGlobalResourceCache().OnDestroyDevice();
	SAFE_DELETE(g_pTxtHelper);

	if (d3dDebug != nullptr)
	{
		d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
		d3dDebug = nullptr;
	}

}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
						 bool* pbNoFurtherProcessing, void* pUserContext)
{
	multiTexture.HandleMessages(hWnd, uMsg, wParam, lParam);
	pointCloud.HandleMessages(hWnd, uMsg, wParam, lParam);
	poseEstimator.HandleMessages(hWnd, uMsg, wParam, lParam);
	meshVolume.HandleMessages(hWnd, uMsg, wParam, lParam);
	switch (uMsg)
	{
	case WM_KEYDOWN:
	{
		int nKey = static_cast<int>(wParam);

		if (nKey == 'O')
		{
			g_bStepMode = !g_bStepMode;
			/* if( g_bStepMode )
			{
			multiTexture.m_ppInputSRV[3] = &tsdfImgs.m_pFreeCamOutSRV;
			}
			else
			{
			multiTexture.m_ppInputSRV[3] = &tsdfImgs.m_pKinectOutSRV[2];
			}*/
		}
		if (nKey == 'I')
		{
			g_bGetNextFrame = true;
		}
		break;
	}
	}
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
	DXUTCreateWindow(L"KinectScanner");

	Initial();

	DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1280, 800);
	DXUTMainLoop(); // Enter into the DXUT ren  der loop

	// Perform any application-level cleanup here

	return DXUTGetExitCode();
}


