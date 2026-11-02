using System.Numerics;

namespace Caligo.Scripting;

public struct Transform
{
  public Transform(Vector3 position, Quaternion rotation, Vector3 scale)
  {
    Position = position;
    Rotation = rotation;
    Scale = scale;
  }

  public Vector3 Position;
  public Quaternion Rotation;
  public Vector3 Scale;

  public static Transform Identity => new(Vector3.Zero, Quaternion.Identity, Vector3.One);
}
