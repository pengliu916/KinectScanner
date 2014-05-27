#pragma once

#include <D3D11.h>
#include"DXUT.h"
#include <DirectXMath.h>
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include <iostream>
#include <vector>
#include "header.h"

using namespace std;

HRESULT CompileFormString( string code,
                           const D3D_SHADER_MACRO* pDefines,
                           LPCSTR pEntrypoint, LPCSTR pTarget,
                           UINT Flags1, UINT Flags2,
                           ID3DBlob** ppCode ){
    HRESULT hr;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    Flags1 |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob = nullptr;
    hr = D3DCompile( code.c_str(), code.size(), NULL, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, pEntrypoint, pTarget, Flags1, Flags2, ppCode, &pErrorBlob );
#pragma warning( suppress : 6102 )
    if( pErrorBlob )
    {
        OutputDebugStringA( reinterpret_cast< const char* >( pErrorBlob->GetBufferPointer() ) );
        pErrorBlob->Release();
    }

    return hr;
};

struct TiledTexObj{
    ID3D11ShaderResourceView**      ppInputSRV;
    string                          strTexFormat;
    string                          strPScode;
};

class TiledTextures
{
public:
    ID3D11VertexShader*             m_pPassVS;
    ID3D11GeometryShader*           m_pTiledQuadGS;
    ID3D11PixelShader*              m_pTexPS;
    ID3D11InputLayout*              m_pPassIL;
    ID3D11Buffer*                   m_pPassVB;

    vector<TiledTexObj>             m_vecTiledObjs;

    ID3D11SamplerState*             m_pSS;

    ID3D11Texture2D*                m_pOutTex;
    ID3D11RenderTargetView*         m_pOutRTV;
    ID3D11ShaderResourceView*       m_pOutSRV;

    ID3D11ShaderResourceView**      m_ppInputSRVs;
    ID3D11ShaderResourceView**      m_ppNullSRVs;

    D3D11_VIEWPORT                  m_RTviewport;

    UINT                            m_uTexCount;
    UINT                            m_uTextureWidth;
    UINT                            m_uTextureHeight;
    UINT                            m_uRTwidth;
    UINT                            m_uRTheight;
    UINT                            m_uTileCount_x;
    UINT                            m_uTileCount_y;
    float                           m_fRTaspectRatio;

    bool                            m_bDirectToBackBuffer;

