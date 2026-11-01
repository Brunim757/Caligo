#pragma once

#include <DirectXMath.h>
#include <cmath>

namespace caligo::math {

using Vec3 = DirectX::XMFLOAT3;
using Quat = DirectX::XMFLOAT4;
using Mat4 = DirectX::XMFLOAT4X4;

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kEpsilon = 1.0e-6f;

[[nodiscard]] constexpr float Clamp(float value, float minimum, float maximum) noexcept {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

[[nodiscard]] constexpr float Lerp(float from, float to, float factor) noexcept {
  return from + (to - from) * factor;
}

[[nodiscard]] constexpr float Radians(float degrees) noexcept {
  return degrees * (kPi / 180.0f);
}

[[nodiscard]] constexpr float Degrees(float radians) noexcept {
  return radians * (180.0f / kPi);
}

[[nodiscard]] inline Vec3 Add(const Vec3& left, const Vec3& right) noexcept {
  Vec3 result{};
  DirectX::XMStoreFloat3(
      &result, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&left), DirectX::XMLoadFloat3(&right)));
  return result;
}

[[nodiscard]] inline Vec3 Subtract(const Vec3& left, const Vec3& right) noexcept {
  Vec3 result{};
  DirectX::XMStoreFloat3(&result, DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&left),
                                                            DirectX::XMLoadFloat3(&right)));
  return result;
}

[[nodiscard]] inline Vec3 Scale(const Vec3& value, float factor) noexcept {
  Vec3 result{};
  DirectX::XMStoreFloat3(&result, DirectX::XMVectorScale(DirectX::XMLoadFloat3(&value), factor));
  return result;
}

[[nodiscard]] inline float Dot(const Vec3& left, const Vec3& right) noexcept {
  return DirectX::XMVectorGetX(
      DirectX::XMVector3Dot(DirectX::XMLoadFloat3(&left), DirectX::XMLoadFloat3(&right)));
}

[[nodiscard]] inline Vec3 Cross(const Vec3& left, const Vec3& right) noexcept {
  Vec3 result{};
  DirectX::XMStoreFloat3(&result, DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&left),
                                                          DirectX::XMLoadFloat3(&right)));
  return result;
}

[[nodiscard]] inline float Length(const Vec3& value) noexcept {
  return std::sqrt(Dot(value, value));
}

[[nodiscard]] inline Vec3 Normalize(const Vec3& value) noexcept {
  const float length = Length(value);
  if (length <= kEpsilon) {
    return Vec3{0.0f, 0.0f, 0.0f};
  }
  return Scale(value, 1.0f / length);
}

[[nodiscard]] inline Mat4 Identity() noexcept {
  Mat4 result{};
  DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixIdentity());
  return result;
}

[[nodiscard]] inline Mat4 Multiply(const Mat4& left, const Mat4& right) noexcept {
  Mat4 result{};
  DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixMultiply(DirectX::XMLoadFloat4x4(&left),
                                                              DirectX::XMLoadFloat4x4(&right)));
  return result;
}

[[nodiscard]] inline Mat4 Transpose(const Mat4& value) noexcept {
  Mat4 result{};
  DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&value)));
  return result;
}

[[nodiscard]] inline Mat4 Inverse(const Mat4& value) noexcept {
  Mat4 result{};
  DirectX::XMStoreFloat4x4(&result,
                           DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&value)));
  return result;
}

[[nodiscard]] inline Quat IdentityRotation() noexcept {
  return Quat{0.0f, 0.0f, 0.0f, 1.0f};
}

[[nodiscard]] inline Quat NormalizeRotation(const Quat& rotation) noexcept {
  const float lengthSquared = rotation.x * rotation.x + rotation.y * rotation.y +
                              rotation.z * rotation.z + rotation.w * rotation.w;
  if (!(lengthSquared > kEpsilon) || !std::isfinite(lengthSquared)) {
    return IdentityRotation();
  }
  Quat result{};
  DirectX::XMStoreFloat4(&result, DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&rotation)));
  return result;
}

