using System.Numerics;
using Caligo.Scripting;

namespace Caligo.Gameplay;

public sealed class PlayerMover : ScriptBehaviour
{
  private const float Speed = 4.0f;

  public override void OnUpdate(float deltaSeconds)
  {
    float horizontal = Axis(ScriptKey.D, ScriptKey.A);
    float vertical = Axis(ScriptKey.W, ScriptKey.S);
    Vector3 direction = new(horizontal, 0.0f, vertical);
    if (direction.LengthSquared() > 0.0f)
    {
      direction = Vector3.Normalize(direction);
    }
    Transform transform = Transform;
    Vector3 position = transform.Position + direction * (Speed * deltaSeconds);
    transform.Position = new Vector3(position.X, transform.Position.Y, position.Z);
    Transform = transform;
  }

  private float Axis(ScriptKey positive, ScriptKey negative)
  {
    float value = 0.0f;
    if (IsKeyDown(positive))
    {
      value += 1.0f;
    }
    if (IsKeyDown(negative))
    {
      value -= 1.0f;
    }
    return value;
  }
}