    string GenerateShaderCode()
    {
        ostringstream  shaderCode;
        for( int i = 0; i < m_vecTiledObjs.size(); i++ ){
            shaderCode << "Texture2D" << m_vecTiledObjs[i].strTexFormat << " textures_" << i << ";\n";
        }
        //int idx = 0;
        //for(auto i:m_vecTiledObjs)
        //    shaderCode << "Texture2D" << i.strTexFormat << " textures_" << idx++ << ";\n";
        //
        shaderCode << "SamplerState samColor : register(s0);\n";
        shaderCode << "struct GS_INPUT{};\n";
        shaderCode << "struct PS_INPUT{float4 Pos:SV_POSITION;float2 Tex:TEXCOORD0;uint PrimID:SV_PrimitiveID;};\n";
        shaderCode << "GS_INPUT VS(){GS_INPUT output=(GS_INPUT)0;return output;}\n";
        m_uTileCount_x = ceil( sqrt( m_vecTiledObjs.size() ) );// Tile count along horizontal axis
        m_uTileCount_y = ceil( m_vecTiledObjs.size() / ( float )m_uTileCount_x );// Tile count along vertical axis
        float subScreen_x = 2.0f / m_uTileCount_x;// The dimension of tile container in screen space, x axis;
        float subScreen_y = 2.0f / m_uTileCount_y;// The dimension of tile container in screen space, y axis;

        // For generating geometry shader
        shaderCode << "[maxvertexcount(4)]\n";
        shaderCode << "void GS(point GS_INPUT particles[1], uint primID:SV_PrimitiveID, inout TriangleStream<PS_INPUT> triStream){\n";
        shaderCode << "   PS_INPUT output;\n";
        shaderCode << "   float offset=(float)primID;\n";
        shaderCode << "   output.Pos=float4(-1.f+" << subScreen_x << "*(primID%" << m_uTileCount_x << "),1.f-" << subScreen_y << "*(primID/" << m_uTileCount_x << "),0.1f,1.f);\n";
        shaderCode << "   output.Tex=float2(0.f,0.f);\n";
        shaderCode << "   output.PrimID=primID;\n";
        shaderCode << "   triStream.Append(output);\n";
        shaderCode << "   output.Pos=float4(-1.f+" << subScreen_x << "*(1.f+primID%" << m_uTileCount_x << "),1.f-" << subScreen_y << "*(primID/" << m_uTileCount_x << "),0.1f,1.f);\n";
        shaderCode << "   output.Tex=float2(1.f,0.f);\n";
        shaderCode << "   output.PrimID=primID;\n";
        shaderCode << "   triStream.Append(output);\n";
        shaderCode << "   output.Pos=float4(-1.f+" << subScreen_x << "*(primID%" << m_uTileCount_x << "),1.f-" << subScreen_y << "*(1.f+primID/" << m_uTileCount_x << "),0.1f,1.f);\n";
        shaderCode << "   output.Tex=float2(0.f,1.f);\n";
        shaderCode << "   output.PrimID=primID;\n";
        shaderCode << "   triStream.Append(output);\n";
        shaderCode << "   output.Pos=float4(-1.f+" << subScreen_x << "*(1.f+primID%" << m_uTileCount_x << "),1.f-" << subScreen_y << "*(1.f+primID/" << m_uTileCount_x << "),0.1f,1.f);\n";
        shaderCode << "   output.Tex=float2(1.f,1.f);\n";
        shaderCode << "   output.PrimID=primID;\n";
        shaderCode << "   triStream.Append(output);\n";
        shaderCode << "}\n";

        // For generating pixel shader
        shaderCode << "float4 PS(PS_INPUT input):SV_Target{\n";
        shaderCode << "   float4 color;\n";
        shaderCode << "   [branch] switch(input.PrimID){\n";

        size_t index;
        for( int i = 0; i < m_vecTiledObjs.size(); i++ ){
            ostringstream replace;
            string replacement;
            replace << "textures_" << i;
            replacement = replace.str();
            index = m_vecTiledObjs[i].strPScode.find("texture",0);
            if(index!=string::npos)
                m_vecTiledObjs[i].strPScode.replace(index,7,replacement);
            shaderCode << "case " << i <<":\n";
            shaderCode << m_vecTiledObjs[i].strPScode << "\n";
        }
        shaderCode << "default:\n";
        shaderCode << "color=float4(input.Tex.xy,0,1);\n";
        shaderCode << "return color;\n";
        shaderCode << "}}\n" << endl;
        return shaderCode.str();
    }

    TiledTextures( bool bRenderToBackbuffer = true, UINT width = 640, UINT height = 480 )
    {
        m_bDirectToBackBuffer = bRenderToBackbuffer;
        m_uTextureWidth = width;
        m_uTextureHeight = height;
    }

    HRESULT Initial()
    {
        return S_OK;
    }

