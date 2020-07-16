#include "d3dapp.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace {
HWND CreateD3DWindow(HINSTANCE instance, const TCHAR* title, int width,
                     int height, DWORD style, DWORD style_ex,
                     WNDPROC window_proc, d3dapp::D3DApp* app) {
  const TCHAR class_name[] = TEXT("d3d12 app class");

  WNDCLASSEX window_class = {};
  window_class.cbSize = sizeof(window_class);
  window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  window_class.lpfnWndProc = window_proc;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hInstance = instance;
  window_class.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
  window_class.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
  window_class.lpszMenuName = nullptr;
  window_class.lpszClassName = class_name;
  window_class.hIconSm = nullptr;
  RegisterClassEx(&window_class);

  RECT rect = {0, 0, width, height};
  AdjustWindowRectEx(&rect, style, false, style_ex);

  HWND window = CreateWindowEx(style_ex, class_name, title, style | WS_VISIBLE,
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               rect.right - rect.left, rect.bottom - rect.top,
                               nullptr, nullptr, instance, nullptr);
  SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
  UpdateWindow(window);
  return window;
}

bool CreateAdapter(IDXGIFactory4* dxgi_factory,
                   ComPtr<IDXGIAdapter1>& adapter) {
  for (int i = 0;; ++i) {
    adapter.Reset();
    if (DXGI_ERROR_NOT_FOUND != dxgi_factory->EnumAdapters1(i, &adapter)) {
      DXGI_ADAPTER_DESC1 desc{};
      adapter->GetDesc1(&desc);

      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue;
      }

      if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                      _uuidof(ID3D12Device), nullptr))) {
        return true;
      }
    }
  }
  return false;
}

bool CreateSwapChain(ID3D12Device* device, IDXGIFactory2* dxgi_factory,
                     ID3D12CommandQueue* command_queue, HWND window,
                     unsigned width, unsigned height,
                     ComPtr<IDXGISwapChain1>& swap_chain,
                     ComPtr<ID3D12Resource>* render_target,
                     int render_target_count,
                     ComPtr<ID3D12DescriptorHeap>& descriptor) {
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = width;
  desc.Height = height;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.Stereo = false;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 2;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
  desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

  dxgi_factory->CreateSwapChainForHwnd(command_queue, window, &desc, nullptr,
                                       nullptr, &swap_chain);

  D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
  rtv_heap_desc.NumDescriptors = render_target_count;
  rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  rtv_heap_desc.NodeMask = 0;
  device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&descriptor));

  CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor_handle{
      descriptor->GetCPUDescriptorHandleForHeapStart()};
  for (int i = 0; i < render_target_count; i++) {
    swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_target[i]));
    device->CreateRenderTargetView(render_target[i].Get(), nullptr,
                                   descriptor_handle);
    descriptor_handle.Offset(1, device->GetDescriptorHandleIncrementSize(
                                    D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
  }

  return true;
}

bool CreateDepthStencilBuffer(ID3D12Device* device, int width, int height,
                              ComPtr<ID3D12Resource>& depth_stencil_buffer,
                              ComPtr<ID3D12DescriptorHeap>& descriptor) {
  D3D12_RESOURCE_DESC depth_stencil_desc{};
  depth_stencil_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  depth_stencil_desc.Alignment = 0;
  depth_stencil_desc.Width = width;
  depth_stencil_desc.Height = width;
  depth_stencil_desc.DepthOrArraySize = 1;
  depth_stencil_desc.MipLevels = 1;
  depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  depth_stencil_desc.SampleDesc.Count = 1;
  depth_stencil_desc.SampleDesc.Quality = 0;
  depth_stencil_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  depth_stencil_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE clear_value{};
  clear_value.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  clear_value.DepthStencil.Depth = 1.0f;
  clear_value.DepthStencil.Stencil = 0;
  device->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &depth_stencil_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value,
      IID_PPV_ARGS(depth_stencil_buffer.GetAddressOf()));

  D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc{};
  dsv_heap_desc.NumDescriptors = 1;
  dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dsv_heap_desc.NodeMask = 0;
  device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&descriptor));

  D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
  dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
  dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  dsv_desc.Texture2D.MipSlice = 0;
  device->CreateDepthStencilView(
      depth_stencil_buffer.Get(), &dsv_desc,
      descriptor->GetCPUDescriptorHandleForHeapStart());

  return true;
}

