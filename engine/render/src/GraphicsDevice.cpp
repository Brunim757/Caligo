#include <windows.h>

#include <caligo/render/GraphicsDevice.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <format>
#include <iterator>
#include <string>
#include <wrl/client.h>

namespace caligo::render {

using Microsoft::WRL::ComPtr;

struct GraphicsDevice::Impl {
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<IDXGISwapChain1> swapChain;
  ComPtr<ID3D11RenderTargetView> renderTarget;
  HWND window = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
};

namespace {

std::string Failure(const char* operation, HRESULT result) {
  return std::format("{} failed with HRESULT {:#010x}", operation, static_cast<uint32_t>(result));
}

Result<ComPtr<ID3D11Device>> CreateHardwareOrWarp(ComPtr<ID3D11DeviceContext>& context) {
  constexpr D3D_FEATURE_LEVEL kLevels[] = {D3D_FEATURE_LEVEL_11_0};
  constexpr UINT kFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

  ComPtr<ID3D11Device> device;
  D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
  HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, kFlags, kLevels,
                                     static_cast<UINT>(std::size(kLevels)), D3D11_SDK_VERSION,
                                     device.GetAddressOf(), &featureLevel, context.GetAddressOf());
  if (FAILED(result)) {
    device.Reset();
    context.Reset();
    result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, kFlags, kLevels,
                               static_cast<UINT>(std::size(kLevels)), D3D11_SDK_VERSION,
                               device.GetAddressOf(), &featureLevel, context.GetAddressOf());
  }
  if (FAILED(result)) {
    return Error{Failure("D3D11CreateDevice", result), 1};
  }
  if (featureLevel < D3D_FEATURE_LEVEL_11_0) {
    return Error{"Direct3D 11.0 feature level is unavailable", 2};
  }
  return device;
}

}

GraphicsDevice::GraphicsDevice() : m_impl(std::make_unique<Impl>()) {}

GraphicsDevice::~GraphicsDevice() {
  Destroy();
}

Result<void> GraphicsDevice::Create(void* nativeWindow, uint32_t width, uint32_t height) {
  if (nativeWindow == nullptr) {
    return Error{"graphics device needs a window", 1};
  }
  if (width == 0 || height == 0) {
    return Error{"graphics device needs a positive size", 2};
  }
  if (IsValid()) {
    return Error{"graphics device is already created", 3};
  }

  Result<ComPtr<ID3D11Device>> created = CreateHardwareOrWarp(m_impl->context);
  if (!created) {
    return created.GetError();
  }
  m_impl->device = created.Value();

  ComPtr<IDXGIDevice> dxgiDevice;
  HRESULT result = m_impl->device.As(&dxgiDevice);
  if (FAILED(result)) {
    Destroy();
    return Error{Failure("ID3D11Device::QueryInterface", result), 4};
  }

  ComPtr<IDXGIAdapter> adapter;
  result = dxgiDevice->GetAdapter(adapter.GetAddressOf());
  if (FAILED(result)) {
    Destroy();
    return Error{Failure("IDXGIDevice::GetAdapter", result), 5};
  }

  ComPtr<IDXGIFactory2> factory;
  result = adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));
  if (FAILED(result)) {
    Destroy();
    return Error{Failure("IDXGIAdapter::GetParent", result), 6};
  }

  DXGI_SWAP_CHAIN_DESC1 description{};
  description.Width = width;
  description.Height = height;
  description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  description.Stereo = FALSE;
  description.SampleDesc.Count = 1;
  description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  description.BufferCount = 2;
  description.Scaling = DXGI_SCALING_STRETCH;
  description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

  result = factory->CreateSwapChainForHwnd(m_impl->device.Get(), static_cast<HWND>(nativeWindow),
                                           &description, nullptr, nullptr,
                                           m_impl->swapChain.GetAddressOf());
  if (FAILED(result)) {
    Destroy();
    return Error{Failure("IDXGIFactory2::CreateSwapChainForHwnd", result), 7};
  }

  factory->MakeWindowAssociation(static_cast<HWND>(nativeWindow), DXGI_MWA_NO_ALT_ENTER);

  m_impl->window = static_cast<HWND>(nativeWindow);
  m_impl->width = width;
  m_impl->height = height;

  const Result<void> target = CreateRenderTarget();
  if (!target) {
    Destroy();
    return target.GetError();
  }
  return {};
}

void GraphicsDevice::Destroy() noexcept {
  if (m_impl->context != nullptr) {
    m_impl->context->ClearState();
    m_impl->context->Flush();
  }
  m_impl->renderTarget.Reset();
  m_impl->swapChain.Reset();
  m_impl->context.Reset();
  m_impl->device.Reset();
  m_impl->window = nullptr;
  m_impl->width = 0;
  m_impl->height = 0;
}

Result<void> GraphicsDevice::Resize(uint32_t width, uint32_t height) {
  if (m_impl->swapChain == nullptr) {
    return Error{"graphics device is not created", 1};
  }
  if (width == 0 || height == 0 || (width == m_impl->width && height == m_impl->height)) {
    return {};
  }

  m_impl->renderTarget.Reset();
  m_impl->context->OMSetRenderTargets(0, nullptr, nullptr);
  const HRESULT result = m_impl->swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
  if (FAILED(result)) {
    return Error{Failure("IDXGISwapChain::ResizeBuffers", result), 2};
  }
  m_impl->width = width;
  m_impl->height = height;
  return CreateRenderTarget();
}

void GraphicsDevice::BeginFrame(float red, float green, float blue, float alpha) noexcept {
  if (!IsValid()) {
    return;
  }
  ID3D11RenderTargetView* targets[] = {m_impl->renderTarget.Get()};
  m_impl->context->OMSetRenderTargets(1, targets, nullptr);

  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(m_impl->width);
  viewport.Height = static_cast<float>(m_impl->height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  m_impl->context->RSSetViewports(1, &viewport);

  const float color[4] = {red, green, blue, alpha};
  m_impl->context->ClearRenderTargetView(m_impl->renderTarget.Get(), color);
}

Result<void> GraphicsDevice::EndFrame() {
  if (m_impl->swapChain == nullptr) {
    return Error{"graphics device is not created", 1};
  }
  const HRESULT result = m_impl->swapChain->Present(1, 0);
  if (FAILED(result)) {
    return Error{Failure("IDXGISwapChain::Present", result), 2};
  }
  return {};
}

bool GraphicsDevice::IsValid() const noexcept {
  return m_impl->device != nullptr && m_impl->context != nullptr && m_impl->swapChain != nullptr &&
         m_impl->renderTarget != nullptr;
}

void* GraphicsDevice::DeviceHandle() const noexcept {
  return m_impl->device.Get();
}

void* GraphicsDevice::ContextHandle() const noexcept {
  return m_impl->context.Get();
}

uint32_t GraphicsDevice::Width() const noexcept {
  return m_impl->width;
}

uint32_t GraphicsDevice::Height() const noexcept {
  return m_impl->height;
}

Result<void> GraphicsDevice::CreateRenderTarget() {
  ComPtr<ID3D11Texture2D> backBuffer;
  HRESULT result = m_impl->swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
  if (FAILED(result)) {
    return Error{Failure("IDXGISwapChain::GetBuffer", result), 1};
  }

  result = m_impl->device->CreateRenderTargetView(backBuffer.Get(), nullptr,
                                                  m_impl->renderTarget.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreateRenderTargetView", result), 2};
  }
  return {};
}

}
