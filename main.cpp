#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <atlbase.h>
#include <atlcore.h>
#include <atltypes.h>
#include <atlwin.h>

#include <comdef.h>

#include <d3d11.h>
#include <d3dcompiler.h>

#include <memory>
#include <array>
#include <string>

#define CHECK(hr) do { if (HRESULT _hr = (hr); FAILED(_hr)) { throw _com_error(_hr); } } while(false);

struct SwapChain {
    SwapChain(const CComPtr<IDXGISwapChain>& chain, const CComPtr<ID3D11RenderTargetView>& renderTargetView)
    : _swapChain(chain)
    , _renderTargetView(renderTargetView)
    {
    }

    void Present(UINT syncInterval) {
        CHECK(_swapChain->Present(syncInterval, 0));
    }

    ID3D11RenderTargetView& GetRenderTargetView() const
    {
        return *_renderTargetView;
    }

    CComPtr<IDXGISwapChain> _swapChain;
    CComPtr<ID3D11RenderTargetView> _renderTargetView;

};

template<typename TShader>
struct Shader{
    CComPtr<ID3DBlob> _code;
    CComPtr<TShader> _compiledShader;
};

struct Target{
    Target() = delete;

    inline Target(const char* s)
    : _str(s)
    {}

    operator const char* () const {
        return _str;
    }

    static const Target Pixel;
    static const Target Vertex;

private:
    const char* _str;
};
const Target Target::Pixel = {"ps_5_0"};
const Target Target::Vertex = {"vs_5_0"};


CComPtr<ID3DBlob>
CompileFile(const std::wstring& filename, const std::string& functionName, const Target& target)
{
    CComPtr<ID3DBlob> code;
    CComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3DCompileFromFile(filename.c_str(), nullptr, nullptr,
            functionName.c_str(), target, 
            D3DCOMPILE_DEBUG|D3DCOMPILE_WARNINGS_ARE_ERRORS, 
            0u,
            &code, &errBlob);
    if (FAILED(hr)) {
        if (errBlob && errBlob->GetBufferSize() > 0)
            throw std::logic_error((char*)errBlob->GetBufferPointer());
        else
            throw _com_error(hr);
    }
    return code;
}

struct VertexBuffer{
    VertexBuffer(const CComPtr<ID3D11Buffer>& buffer, const CComPtr<ID3D11InputLayout>& layout, size_t itemSize)
    : _buffer(buffer), _layout(layout), _itemSize(itemSize)
    {
    }

    size_t GetItemSize() const {
        return _itemSize;
    }

    ID3D11Buffer& GetBuffer() const {
        return *_buffer;
    }
    ID3D11InputLayout& GetLayout() const {
        return *_layout;
    }
private:
    CComPtr<ID3D11Buffer> _buffer;
    CComPtr<ID3D11InputLayout> _layout;
    size_t _itemSize;
};

struct DeviceContext{
    DeviceContext(const CComPtr<ID3D11DeviceContext>& context)
    : _context(context)
    {}

    void SetViewPort(float width, float height)
    {
        D3D11_VIEWPORT vp{ 0, 0, width, height, 0.0f, 1.0f};

        _context->RSSetViewports(1, &vp);
    }

    void Clear(ID3D11RenderTargetView& renderTarget, float red, float green, float blue, float alpha)
    {
        float color[]{red, green, blue, alpha};
        _context->ClearRenderTargetView(&renderTarget, color);
    }

    void SetRenderTarget(ID3D11RenderTargetView& renderTarget)
    {
        ID3D11RenderTargetView* targets[] = { &renderTarget };
        _context->OMSetRenderTargets(std::size(targets),  targets, nullptr);
    }

    void BindBuffer(const VertexBuffer& vb)
    {
        UINT strides[] { vb.GetItemSize() };
        UINT offsets[] { 0 };
        ID3D11Buffer * buffers[]{ &vb.GetBuffer() };
        _context->IASetVertexBuffers(0, std::size(buffers), buffers, strides, offsets);
        _context->IASetInputLayout(&vb.GetLayout());
    }

    void SetShaders(const Shader<ID3D11VertexShader>& vs, const Shader<ID3D11PixelShader>& ps)
    {
        _context->VSSetShader(vs._compiledShader, nullptr, 0);
        _context->PSSetShader(ps._compiledShader, nullptr, 0);
    }

