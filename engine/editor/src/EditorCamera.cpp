#include "EditorState.h"
#include <algorithm>
#include <cmath>

namespace caligo::editor {

math::Vec3 Camera::Position() const noexcept {
  const float safeDistance = std::max(distance, 0.5f);
  const float yawRadians = math::Radians(yaw);
  const float pitchRadians = math::Radians(pitch);
  const float horizontal = std::cos(pitchRadians);
  const math::Vec3 offset{horizontal * std::sin(yawRadians), std::sin(pitchRadians),
                          horizontal * std::cos(yawRadians)};
  return math::Add(target, math::Scale(offset, safeDistance));
}

math::Mat4 Camera::ViewMatrix() const noexcept {
  const math::Vec3 eye = Position();
  const math::Vec3 direction = math::Normalize(math::Subtract(target, eye));
  math::Vec3 up{0.0f, 1.0f, 0.0f};
  if (std::abs(math::Dot(direction, up)) > 0.999f) {
    up = math::Vec3{0.0f, 0.0f, 1.0f};
  }
  return math::LookAt(eye, target, up);
}

math::Mat4 Camera::ProjectionMatrix(float aspect) const noexcept {
  return math::Perspective(55.0f, aspect, 0.1f, 5000.0f);
}

void Camera::Focus(const math::Vec3& point) noexcept {
  target = point;
  distance = std::clamp(distance, 3.0f, 12.0f);
}

}
