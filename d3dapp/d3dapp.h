#pragma once

#ifndef __D3DAPP_H__
#define __D3DAPP_H__

#include <DirectXColors.h>
#include <d3dx12.h>
#include <dxgi1_4.h>
#include <memory>
#include <vector>
#include "framework.h"

namespace d3dapp {
class Render {
 public:
  virtual void OnCreate(ID3D12Device* device, void* data) = 0;
  virtual void OnRender(int frame_index,
                        ID3D12GraphicsCommandList* command_list) = 0;
};

class D3DApp {
 public:
  struct Desc {
    const TCHAR* title;
    HINSTANCE instance;
    int width;
    int height;
    DWORD window_style;
    DWORD window_style_ex;
    int frame_count;
    DirectX::XMVECTORF32 clear_color;
    Render* render;
    void* data;
  };
  using Deleter = void (*)(D3DApp*);
  static std::unique_ptr<D3DApp, Deleter> Create(const Desc& desc);
  static void Destroy(D3DApp* app);

  void Run();

 private:
  struct FrameResource {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator;
    UINT64 fence_value{0};
  };

  static constexpr int kRenderTargetCount = 2;

  D3DApp();
  D3DApp(const D3DApp&) = delete;
  D3DApp& operator=(const D3DApp&) = delete;
  virtual ~D3DApp();

  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam,
                                     LPARAM lParam);

  D3D12_CPU_DESCRIPTOR_HANDLE CurrentRenderTargetDescriptor();
  D3D12_CPU_DESCRIPTOR_HANDLE CurrentDepthStencilDescriptor();

  void WaitForGPU();
  void RenderFrame();

  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list_;
  std::vector<FrameResource> frame_resources_;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  Microsoft::WRL::ComPtr<ID3D12Resource> render_target_[kRenderTargetCount];
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_descriptor_;

  Microsoft::WRL::ComPtr<ID3D12Resource> depth_stencil_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsv_descriptor_;

  HANDLE event_handle_{CreateEvent(nullptr, false, false, nullptr)};
  Render* render_{nullptr};
  DirectX::XMVECTORF32 clear_color_{DirectX::Colors::LightCyan};

  UINT rtv_descriptor_size_{0};
  UINT dsv_descriptor_size_{0};
  UINT cbv_srv_uav_descripter_size_{0};

  D3D12_VIEWPORT viewport_{};
  D3D12_RECT scissor_rect_{};

  UINT64 current_fence_value_{0};
  int frame_count_{0};
  int frame_index_{0};
  int back_buffer_index_{0};
};

}  // namespace d3dapp

#endif  // !__D3DAPP_H__
