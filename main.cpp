#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <atlbase.h>
#include <atlcore.h>
#include <atltypes.h>
#include <atlwin.h>

#include <comdef.h>

#include <d3d11.h>

#include <memory>
#include <array>
#include <string>

#define CHECK(hr) if(HRESULT _hr = (hr); FAILED(_hr)) throw _com_error(_hr)

struct SwapChain{
    SwapChain(CComPtr<IDXGISwapChain> chain)
    : _swapChain(chain)
    {}

    CComPtr<IDXGISwapChain> _swapChain;
};

struct DxEngine{
    DxEngine(){
        std::array<D3D_DRIVER_TYPE, 3> driverTypes {
            D3D_DRIVER_TYPE_HARDWARE, 
            D3D_DRIVER_TYPE_WARP, 
            D3D_DRIVER_TYPE_REFERENCE};

        std::array<D3D_FEATURE_LEVEL, 1> featureLevels { D3D_FEATURE_LEVEL_11_0 };
        HRESULT hr;
        for(auto driverType : driverTypes){
            D3D_FEATURE_LEVEL featureLevel;
            hr = D3D11CreateDevice(nullptr, driverType, nullptr, 0, 
                &featureLevels[0], featureLevels.size(),
                D3D11_SDK_VERSION,
                &device, &featureLevel, &deviceContext);
            if(SUCCEEDED(hr)) {
                break;
            }
        }
        CHECK(hr);
        CHECK(device->QueryInterface(IID_PPV_ARGS(&dxgi_device)));
        CHECK(dxgi_device->GetParent(IID_PPV_ARGS(&dxgi_adapter)));
        CHECK(dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)));
    }
                                                          
    std::unique_ptr<SwapChain> CreateSwapChain(HWND hwnd, uint32_t w, uint32_t h){
        DXGI_SWAP_CHAIN_DESC desc{
            DXGI_MODE_DESC{     // BufferDesc
                w, h,
                { 60, 1 },  // refresh rate
                DXGI_FORMAT_R8G8B8A8_UNORM,
                DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
                DXGI_MODE_SCALING_UNSPECIFIED,
            },
            DXGI_SAMPLE_DESC{   // SampleDesc
                1,              // Count
                0               // Quality
            },
            DXGI_USAGE_RENDER_TARGET_OUTPUT, // BufferUsage
            1, // BufferCount
            hwnd,
            true,
            DXGI_SWAP_EFFECT_DISCARD,
            0
        };
        CComPtr<IDXGISwapChain> swapChain;
        CHECK(dxgi_factory->CreateSwapChain(device, &desc, &swapChain));
        return std::make_unique<SwapChain>(swapChain);
    }

    CComPtr<ID3D11DeviceContext> deviceContext;
    CComPtr<ID3D11Device> device;
    CComPtr<IDXGIDevice> dxgi_device;
    CComPtr<IDXGIAdapter> dxgi_adapter;
    CComPtr<IDXGIFactory> dxgi_factory;

};

struct DxWindow: public CWindowImpl<DxWindow> {
    DECLARE_WND_CLASS(_T("DxWindow"))
  
    BEGIN_MSG_MAP(DxWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate);
        MESSAGE_HANDLER(WM_CLOSE, OnClose);
    END_MSG_MAP()

    LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled )
    { 
        _engine = std::make_unique<DxEngine>();
        CRect rc;
        GetClientRect(&rc);
        _swapChain = _engine->CreateSwapChain(*this, rc.Width(), rc.Height()); 
        return 0;
    }

    LRESULT OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled ) { 
        _engine.reset(nullptr);
        PostQuitMessage(0); 
        bHandled = false; 
        return 0;
    }

    std::unique_ptr<DxEngine> _engine;
    std::unique_ptr<SwapChain> _swapChain;
};                                                                    

CAtlWinModule module;

int main(){
    try {
        DxWindow win;
        win.Create(nullptr, CWindow::rcDefault, _T("Dx"), WS_OVERLAPPEDWINDOW);
        win.ShowWindow(SW_SHOW);
        win.UpdateWindow();

        MSG msg;
        while(GetMessage(&msg, nullptr, 0, 0)){
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        return 0;
    } catch(_com_error& e){
        ::MessageBox(nullptr, e.ErrorMessage(), _T("ERROR"), MB_OK | MB_ICONHAND);
        return -1;
    }
}