    void ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings )
    {
        //DXGI_SWAP_CHAIN_DESC sd;
        //ZeroMemory( &sd, sizeof( sd ) );
        //d3d11
        //int widthNum;
        //int heightNum;
        //switch( m_uTextureNumber )
        //{
        //case 1:
        //    widthNum = 1; heightNum = 1; break;
        //case 2:
        //    widthNum = 2; heightNum = 1; break;
        //case 3:
        //    widthNum = 2; heightNum = 2; break;
        //case 4:
        //    widthNum = 2; heightNum = 2; break;
        //case 5:
        //    widthNum = 3; heightNum = 2; break;
        //case 6:
        //    widthNum = 3; heightNum = 2; break;
        //default:
        //    widthNum = 1; heightNum = 1;
        //}

        //sd.BufferCount = 1;
        //sd.BufferDesc.Width = m_uTextureWidth*widthNum;
        //sd.BufferDesc.Height = m_uTextureHeight*heightNum;
        //sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        ////sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        //sd.BufferDesc.RefreshRate.Numerator = 60;
        //sd.BufferDesc.RefreshRate.Denominator = 1;
        //sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        //sd.OutputWindow = pDeviceSettings->d3d11.sd.OutputWindow;
        //sd.SampleDesc.Count = 1;
        //sd.SampleDesc.Quality = 0;
        //sd.Windowed = TRUE;
        pDeviceSettings->d3d11.sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    void AddTexture( ID3D11ShaderResourceView** ppInputSRV, string strPScode = "", string strFormat = "<float4>" )
    {
        TiledTexObj texObj;
        string defaultPScode = "color = texture.Sample(samColor, input.Tex);\n return color;";
        texObj.ppInputSRV = ppInputSRV;
        texObj.strPScode = strPScode.empty() ? defaultPScode : strPScode;
        texObj.strTexFormat = strFormat;
        m_vecTiledObjs.push_back(texObj);
    }

    HRESULT CreateResource( ID3D11Device* pd3dDevice )
    {
        HRESULT hr = S_OK;

        string shaderCode = GenerateShaderCode();

        ID3DBlob* pVSBlob = NULL;

        V_RETURN( CompileFormString( shaderCode, nullptr, "VS", "vs_5_0", COMPILE_FLAG, 0, &pVSBlob ) )
        V_RETURN( pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pPassVS ) );
        DXUT_SetDebugName( m_pPassVS, "m_pPassVS" );

        ID3DBlob* pGSBlob = NULL;
        V_RETURN( CompileFormString( shaderCode, nullptr, "GS", "gs_5_0", COMPILE_FLAG, 0, &pGSBlob ) )
        V_RETURN( pd3dDevice->CreateGeometryShader( pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), NULL, &m_pTiledQuadGS ) );
        DXUT_SetDebugName( m_pTiledQuadGS, "m_pTiledQuadGS" );
        pGSBlob->Release();

        ID3DBlob* pPSBlob = NULL;
        V_RETURN( CompileFormString( shaderCode, nullptr, "PS", "ps_5_0", COMPILE_FLAG, 0, &pPSBlob ) )
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

        // Create rendertarget resource
        if( !m_bDirectToBackBuffer )
        {
            D3D11_TEXTURE2D_DESC   RTtextureDesc = { 0 };
            RTtextureDesc.Width = m_uRTwidth;
            RTtextureDesc.Height = m_uRTheight;
            RTtextureDesc.MipLevels = 1;
            RTtextureDesc.ArraySize = 1;
            RTtextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            //RTtextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

            RTtextureDesc.SampleDesc.Count = 1;
            RTtextureDesc.SampleDesc.Quality = 0;
            RTtextureDesc.Usage = D3D11_USAGE_DEFAULT;
            RTtextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            RTtextureDesc.CPUAccessFlags = 0;
            RTtextureDesc.MiscFlags = 0;
            V_RETURN( pd3dDevice->CreateTexture2D( &RTtextureDesc, NULL, &m_pOutTex ) );
            DXUT_SetDebugName( m_pOutTex, "m_pOutTex" );

            D3D11_RENDER_TARGET_VIEW_DESC      RTViewDesc;
            ZeroMemory( &RTViewDesc, sizeof( RTViewDesc ) );
            RTViewDesc.Format = RTtextureDesc.Format;
            RTViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            RTViewDesc.Texture2D.MipSlice = 0;
            V_RETURN( pd3dDevice->CreateRenderTargetView( m_pOutTex, &RTViewDesc, &m_pOutRTV ) );
            DXUT_SetDebugName( m_pOutRTV, "m_pOutRTV" );

            D3D11_SHADER_RESOURCE_VIEW_DESC      SRViewDesc;
            ZeroMemory( &SRViewDesc, sizeof( SRViewDesc ) );
            SRViewDesc.Format = RTtextureDesc.Format;
            SRViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRViewDesc.Texture2D.MostDetailedMip = 0;
            SRViewDesc.Texture2D.MipLevels = 1;
            V_RETURN( pd3dDevice->CreateShaderResourceView( m_pOutTex, &SRViewDesc, &m_pOutSRV ) );
            DXUT_SetDebugName( m_pOutSRV, "m_pOutSRV" );
        }

        m_RTviewport.Width = m_uTextureWidth*m_uTileCount_x;
        m_RTviewport.Height = m_uTextureHeight*m_uTileCount_y;
        m_RTviewport.MinDepth = 0.0f;
        m_RTviewport.MaxDepth = 1.0f;
        m_RTviewport.TopLeftX = 0;
        m_RTviewport.TopLeftY = 0;

        m_fRTaspectRatio = (float)m_uTextureWidth*m_uTileCount_x / ((float)m_uTextureHeight*m_uTileCount_y);

        // Create the sample state
        D3D11_SAMPLER_DESC sampDesc;
        ZeroMemory( &sampDesc, sizeof( sampDesc ) );
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
        V_RETURN( pd3dDevice->CreateSamplerState( &sampDesc, &m_pSS ) );
        DXUT_SetDebugName( m_pSS, "m_pSS" );

        m_uTexCount = m_vecTiledObjs.size();

        m_ppInputSRVs = new ID3D11ShaderResourceView*[m_uTexCount];
        m_ppNullSRVs = new ID3D11ShaderResourceView*[m_uTexCount];

        for(int i=0;i<m_uTexCount;i++)
            m_ppNullSRVs[i] = NULL;

        return hr;
    }

    void SetupPipeline( ID3D11DeviceContext* pd3dImmediateContext )
    {
        pd3dImmediateContext->OMSetRenderTargets( 1, &m_pOutRTV, NULL );
        pd3dImmediateContext->IASetInputLayout( m_pPassIL );
        pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
        UINT stride = 0;
        UINT offset = 0;
        pd3dImmediateContext->IASetVertexBuffers( 0, 1, &m_pPassVB, &stride, &offset );
        pd3dImmediateContext->VSSetShader( m_pPassVS, NULL, 0 );
        pd3dImmediateContext->GSSetShader( m_pTiledQuadGS, NULL, 0 );
        pd3dImmediateContext->PSSetShader( m_pTexPS, NULL, 0 );

        int idx = 0;
        for(auto item : m_vecTiledObjs)
            m_ppInputSRVs[idx++] = item.ppInputSRV ? *item.ppInputSRV : NULL;

        pd3dImmediateContext->PSSetShaderResources( 0, m_vecTiledObjs.size(), m_ppInputSRVs );
        pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pSS );
        pd3dImmediateContext->RSSetViewports( 1, &m_RTviewport );

        float ClearColor[4] = { 0.2f, 0.2f, 0.2f, 0.0f };
        pd3dImmediateContext->ClearRenderTargetView( m_pOutRTV, ClearColor );
    }

    void Resize( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
    {
        int winWidth = pBackBufferSurfaceDesc->Width;
        int winHeight = pBackBufferSurfaceDesc->Height;
        float winAspectRatio = (float)winWidth/(float)winHeight;

        if( winAspectRatio > m_fRTaspectRatio){
            m_RTviewport.Height = winHeight;
            m_RTviewport.Width = winHeight*m_fRTaspectRatio;
        }else{
            m_RTviewport.Width = winWidth;
            m_RTviewport.Height = winWidth / m_fRTaspectRatio;
        }

        m_RTviewport.TopLeftX = (winWidth - m_RTviewport.Width)/2.f;
        m_RTviewport.TopLeftY = (winHeight - m_RTviewport.Height)/2.f;
        if( m_bDirectToBackBuffer )
        {
            m_pOutRTV = DXUTGetD3D11RenderTargetView();
        }
    }

    void Update()
    {

    }

    void Render( ID3D11DeviceContext* pd3dImmediateContext )
    {
        this->SetupPipeline( pd3dImmediateContext );
        pd3dImmediateContext->Draw( m_uTexCount, 0 );
        pd3dImmediateContext->PSSetShaderResources( 0, m_uTexCount, m_ppNullSRVs );
    }

    void Release()
    {
        SAFE_RELEASE( m_pPassVS );
        SAFE_RELEASE( m_pTexPS );
        SAFE_RELEASE( m_pTiledQuadGS );
        SAFE_RELEASE( m_pPassIL );
        SAFE_RELEASE( m_pPassVB );
        SAFE_RELEASE( m_pSS );
        if( !m_bDirectToBackBuffer )
        {
            SAFE_RELEASE( m_pOutTex );
            SAFE_RELEASE( m_pOutRTV );
            SAFE_RELEASE( m_pOutSRV );
        }
        delete[] m_ppInputSRVs;
        delete[] m_ppNullSRVs;
    }

    LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
    {
        UNREFERENCED_PARAMETER( lParam );
        UNREFERENCED_PARAMETER( hWnd );

        return 0;
    }
};