bool CreateD3DResources(
    int frame_count, ID3D12Device* device, ComPtr<ID3D12Fence>& fence,
    ComPtr<ID3D12CommandQueue>& command_queue,
    ComPtr<ID3D12GraphicsCommandList>& command_list,
    std::vector<ComPtr<ID3D12CommandAllocator>>& allocators) {
  device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

  D3D12_COMMAND_QUEUE_DESC queue_desc{D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
                                      D3D12_COMMAND_QUEUE_FLAG_NONE, 0};
  device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue));

  ComPtr<ID3D12CommandAllocator> command_allocator;
  device->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT,
      IID_PPV_ARGS(&command_allocator));

  device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                            command_allocator.Get(), nullptr,
                            IID_PPV_ARGS(&command_list));
  command_list->Close();

  for (int i = 0; i < frame_count; ++i) {
    ComPtr<ID3D12CommandAllocator> allocator;
    device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&allocator));
    allocators.push_back(allocator);
  }

  return true;
}

}  // namespace

namespace d3dapp {
LRESULT Render::OnMessage(HWND hwnd, UINT message, WPARAM wParam,
                          LPARAM lParam) {
  return DefWindowProc(hwnd, message, wParam, lParam);
}
void Render::OnCreate(ID3D12Device* device, void* data) {}
void Render::OnRender(int frame_index,
                      ID3D12GraphicsCommandList* command_list) {}
Render::~Render() {}

/////////////////////////////////////////////////////////////////////////////
thread_local std::shared_ptr<D3DApp> D3DApp::tlsAppInstance{nullptr,
                                                            D3DApp::Destroy};

std::shared_ptr<D3DApp> D3DApp::Create(const D3DApp::Desc& desc) {
  UINT dxgi_flags = 0;
#if defined(DEBUG) || defined(_DEBUG)
  ComPtr<ID3D12Debug> debug;
  D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
  debug->EnableDebugLayer();
  dxgi_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  // dxgi factory and device
  ComPtr<IDXGIFactory4> dxgi_factory;
  CreateDXGIFactory2(dxgi_flags, IID_PPV_ARGS(&dxgi_factory));

  ComPtr<IDXGIAdapter1> adapter;
  CreateAdapter(dxgi_factory.Get(), adapter);

  ComPtr<ID3D12Device> device;
  D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                    IID_PPV_ARGS(&device));

  // d3d resources
  ComPtr<ID3D12Fence> fence;
  ComPtr<ID3D12CommandQueue> command_queue;
  ComPtr<ID3D12GraphicsCommandList> command_list;
  std::vector<ComPtr<ID3D12CommandAllocator>> allocators;
  CreateD3DResources(desc.frame_count, device.Get(), fence, command_queue,
                     command_list, allocators);

  std::shared_ptr<D3DApp> app{new D3DApp{}, Destroy};
  tlsAppInstance = app;
  app->render_ = desc.render;
  HWND window = CreateD3DWindow(desc.instance, desc.title, desc.width,
                                desc.height, desc.window_style,
                                desc.window_style_ex, WindowProc, app.get());

  // swap chain
  ComPtr<IDXGISwapChain1> swap_chain;
  ComPtr<ID3D12Resource> render_target[kRenderTargetCount];
  ComPtr<ID3D12DescriptorHeap> rtv_descriptor;
  CreateSwapChain(device.Get(), dxgi_factory.Get(), command_queue.Get(), window,
                  desc.width, desc.height, swap_chain, render_target,
                  kRenderTargetCount, rtv_descriptor);

  // depth stencil
  ComPtr<ID3D12Resource> depth_stencil_buffer;
  ComPtr<ID3D12DescriptorHeap> dsv_desctriptor;
  CreateDepthStencilBuffer(device.Get(), desc.width, desc.height,
                           depth_stencil_buffer, dsv_desctriptor);

  ///////////////////////////////////////////////////////////////
  app->device_ = device;
  app->fence_ = fence;
  app->command_queue_ = command_queue;
  app->command_list_ = command_list;

  for (auto& i : allocators) {
    app->frame_resources_.emplace_back();
    app->frame_resources_.back().command_allocator = i;
  }

  app->swap_chain_ = swap_chain;
  app->rtv_descriptor_ = rtv_descriptor;
  for (int i = 0; i < kRenderTargetCount; ++i) {
    app->render_target_[i] = render_target[i];
  }

  app->depth_stencil_ = depth_stencil_buffer;
  app->dsv_descriptor_ = dsv_desctriptor;

