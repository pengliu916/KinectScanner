//--------------------------------------------------------------------------------------
// KinectScanner
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"

#include "header.h"
#include "MultiTexturePresenter.h"
#include "FilteredPCL.h"
#include "VolumeTSDF.h"
#include "TSDFImages.h"
#include "PoseEstimator.h"
//hi
CDXUTDialogResourceManager      g_DialogResourceManager;
CDXUTTextHelper*                g_pTxtHelper = NULL;
wchar_t                         g_debugLine1[100];
wchar_t                         g_debugLine2[100];
wchar_t                         g_debugLine3[100];

MultiTexturePresenter           multiTexture = MultiTexturePresenter( 4, true, SUB_TEXTUREWIDTH, SUB_TEXTUREHEIGHT );
FilteredPCL                     pointCloud = FilteredPCL();
VolumeTSDF                      meshVolume = VolumeTSDF(VOXEL_SIZE, VOXEL_NUM_X, VOXEL_NUM_Y, VOXEL_NUM_Z);
//VolumeTSDF                      meshVolume = VolumeTSDF(0.0075f, 384, 384, 384);
TSDFImages                      tsdfImgs = TSDFImages(&meshVolume);
PoseEstimator                   poseEstimator = PoseEstimator();

bool                            g_bFirstFrame = true;
//--------------------------------------------------------------------------------------
//Global Variables only for test purpose..... 
//--------------------------------------------------------------------------------------

