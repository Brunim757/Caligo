namespace Caligo.Scripting;

internal sealed class ScriptContext
{
  private readonly INativeBindings _bindings;

  public ScriptContext(INativeBindings bindings, ulong entityId)
  {
    _bindings = bindings ?? throw new ArgumentNullException(nameof(bindings));
    EntityId = entityId;
  }

  public ulong EntityId { get; }

  public Transform GetTransform()
  {
    return _bindings.GetTransform(EntityId);
  }

  public void SetTransform(Transform transform)
  {
    _bindings.SetTransform(EntityId, transform);
  }

  public bool IsKeyDown(ScriptKey key)
  {
    return _bindings.IsKeyDown(key);
  }

  public bool IsKeyPressed(ScriptKey key)
  {
    return _bindings.IsKeyPressed(key);
  }

  public void Log(string message)
  {
    _bindings.Log(ScriptLogSeverity.Info, message);
  }
}
