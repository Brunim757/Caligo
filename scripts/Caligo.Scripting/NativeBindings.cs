using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;

namespace Caligo.Scripting;

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeBindings
{
  public IntPtr User;
  public delegate* unmanaged[Cdecl]<IntPtr, int, int> IsKeyDown;
  public delegate* unmanaged[Cdecl]<IntPtr, int, int> IsKeyPressed;
  public delegate* unmanaged[Cdecl]<IntPtr, ulong, float*, float*, float*, int> GetTransform;
  public delegate* unmanaged[Cdecl]<IntPtr, ulong, float*, float*, float*, int> SetTransform;
  public delegate* unmanaged[Cdecl]<IntPtr, int, byte*, int, void> Log;
}

internal sealed unsafe class NativeBindingsAdapter : INativeBindings
{
  private readonly NativeBindings _native;

  public NativeBindingsAdapter(NativeBindings native)
  {
    _native = native;
  }

  public Transform GetTransform(ulong entityId)
  {
    float* position = stackalloc float[3];
    float* rotation = stackalloc float[4];
    float* scale = stackalloc float[3];
    if (_native.GetTransform(_native.User, entityId, position, rotation, scale) == 0)
    {
      throw new InvalidOperationException($"entity {entityId} does not expose a transform");
    }
    return new Transform(
        new Vector3(position[0], position[1], position[2]),
        Quaternion.Normalize(new Quaternion(rotation[0], rotation[1], rotation[2], rotation[3])),
        new Vector3(scale[0], scale[1], scale[2]));
  }

  public void SetTransform(ulong entityId, Transform transform)
  {
    float* position = stackalloc float[3];
    float* rotation = stackalloc float[4];
    float* scale = stackalloc float[3];
    position[0] = transform.Position.X;
    position[1] = transform.Position.Y;
    position[2] = transform.Position.Z;
    rotation[0] = transform.Rotation.X;
    rotation[1] = transform.Rotation.Y;
    rotation[2] = transform.Rotation.Z;
    rotation[3] = transform.Rotation.W;
    scale[0] = transform.Scale.X;
    scale[1] = transform.Scale.Y;
    scale[2] = transform.Scale.Z;
    if (_native.SetTransform(_native.User, entityId, position, rotation, scale) == 0)
    {
      throw new InvalidOperationException($"entity {entityId} rejected its transform");
    }
  }

  public bool IsKeyDown(ScriptKey key)
  {
    return _native.IsKeyDown(_native.User, (int)key) != 0;
  }

  public bool IsKeyPressed(ScriptKey key)
  {
    return _native.IsKeyPressed(_native.User, (int)key) != 0;
  }

  public void Log(ScriptLogSeverity severity, string message)
  {
    byte[] bytes = Encoding.UTF8.GetBytes(message);
    fixed (byte* pointer = bytes)
    {
      _native.Log(_native.User, (int)severity, pointer, bytes.Length);
    }
  }
}
