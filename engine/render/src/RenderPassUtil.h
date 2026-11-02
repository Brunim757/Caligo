#pragma once

#include <windows.h>

#include <caligo/core/Result.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiInputLayout.h>
#include <caligo/render/RhiPipeline.h>
#include <caligo/render/RhiShader.h>
#include <d3d11.h>
#include <memory>
#include <utility>

namespace caligo::render::detail {

template <typename T>
Result<void> CreateShader(RhiDevice& device, const char* source, std::unique_ptr<T>& target) {
  const RhiShaderDesc description{source};
  auto shader = RhiShader::Create(device, description);
  if (!shader) {
    return shader.GetError();
  }
  target = std::move(shader).Value();
  return {};
}

template <typename T>
Result<void> CreatePipeline(RhiDevice& device, const RhiPipelineDesc& description,
                            std::unique_ptr<T>& target) {
  auto pipeline = RhiPipeline::Create(device, description);
  if (!pipeline) {
    return pipeline.GetError();
  }
  target = std::move(pipeline).Value();
  return {};
}

inline void BindPipeline(ID3D11DeviceContext* context, const RhiShader& shader,
                         const RhiInputLayout& layout, const RhiPipeline& pipeline) {
  context->IASetInputLayout(static_cast<ID3D11InputLayout*>(layout.Handle()));
  context->VSSetShader(static_cast<ID3D11VertexShader*>(shader.VertexShaderHandle()), nullptr, 0);
  context->PSSetShader(static_cast<ID3D11PixelShader*>(shader.PixelShaderHandle()), nullptr, 0);
  context->RSSetState(static_cast<ID3D11RasterizerState*>(pipeline.RasterizerState()));
  context->OMSetDepthStencilState(
      static_cast<ID3D11DepthStencilState*>(pipeline.DepthStencilState()), 0);
}

}