namespace Caligo.Scripting;

public abstract class ScriptBehaviour
{
  private ScriptContext? _context;

  public ulong EntityId => Context.EntityId;

  public Transform Transform
  {
    get => Context.GetTransform();
    set => Context.SetTransform(value);
  }

  private ScriptContext Context =>
      _context ?? throw new InvalidOperationException("script behaviour is not attached to an entity");

  public virtual void OnCreate()
  {
  }

  public virtual void OnUpdate(float deltaSeconds)
  {
  }

  public virtual void OnDestroy()
  {
  }

  protected bool IsKeyDown(ScriptKey key)
  {
    return Context.IsKeyDown(key);
  }

  protected bool IsKeyPressed(ScriptKey key)
  {
    return Context.IsKeyPressed(key);
  }

  protected void Log(string message)
  {
    Context.Log($"{GetType().Name}: {message}");
  }

  internal void Attach(ScriptContext context)
  {
    ArgumentNullException.ThrowIfNull(context);
    if (_context != null)
    {
      throw new InvalidOperationException("script behaviour is already attached to an entity");
    }
    _context = context;
  }
}
