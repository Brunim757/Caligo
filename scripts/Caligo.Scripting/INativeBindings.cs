namespace Caligo.Scripting;

internal interface INativeBindings
{
  Transform GetTransform(ulong entityId);
  void SetTransform(ulong entityId, Transform transform);
  bool IsKeyDown(ScriptKey key);
  bool IsKeyPressed(ScriptKey key);
  void Log(ScriptLogSeverity severity, string message);
}