float time=0.05;
float fElpst;
bool nextIteration=true;
bool stepMode = false;

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

    
    swprintf(g_debugLine1,100,L"Debug Line 1...");
    swprintf(g_debugLine2,100,L"Debug Line 2...");
    swprintf(g_debugLine3,100,L"Debug Line 3...");

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
    g_pTxtHelper->SetInsertionPos( 2, 30 );
    g_pTxtHelper->DrawTextLine(g_debugLine2);
    g_pTxtHelper->SetInsertionPos( 2, 50 );
    g_pTxtHelper->DrawTextLine(g_debugLine3);
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
    V_RETURN( poseEstimator.CreateResource(pd3dDevice,tsdfImgs.m_pGeneratedTPC, &pointCloud.m_TransformedPC));
    V_RETURN( multiTexture.CreateResource( pd3dDevice, 
        //poseEstimator.m_pSumOfCoordSRV,
        poseEstimator.m_pKinectTPC->ppMeshNormalTexSRV,
        pointCloud.m_ppColorSRV,
        //poseEstimator.m_pTsdfTPC->ppMeshRGBZTexSRV,
        //poseEstimator.m_pTsdfTPC->ppMeshNormalTexSRV,
        &tsdfImgs.m_pFreeCamOutSRV,
        &tsdfImgs.m_pKinectOutSRV[2]
        //&pointCloud.m_bilateralFilter.m_pOutSRV
        //&pointCloud.m_normalGenerator.m_pOutSRV 
        ) );
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

    multiTexture.Resize();
    poseEstimator.Resize(pd3dimmediateContext);
    SAFE_RELEASE(pd3dimmediateContext);
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    tsdfImgs.Update(fElapsedTime);
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext )
{
    pointCloud.SetupPipeline(pd3dImmediateContext);
    pointCloud.Render(pd3dImmediateContext);
    
    /*tsdfImgs.Get3ImgForKinect ( pd3dImmediateContext );
    poseEstimator.Processing ( pd3dImmediateContext );
    meshVolume.Integrate( pd3dImmediateContext );
    tsdfImgs.GetRaycastImg( pd3dImmediateContext);*/

    int iterationTime = 0;

    if ( !stepMode ) nextIteration = true;

    if ( nextIteration ){
        nextIteration = false;
        bool tracked = false;
        if ( pointCloud.m_bUpdated ){
        //if ( true ){
            XMFLOAT3 vRotate = XMFLOAT3 ( 0, 0, 0 );
            XMFLOAT3 vTrans = XMFLOAT3 ( 0, 0, 0 );
            XMVECTOR vectorR = XMLoadFloat3 ( &vRotate );
            XMVECTOR vectorT = XMLoadFloat3 ( &vTrans );

            iterationTime = 0;

            do
            {
                pointCloud.m_TransformedPC.mModelM_pre = pointCloud.m_TransformedPC.mModelM_now;
                pointCloud.m_TransformedPC.mModelM_r_pre = pointCloud.m_TransformedPC.mModelM_r_now;

                tsdfImgs.Get3ImgForKinect ( pd3dImmediateContext );

                tracked = poseEstimator.Processing ( pd3dImmediateContext );

                if ( tracked ){
                    pointCloud.m_TransformedPC.m_vRotate += poseEstimator.m_vIncRotate;
                    XMMATRIX Ri = XMMatrixRotationRollPitchYawFromVector ( poseEstimator.m_vIncRotate );  
                    pointCloud.m_TransformedPC.m_vTrans = XMVector3Transform ( pointCloud.m_TransformedPC.m_vTrans, Ri ) + poseEstimator.m_vIncTran;

                    pointCloud.m_TransformedPC.mModelM_r_now *= XMMatrixRotationRollPitchYawFromVector ( poseEstimator.m_vIncRotate );
                    pointCloud.m_TransformedPC.mModelM_now = pointCloud.m_TransformedPC.mModelM_r_now * XMMatrixTranslationFromVector ( pointCloud.m_TransformedPC.m_vTrans );

                    vectorR += poseEstimator.m_vIncRotate;
                    vectorT += poseEstimator.m_vIncTran;
                
                    XMFLOAT3 r_overall;
                    XMFLOAT3 t_overall;
                    XMStoreFloat3 ( &r_overall, ( pointCloud.m_TransformedPC.m_vRotate ) * 180.0f / XM_PI );
                    XMStoreFloat3 ( &t_overall, ( pointCloud.m_TransformedPC.m_vTrans ) * 100.0f );
                    swprintf(g_debugLine1,100,L"%-8s x:% 6.5f y:% 6.5f z:% 6.5f",L"tran:", t_overall.x, t_overall.y, t_overall.z);
                    swprintf(g_debugLine2,100,L"%-8s a:% 6.5f b:% 6.5f g:% 6.5f",L"rotate:", r_overall.x, r_overall.y, r_overall.z );

                    iterationTime++; 
                }else if ( poseEstimator.m_bSigularMatrix ){
                    //stepMode = true;
                    swprintf(g_debugLine3,100,L"Track failed!!  Singular Matrix");
                }
            }while(tracked && iterationTime<15);//meshMatrixN.m_fError>=0.000000001);
            if ( tracked ) {
                float rnorm = norm ( vectorR );
                float tnorm = norm ( vectorT );
                //pointCloud.m_TransformedPC.m_vRotate += vectorR;
                //pointCloud.m_TransformedPC.m_vTrans = vectorT;
                swprintf(g_debugLine3,100,L"Track success  % 6.5f", tnorm + rnorm);
                if (  rnorm + tnorm > 0.001f ) meshVolume.Integrate( pd3dImmediateContext );
            }
        }
    }
    if( stepMode ) tsdfImgs.GetRaycastImg( pd3dImmediateContext );
    tsdfImgs.GetRaycastImg( pd3dImmediateContext);

    multiTexture.Render( pd3dImmediateContext );
    RenderText();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    multiTexture.Release();
    pointCloud.Release();
    tsdfImgs.Release();
    meshVolume.Release();
    poseEstimator.Release();


    
    g_DialogResourceManager.OnD3D11DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
    multiTexture.HandleMessages( hWnd, uMsg, wParam, lParam );
    pointCloud.HandleMessages( hWnd, uMsg, wParam, lParam );
    poseEstimator.HandleMessages( hWnd, uMsg, wParam, lParam );
    meshVolume.HandleMessages( hWnd, uMsg, wParam, lParam );
    tsdfImgs.HandleMessages( hWnd, uMsg, wParam, lParam );
    switch(uMsg)
    {
    case WM_KEYDOWN:
        {
            int nKey = static_cast<int>(wParam);

            if (nKey == 'O')
            {
                stepMode=!stepMode;
                if( stepMode )
                {
                    multiTexture.m_ppInputSRV[3] = &tsdfImgs.m_pFreeCamOutSRV;
                }
                else
                {
                    multiTexture.m_ppInputSRV[3] = &tsdfImgs.m_pKinectOutSRV[2];
                }
            }
            if (nKey == 'I')
            {
                nextIteration=true;
            }
            break;
        }
    }
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

    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 640, 480 );
    DXUTMainLoop(); // Enter into the DXUT ren  der loop

    // Perform any application-level cleanup here

    return DXUTGetExitCode();
}


