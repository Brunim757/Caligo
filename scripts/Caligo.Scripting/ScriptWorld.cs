namespace Caligo.Scripting;

internal sealed class ScriptWorld
{
  private readonly ScriptRegistry _registry;
  private readonly INativeBindings _bindings;
  private readonly List<ScriptBehaviour> _scripts = new();
  private readonly HashSet<(ulong EntityId, string Type)> _attached = new();
  private bool _running;

  public ScriptWorld(ScriptRegistry registry, INativeBindings bindings)
  {
    _registry = registry ?? throw new ArgumentNullException(nameof(registry));
    _bindings = bindings ?? throw new ArgumentNullException(nameof(bindings));
  }

  public bool IsRunning => _running;

  public void Create(ulong entityId, string type)
  {
    if (!_attached.Add((entityId, type)))
    {
      throw new InvalidOperationException($"entity {entityId} already has script '{type}'");
    }
    ScriptBehaviour script = _registry.Create(type);
    script.Attach(new ScriptContext(_bindings, entityId));
    _scripts.Add(script);
    script.OnCreate();
  }

  public void Start()
  {
    _running = true;
  }

  public void Update(float deltaSeconds)
  {
    if (!_running)
    {
      throw new InvalidOperationException("script world is not running");
    }
    if (!float.IsFinite(deltaSeconds) || deltaSeconds < 0.0f)
    {
      throw new ArgumentOutOfRangeException(nameof(deltaSeconds), "delta must be finite and non-negative");
    }
    foreach (ScriptBehaviour script in _scripts)
    {
      script.OnUpdate(deltaSeconds);
    }
  }

  public void Stop()
  {
    foreach (ScriptBehaviour script in _scripts)
    {
      try
      {
        script.OnDestroy();
      }
      catch (Exception exception)
      {
        _bindings.Log(ScriptLogSeverity.Error, $"script destroy failed: {exception.Message}");
      }
    }
    _scripts.Clear();
    _attached.Clear();
    _running = false;
  }
}