  app->rtv_descriptor_size_ =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  app->dsv_descriptor_size_ =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  app->cbv_srv_uav_descripter_size_ = device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  app->viewport_.TopLeftX = 0.0f;
  app->viewport_.TopLeftY = 0.0f;
  app->viewport_.Width = static_cast<FLOAT>(desc.width);
  app->viewport_.Height = static_cast<FLOAT>(desc.height);
  app->viewport_.MinDepth = 0.0f;
  app->viewport_.MaxDepth = 1.0f;

  app->scissor_rect_.left = 0;
  app->scissor_rect_.top = 0;
  app->scissor_rect_.right = desc.width;
  app->scissor_rect_.bottom = desc.height;

  app->clear_color_ = desc.clear_color;
  app->frame_count_ = desc.frame_count;
  desc.render->OnCreate(device.Get(), desc.data);

  return app;
}

void D3DApp::Destroy(D3DApp* app) { delete app; }

void D3DApp::Run() {
  MSG message{};
  while (WM_QUIT != message.message) {
    if (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessage(&message);
    } else {
      RenderFrame();
    }
  }
}

D3DApp::D3DApp() {
  event_handle_ = CreateEvent(nullptr, false, false, nullptr);
}

D3DApp::~D3DApp() {
  WaitForGPU();
  CloseHandle(event_handle_);
}

LRESULT CALLBACK D3DApp::WindowProc(HWND hwnd, UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  return D3DApp::tlsAppInstance->OnMessage(hwnd, message, wParam, lParam);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentRenderTargetDescriptor() {
  return CD3DX12_CPU_DESCRIPTOR_HANDLE(
      rtv_descriptor_->GetCPUDescriptorHandleForHeapStart(), back_buffer_index_,
      rtv_descriptor_size_);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentDepthStencilDescriptor() {
  return dsv_descriptor_->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::WaitForGPU() {
  if (fence_->GetCompletedValue() < current_fence_value_) {
    fence_->SetEventOnCompletion(current_fence_value_, event_handle_);
    WaitForSingleObject(event_handle_, INFINITE);
  }
}

void D3DApp::RenderFrame() {
  FrameResource* current_frame = &frame_resources_[frame_index_];
  ID3D12Resource* back_buffer = render_target_[back_buffer_index_].Get();

  if (fence_->GetCompletedValue() < current_frame->fence_value) {
    fence_->SetEventOnCompletion(current_frame->fence_value, event_handle_);
    WaitForSingleObject(event_handle_, INFINITE);
  }

  current_frame->command_allocator->Reset();
  command_list_->Reset(current_frame->command_allocator.Get(), nullptr);
  command_list_->ResourceBarrier(1,
                                 &CD3DX12_RESOURCE_BARRIER::Transition(
                                     back_buffer, D3D12_RESOURCE_STATE_PRESENT,
                                     D3D12_RESOURCE_STATE_RENDER_TARGET));

  command_list_->RSSetViewports(1, &viewport_);
  command_list_->RSSetScissorRects(1, &scissor_rect_);

  command_list_->ClearRenderTargetView(CurrentRenderTargetDescriptor(),
                                       clear_color_, 0, nullptr);
  command_list_->ClearDepthStencilView(
      CurrentDepthStencilDescriptor(),
      D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

  command_list_->OMSetRenderTargets(1, &CurrentRenderTargetDescriptor(), true,
                                    &CurrentDepthStencilDescriptor());

  render_->OnRender(frame_index_, command_list_.Get());

  command_list_->ResourceBarrier(
      1, &CD3DX12_RESOURCE_BARRIER::Transition(
             back_buffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
             D3D12_RESOURCE_STATE_PRESENT));

  command_list_->Close();

  ID3D12CommandList* command_lists[]{command_list_.Get()};
  command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);

  swap_chain_->Present(0, 0);

  ++current_fence_value_;
  command_queue_->Signal(fence_.Get(), current_fence_value_);
  current_frame->fence_value = current_fence_value_;

  frame_index_ = (frame_index_ + 1) % frame_count_;
  back_buffer_index_ = (back_buffer_index_ + 1) % kRenderTargetCount;
}

LRESULT D3DApp::OnMessage(HWND hwnd, UINT message, WPARAM wParam,
                          LPARAM lParam) {
  switch (message) {
    case WM_LBUTTONDOWN: {
      PostMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
      break;
    }
    case WM_KEYUP: {
      if (VK_ESCAPE == wParam) {
        DestroyWindow(hwnd);
      }
      break;
    }
    case WM_DESTROY: {
      WaitForGPU();
      PostQuitMessage(0);
      break;
    }
  }

  return render_->OnMessage(hwnd, message, wParam, lParam);
}

}  // namespace d3dapp
