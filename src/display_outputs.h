#pragma once

// One clean frame is sampled by all output windows. No extra scene simulation.
struct StageDisplay { RECT bounds{}; std::wstring name; };
inline std::vector<StageDisplay> EnumerateStageDisplays(){
    std::vector<StageDisplay> result;
    EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR monitor,HDC,LPRECT,LPARAM data)->BOOL{
        MONITORINFOEXW info{};info.cbSize=sizeof(info);
        if(GetMonitorInfoW(monitor,&info))reinterpret_cast<std::vector<StageDisplay>*>(data)->push_back({info.rcMonitor,info.szDevice});
        return TRUE;
    },reinterpret_cast<LPARAM>(&result));
    std::sort(result.begin(),result.end(),[](const auto&a,const auto&b){return a.name<b.name;});
    if(result.size()>8)result.resize(8);
    return result;
}
inline RECT FitStageFrame(UINT sourceWidth,UINT sourceHeight,UINT width,UINT height){
    const double scale=std::min(double(width)/std::max(1u,sourceWidth),double(height)/std::max(1u,sourceHeight));
    const LONG w=LONG(std::round(sourceWidth*scale)),h=LONG(std::round(sourceHeight*scale));
    return {LONG(width-w)/2,LONG(height-h)/2,LONG(width-w)/2+w,LONG(height-h)/2+h};
}
inline bool FullscreenKey(UINT m,WPARAM k,LPARAM f){return rarestage::control::IsFullscreenShortcut(m,k,f);}
inline constexpr UINT CloseMirrorMessage=WM_APP+0x453;
class DisplayOutputs {
    struct Target {HWND window{};RECT bounds{};ComPtr<IDXGISwapChain> swap;ComPtr<ID3D11RenderTargetView> view;};
    std::vector<Target> targets_;
    ComPtr<ID3D11Device> device_;ComPtr<ID3D11DeviceContext> context_;ComPtr<IDXGIFactory> factory_;
    ComPtr<ID3D11VertexShader> vertex_;ComPtr<ID3D11PixelShader> pixel_;ComPtr<ID3D11SamplerState> sampler_;
    ComPtr<ID3D11Texture2D> clean_;ComPtr<ID3D11ShaderResourceView> cleanView_;
    UINT width_{},height_{};HWND owner_{};bool hidden_{};
    static LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM key,LPARAM flags){
        HWND owner=reinterpret_cast<HWND>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE){owner=static_cast<HWND>(reinterpret_cast<CREATESTRUCTW*>(flags)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(owner));}
        if(message==WM_CLOSE||(message==WM_KEYDOWN&&key==VK_ESCAPE)){PostMessageW(owner,CloseMirrorMessage,0,0);return 0;}
        if(FullscreenKey(message,key,flags)){PostMessageW(owner,message,key,flags);return 0;}
        if(message==WM_SYSCHAR&&key==VK_RETURN)return 0;
        if(message==WM_ERASEBKGND)return 1;
        return DefWindowProcW(window,message,key,flags);
    }
public:
    ~DisplayOutputs(){clear();}
    void clear(){if(context_)context_->OMSetRenderTargets(0,nullptr,nullptr);for(auto&target:targets_){target.view.Reset();target.swap.Reset();if(target.window)DestroyWindow(target.window);}targets_.clear();cleanView_.Reset();clean_.Reset();width_=height_=0;}
    size_t count()const{return targets_.size();}
    bool initialize(ID3D11Device*device,HWND owner,bool hidden){
        device_=device;device_->GetImmediateContext(&context_);owner_=owner;hidden_=hidden;
        ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;
        if(FAILED(device_->QueryInterface(IID_PPV_ARGS(&dxgi)))||FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetParent(IID_PPV_ARGS(&factory_))))return false;
        if(FAILED(factory_->MakeWindowAssociation(owner_,DXGI_MWA_NO_WINDOW_CHANGES|DXGI_MWA_NO_ALT_ENTER)))return false;
        const char*shader=R"(
