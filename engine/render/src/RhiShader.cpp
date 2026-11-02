#include <windows.h>

#include "RhiError.h"
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiShader.h>
#include <cstring>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>
#include <utility>
#include <wrl/client.h>

namespace caligo::render {

using Microsoft::WRL::ComPtr;

struct RhiShader::Impl {
  ComPtr<ID3D11VertexShader> vertexShader;
  ComPtr<ID3D11PixelShader> pixelShader;
};

namespace {

std::string CompileError(const ComPtr<ID3DBlob>& errors, HRESULT result) {
  if (errors != nullptr && errors->GetBufferSize() > 0) {
    return std::string(static_cast<const char*>(errors->GetBufferPointer()),
                       errors->GetBufferSize());
  }
  return Failure("D3DCompile", result);
}

Result<ComPtr<ID3DBlob>> CompileStage(const char* source, const char* entry, const char* target) {
  ComPtr<ID3DBlob> code;
  ComPtr<ID3DBlob> errors;
  constexpr UINT kFlags = D3DCOMPILE_ENABLE_STRICTNESS;
  const HRESULT result =
      D3DCompile(source, std::strlen(source), "caligo_shader", nullptr, nullptr, entry, target,
                 kFlags, 0, code.GetAddressOf(), errors.GetAddressOf());
  if (FAILED(result)) {
    return Error{CompileError(errors, result), 1};
  }
  return code;
}

}

Result<std::unique_ptr<RhiShader>> RhiShader::Create(RhiDevice& device, const RhiShaderDesc& desc) {
  if (desc.source == nullptr) {
    return Error{"shader needs a source", 1};
  }
  ID3D11Device* nativeDevice = static_cast<ID3D11Device*>(device.DeviceHandle());
  if (nativeDevice == nullptr) {
    return Error{"shader needs a live device", 2};
  }

  Result<ComPtr<ID3DBlob>> vertexCode = CompileStage(desc.source, desc.vertexEntry, "vs_5_0");
  if (!vertexCode) {
    return vertexCode.GetError();
  }
  Result<ComPtr<ID3DBlob>> pixelCode = CompileStage(desc.source, desc.pixelEntry, "ps_5_0");
  if (!pixelCode) {
    return pixelCode.GetError();
  }

  std::unique_ptr<RhiShader> shader(new RhiShader());
  HRESULT result = nativeDevice->CreateVertexShader(vertexCode.Value()->GetBufferPointer(),
                                                    vertexCode.Value()->GetBufferSize(), nullptr,
                                                    shader->m_impl->vertexShader.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreateVertexShader", result), 3};
  }
  result = nativeDevice->CreatePixelShader(pixelCode.Value()->GetBufferPointer(),
                                           pixelCode.Value()->GetBufferSize(), nullptr,
                                           shader->m_impl->pixelShader.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreatePixelShader", result), 4};
  }
  return std::move(shader);
}

RhiShader::RhiShader() : m_impl(std::make_unique<Impl>()) {}

RhiShader::~RhiShader() = default;

void* RhiShader::VertexShaderHandle() const noexcept {
  return m_impl->vertexShader.Get();
}

void* RhiShader::PixelShaderHandle() const noexcept {
  return m_impl->pixelShader.Get();
}

}
