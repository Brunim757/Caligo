using Caligo.Scripting;

namespace Caligo.Gameplay.Tests;

internal sealed class FakeBindings : INativeBindings
{
  public Dictionary<ulong, Transform> Transforms { get; } = new();

  public HashSet<ScriptKey> Down { get; } = new();

  public List<ScriptLogSeverity> Severities { get; } = new();

  public List<string> Messages { get; } = new();

  public Transform GetTransform(ulong entityId)
  {
    return Transforms.TryGetValue(entityId, out Transform value)
        ? value
        : throw new InvalidOperationException($"entity {entityId} was not registered");
  }

  public void SetTransform(ulong entityId, Transform transform)
  {
    if (!Transforms.ContainsKey(entityId))
    {
      throw new InvalidOperationException($"entity {entityId} was not registered");
    }
    Transforms[entityId] = transform;
  }

  public bool IsKeyDown(ScriptKey key)
  {
    return Down.Contains(key);
  }

  public bool IsKeyPressed(ScriptKey key)
  {
    return Down.Contains(key);
  }

  public void Log(ScriptLogSeverity severity, string message)
  {
    Severities.Add(severity);
    Messages.Add(message);
  }
}