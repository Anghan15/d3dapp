#include "../d3dapp/D3DApp.h"

class TerrainRender : public d3dapp::Render {
  virtual void OnCreate(ID3D12Device* device, void* data) override {}
  virtual void OnRender(int frame_index,
                        ID3D12GraphicsCommandList* command_list) override {}
};

int WINAPI _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE,
                     _In_ LPWSTR lpCmdLine, _In_ int nShowCmd) {
  TerrainRender render;
  d3dapp::D3DApp::Desc desc{};
  desc.instance = hInstance;
  desc.title = TEXT("CDLOD sample");
  desc.width = 1280;
  desc.height = 800;
  desc.window_style = WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
  desc.window_style_ex = 0;
  desc.frame_count = 3;
  desc.clear_color = DirectX::Colors::DarkGray;
  desc.render = &render;
  desc.data = nullptr;

  auto app = d3dapp::D3DApp::Create(desc);
  app->Run();
  return 0;
}