Texture2D cleanFrame:register(t0);SamplerState linearSampler:register(s0);
struct O{float4 position:SV_POSITION;float2 uv:TEXCOORD;};
O vs(uint id:SV_VertexID){O o;o.uv=float2((id<<1)&2,id&2);o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float4 ps(O i):SV_TARGET{return cleanFrame.Sample(linearSampler,i.uv);}
)";
        ComPtr<ID3DBlob> vs,ps,error;
        if(FAILED(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&error))||FAILED(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&error)))return false;
        if(FAILED(device_->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex_))||FAILED(device_->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel_)))return false;
        D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=D3D11_FLOAT32_MAX;
        return SUCCEEDED(device_->CreateSamplerState(&sampler,&sampler_));
    }
    bool configure(const std::vector<StageDisplay>&displays,unsigned mask,int mainDisplay){
        clear();if(!device_||!factory_)return false;
        WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=WindowProc;wc.lpszClassName=L"RareStageCleanDisplay";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
        for(size_t index=0;index<displays.size();++index){
            if(!(mask&(1u<<index))||int(index)==mainDisplay)continue;
            Target target;target.bounds=displays[index].bounds;const RECT r=target.bounds;
            target.window=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"RareStage Clean Output",WS_POPUP,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,wc.hInstance,owner_);
            if(!target.window){clear();return false;}
            DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=r.right-r.left;desc.BufferDesc.Height=r.bottom-r.top;desc.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.OutputWindow=target.window;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
            ComPtr<ID3D11Texture2D> buffer;
            if(FAILED(factory_->CreateSwapChain(device_.Get(),&desc,&target.swap))||FAILED(target.swap->GetBuffer(0,IID_PPV_ARGS(&buffer)))||FAILED(device_->CreateRenderTargetView(buffer.Get(),nullptr,&target.view))){DestroyWindow(target.window);clear();return false;}
            if(!hidden_)ShowWindow(target.window,SW_SHOWNOACTIVATE);
            targets_.push_back(std::move(target));
        }
        factory_->MakeWindowAssociation(owner_,DXGI_MWA_NO_WINDOW_CHANGES|DXGI_MWA_NO_ALT_ENTER);
        return true;
    }
    bool present(ID3D11Texture2D*frame,bool verify=false){
        if(targets_.empty())return true;
        D3D11_TEXTURE2D_DESC desc{};frame->GetDesc(&desc);
        if(!clean_||desc.Width!=width_||desc.Height!=height_){
            cleanView_.Reset();clean_.Reset();desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;desc.MiscFlags=0;
            if(FAILED(device_->CreateTexture2D(&desc,nullptr,&clean_))||FAILED(device_->CreateShaderResourceView(clean_.Get(),nullptr,&cleanView_)))return false;
            width_=desc.Width;height_=desc.Height;
        }
        context_->OMSetRenderTargets(0,nullptr,nullptr);context_->CopyResource(clean_.Get(),frame);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context_->VSSetShader(vertex_.Get(),nullptr,0);context_->PSSetShader(pixel_.Get(),nullptr,0);context_->PSSetSamplers(0,1,sampler_.GetAddressOf());context_->PSSetShaderResources(0,1,cleanView_.GetAddressOf());
        auto readPixel=[&](ID3D11Texture2D*texture,UINT x,UINT y){
            std::array<BYTE,4> color{};D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
            ComPtr<ID3D11Texture2D> staging;if(FAILED(device_->CreateTexture2D(&d,nullptr,&staging)))return color;context_->CopyResource(staging.Get(),texture);D3D11_MAPPED_SUBRESOURCE map{};
            if(SUCCEEDED(context_->Map(staging.Get(),0,D3D11_MAP_READ,0,&map))){memcpy(color.data(),static_cast<const BYTE*>(map.pData)+size_t(y)*map.RowPitch+x*4,4);context_->Unmap(staging.Get(),0);}return color;
        };
        std::array<BYTE,4> expected{};if(verify)expected=readPixel(frame,width_/2,height_/2);
        bool ok=true;for(auto&target:targets_){
            const UINT w=target.bounds.right-target.bounds.left,h=target.bounds.bottom-target.bounds.top;const RECT fit=FitStageFrame(width_,height_,w,h);
            D3D11_VIEWPORT viewport{float(fit.left),float(fit.top),float(fit.right-fit.left),float(fit.bottom-fit.top),0,1};
            const float black[4]={0,0,0,1};context_->ClearRenderTargetView(target.view.Get(),black);context_->RSSetViewports(1,&viewport);context_->OMSetRenderTargets(1,target.view.GetAddressOf(),nullptr);context_->Draw(3,0);
            if(verify){ComPtr<ID3D11Texture2D> buffer;target.swap->GetBuffer(0,IID_PPV_ARGS(&buffer));const auto center=readPixel(buffer.Get(),w/2,h/2);for(int channel=0;channel<4;++channel)if(std::abs(int(center[channel])-int(expected[channel]))>2)ok=false;
                if(fit.left>0||fit.top>0){const auto corner=readPixel(buffer.Get(),0,0);if(corner[0]||corner[1]||corner[2])ok=false;}}
            ok=SUCCEEDED(target.swap->Present(0,0))&&ok;
        }
        ID3D11ShaderResourceView*empty=nullptr;context_->PSSetShaderResources(0,1,&empty);context_->OMSetRenderTargets(0,nullptr,nullptr);return ok;
    }
};