[[nodiscard]] inline Quat RotationFromEulerDegrees(const Vec3& euler) noexcept {
  Quat result{};
  DirectX::XMStoreFloat4(&result, DirectX::XMQuaternionRotationRollPitchYaw(
                                      Radians(euler.x), Radians(euler.y), Radians(euler.z)));
  return NormalizeRotation(result);
}

[[nodiscard]] inline Vec3 EulerDegreesFromRotation(const Quat& rotation) noexcept {
  const Quat value = NormalizeRotation(rotation);
  const float m00 = 1.0f - 2.0f * (value.y * value.y + value.z * value.z);
  const float m01 = 2.0f * (value.x * value.y + value.w * value.z);
  const float m02 = 2.0f * (value.x * value.z - value.w * value.y);
  const float m11 = 1.0f - 2.0f * (value.x * value.x + value.z * value.z);
  const float m20 = 2.0f * (value.x * value.z + value.w * value.y);
  const float m21 = 2.0f * (value.y * value.z - value.w * value.x);
  const float m22 = 1.0f - 2.0f * (value.x * value.x + value.y * value.y);

  const float pitch = std::asin(Clamp(-m21, -1.0f, 1.0f));
  const float yawReference = std::sqrt(m00 * m00 + m02 * m02);
  float yaw = 0.0f;
  float roll = 0.0f;
  if (yawReference > 1.0e-4f) {
    yaw = std::atan2(m20, m22);
    roll = std::atan2(m01, m11);
  } else {
    yaw = std::atan2(-m02, m00);
  }
  return Vec3{Degrees(pitch), Degrees(yaw), Degrees(roll)};
}

[[nodiscard]] inline Mat4 ComposeTransform(const Vec3& position, const Quat& rotation,
                                           const Vec3& scale) noexcept {
  const DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
  const DirectX::XMMATRIX rotating =
      DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation));
  const DirectX::XMMATRIX translating =
      DirectX::XMMatrixTranslation(position.x, position.y, position.z);
  Mat4 result{};
  DirectX::XMStoreFloat4x4(&result, scaling * rotating * translating);
  return result;
}

[[nodiscard]] inline bool DecomposeTransform(const Mat4& matrix, Vec3& position, Quat& rotation,
                                             Vec3& scale) noexcept {
  DirectX::XMVECTOR scaleVector{};
  DirectX::XMVECTOR rotationVector{};
  DirectX::XMVECTOR translationVector{};
  if (!DirectX::XMMatrixDecompose(&scaleVector, &rotationVector, &translationVector,
                                  DirectX::XMLoadFloat4x4(&matrix))) {
    return false;
  }
  DirectX::XMStoreFloat3(&position, translationVector);
  DirectX::XMStoreFloat4(&rotation, DirectX::XMQuaternionNormalize(rotationVector));
  DirectX::XMStoreFloat3(&scale, scaleVector);
  return true;
}

[[nodiscard]] inline Vec3 TransformPoint(const Mat4& matrix, const Vec3& point) noexcept {
  Vec3 result{};
  DirectX::XMStoreFloat3(&result, DirectX::XMVector3TransformCoord(
                                      DirectX::XMVectorSet(point.x, point.y, point.z, 1.0f),
                                      DirectX::XMLoadFloat4x4(&matrix)));
  return result;
}

[[nodiscard]] inline Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) noexcept {
  Mat4 result{};
  DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye),
                                                              DirectX::XMLoadFloat3(&target),
                                                              DirectX::XMLoadFloat3(&up)));
  return result;
}

[[nodiscard]] inline Mat4 Perspective(float verticalFovDegrees, float aspect, float nearPlane,
                                      float farPlane) noexcept {
  const float safeAspect = aspect > kEpsilon ? aspect : 1.0f;
  Mat4 result{};
  DirectX::XMStoreFloat4x4(&result,
                           DirectX::XMMatrixPerspectiveFovLH(Radians(verticalFovDegrees),
                                                             safeAspect, nearPlane, farPlane));
  return result;
}

}