    void DrawTriangleList(UINT vertexCount, UINT startIndex) {
        _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        _context->Draw(vertexCount, startIndex);
    }

private:
    CComPtr<ID3D11DeviceContext> _context;
};

struct DxEngine{
    DxEngine(){
        std::array<D3D_DRIVER_TYPE, 3> driverTypes {
            D3D_DRIVER_TYPE_HARDWARE, 
            D3D_DRIVER_TYPE_WARP, 
            D3D_DRIVER_TYPE_REFERENCE};

        D3D_FEATURE_LEVEL featureLevels[] { D3D_FEATURE_LEVEL_11_0 };
        HRESULT hr;
        for(auto driverType : driverTypes){
            D3D_FEATURE_LEVEL featureLevel;
            hr = D3D11CreateDevice(nullptr, driverType, nullptr, D3D11_CREATE_DEVICE_DEBUG,
                featureLevels, (UINT)std::size(featureLevels),
                D3D11_SDK_VERSION,
                &device, &featureLevel, &deviceContext);
            if(SUCCEEDED(hr)) {
                break;
            }
        }
        CHECK(hr);
    }
                                                          
    std::unique_ptr<SwapChain> CreateSwapChain(HWND hwnd, uint32_t w, uint32_t h) const{
        CComPtr<IDXGIDevice> dxgi_device;
        CHECK(device->QueryInterface(IID_PPV_ARGS(&dxgi_device)));

        CComPtr<IDXGIAdapter> dxgi_adapter;
        CHECK(dxgi_device->GetParent(IID_PPV_ARGS(&dxgi_adapter)));

        CComPtr<IDXGIFactory1> dxgi_factory;
        CHECK(dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)));

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
            2, // BufferCount
            hwnd,   // OutputWindow
            true,   // Windowed
            DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,   // SwapEffect
            0                           // Flags
        };
        CComPtr<IDXGISwapChain> swapChain;
        CHECK(dxgi_factory->CreateSwapChain(device, &desc, &swapChain));

        CComPtr<ID3D11Texture2D> buffer;
        CHECK(swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)));

        CComPtr<ID3D11RenderTargetView> renderTargetView;
        CHECK(device->CreateRenderTargetView(buffer, nullptr, &renderTargetView));

        return std::make_unique<SwapChain>(swapChain, renderTargetView);
    }

    Shader<ID3D11VertexShader> CreateVertexShader(const std::wstring& file, const std::string& functionName) const
    {
        auto blob = CompileFile(file, functionName, Target::Vertex);
        CComPtr<ID3D11VertexShader> shader;
        CHECK(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), 
            nullptr, &shader));
        return { blob, shader };
    }

    Shader<ID3D11PixelShader> CreatePixelShader(const std::wstring& file, const std::string& functionName) const
    {
        auto blob = CompileFile(file, functionName, Target::Pixel);
        CComPtr<ID3D11PixelShader> shader;
        CHECK(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), 
            nullptr, &shader));
        return { blob, shader };
    }

    template<typename InputIt>
    CComPtr<ID3D11Buffer> LoadVertices(InputIt first, InputIt last) const {
        D3D11_BUFFER_DESC desc{
            (UINT)(std::distance(first, last) * sizeof(*first)), // ByteWidth
            D3D11_USAGE_DEFAULT,
            D3D11_BIND_VERTEX_BUFFER,
            0,  // CPU Access
            0,  // Misc Flags
            sizeof(*first),  // StructureByteStride
        };

        D3D11_SUBRESOURCE_DATA initData {
            &*first,
            0
        };
        CComPtr<ID3D11Buffer> buffer;
        CHECK(device->CreateBuffer(&desc, &initData, &buffer));
        return buffer;
    }

    CComPtr<ID3D11InputLayout> CreateInputLayout(const Shader<ID3D11VertexShader>& shader) const {
        D3D11_INPUT_ELEMENT_DESC inputDescriptors[]
        {
            //SEMANTIC NAME - SEMANTIC INDEX - FORMAT - INPUT SLOT - ALIGNED BYTE OFFSET - INPUT SLOT CLASS - INSTANCE DATA STEP RATE
            { "SV_POSITION", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        CComPtr<ID3D11InputLayout> layout;
        auto code = shader._code->GetBufferPointer();
        auto codeSize = shader._code->GetBufferSize();
        CHECK(device->CreateInputLayout(inputDescriptors, std::size(inputDescriptors), 
            code, codeSize, &layout));
        return layout;
    }

    DeviceContext GetDeviceContext() const {    
        return {deviceContext};
    }

    CComPtr<ID3D11DeviceContext> deviceContext;
    CComPtr<ID3D11Device> device;
};

struct vec3
{
    float x, y, z;
};

struct DxWindow : public CWindowImpl<DxWindow> {
    DECLARE_WND_CLASS(_T("DxWindow"))

    BEGIN_MSG_MAP(DxWindow)
    try {
        MESSAGE_HANDLER(WM_CREATE, OnCreate);
        MESSAGE_HANDLER(WM_CLOSE, OnClose);
    } catch(_com_error& e){
        ::MessageBox(nullptr, e.ErrorMessage(), _T("ERROR"), MB_OK | MB_ICONHAND);
        PostQuitMessage(-1);
    }
    END_MSG_MAP()

    LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled )
    { 
        _engine = std::make_unique<DxEngine>();
        CRect rc;
        GetClientRect(&rc);
        _swapChain = _engine->CreateSwapChain(*this, rc.Width(), rc.Height());

        auto shaderPath = L"shader.fx";
        _vertexShader = _engine->CreateVertexShader(shaderPath, "vsmain");
        _pixelShader = _engine->CreatePixelShader(shaderPath, "psmain");

        vec3 vertices[] = 
        {
            //X - Y - Z
            {-0.5f,-0.5f, 0.0f }, // POS1
            { 0.0f, 0.5f, 0.0f }, // POS2
            { 0.5f,-0.5f, 0.0f },
        };
        auto bufferedVertices = _engine->LoadVertices(std::begin(vertices), std::end(vertices));
        auto layout = _engine->CreateInputLayout(_vertexShader);
        _vertices = std::make_unique<VertexBuffer>(bufferedVertices, layout, sizeof(vec3));
        return 0;
    }

    LRESULT OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled ) { 
        _engine.reset(nullptr);
        PostQuitMessage(0);
        bHandled = false; 
        return 0;
    }

    void Present(){
        auto deviceContext = _engine->GetDeviceContext();
        auto& renderTarget = _swapChain->GetRenderTargetView();
        deviceContext.Clear(renderTarget, 0.f, 1.f, 0.f, 1.f);

        CRect r;
        GetClientRect(&r);
        deviceContext.SetViewPort(r.Width(), r.Height());

        deviceContext.SetRenderTarget(renderTarget);

        deviceContext.SetShaders(_vertexShader, _pixelShader);
        deviceContext.BindBuffer(*_vertices);
        deviceContext.DrawTriangleList(3, 0);

        _swapChain->Present(1);
    }

    std::unique_ptr<DxEngine> _engine;
    std::unique_ptr<SwapChain> _swapChain;
    std::unique_ptr<VertexBuffer> _vertices;
    Shader<ID3D11VertexShader> _vertexShader;
    Shader<ID3D11PixelShader> _pixelShader;
};                                                                    

CAtlWinModule module;

int main(){
    try {
        DxWindow win;
        win.Create(nullptr, CWindow::rcDefault, _T("Dx"), WS_OVERLAPPEDWINDOW);
        win.ShowWindow(SW_SHOW);
        win.UpdateWindow();

        while(true){
            MSG msg;
            while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && msg.message != WM_QUIT){
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (msg.message == WM_QUIT) break;

            win.Present();
        }

        return 0;
    } catch(_com_error& e){
        ::MessageBox(nullptr, e.ErrorMessage(), _T("ERROR"), MB_OK | MB_ICONHAND);
        return -1;
    }
    catch(std::exception& e){
        ::MessageBox(nullptr, e.what(), _T("ERROR"), MB_OK | MB_ICONHAND);        
    }
    catch(...){
        ::MessageBox(nullptr, _T("Something went wrong"), _T("ERROR"), MB_OK | MB_ICONHAND);
    }
}