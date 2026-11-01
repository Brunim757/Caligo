using System.Numerics;
using Caligo.Scripting;

namespace Caligo.Gameplay;

public sealed class Spinner : ScriptBehaviour
{
  private const float DegreesPerSecond = 90.0f;

  private Quaternion _baseRotation = Quaternion.Identity;
  private float _angle;

  public override void OnCreate()
  {
    _baseRotation = Transform.Rotation;
  }

  public override void OnUpdate(float deltaSeconds)
  {
    _angle += deltaSeconds * DegreesPerSecond * MathF.PI / 180.0f;
    _angle %= MathF.Tau;
    Transform transform = Transform;
    transform.Rotation = Quaternion.Normalize(
        _baseRotation * Quaternion.CreateFromAxisAngle(Vector3.UnitY, _angle));
    Transform = transform;
  }
}
