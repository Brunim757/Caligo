using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Text;

namespace Caligo.Scripting;

public static unsafe class HostEntry
{
  private static ScriptRegistry? _registry;
  private static ScriptWorld? _world;
  private static INativeBindings? _bindings;
  private static string? _lastError;
  private static bool _failed;

  public static int Initialize(IntPtr nativeBindings, IntPtr gameplayAssemblyPath)
  {
    try
    {
      if (nativeBindings == IntPtr.Zero)
      {
        throw new ArgumentException("native bindings pointer is null", nameof(nativeBindings));
      }
      if (gameplayAssemblyPath == IntPtr.Zero)
      {
        throw new ArgumentException("gameplay assembly path is null", nameof(gameplayAssemblyPath));
      }
      if (_registry != null)
      {
        throw new InvalidOperationException("script host is already initialized");
      }
      string? path = Marshal.PtrToStringUTF8(gameplayAssemblyPath);
      if (string.IsNullOrWhiteSpace(path))
      {
        throw new ArgumentException("gameplay assembly path is empty", nameof(gameplayAssemblyPath));
      }
      AssemblyLoadContext context = AssemblyLoadContext.GetLoadContext(typeof(HostEntry).Assembly)
          ?? throw new InvalidOperationException("script host has no load context");
      Assembly gameplay = context.LoadFromAssemblyPath(Path.GetFullPath(path));
      Type moduleType = gameplay.GetType("Caligo.Gameplay.GameModule", throwOnError: false)
          ?? throw new InvalidOperationException("Caligo.Gameplay.GameModule was not found");
      if (!typeof(IGameModule).IsAssignableFrom(moduleType))
      {
        throw new InvalidOperationException("GameModule does not implement IGameModule");
      }
      if (Activator.CreateInstance(moduleType) is not IGameModule module)
      {
        throw new InvalidOperationException("GameModule could not be created");
      }
      INativeBindings bindings = new NativeBindingsAdapter(*(NativeBindings*)nativeBindings);
      _bindings = bindings;
      ScriptRegistry registry = new();
      module.Configure(registry);
      _registry = registry;
      _world = new ScriptWorld(registry, bindings);
      _failed = false;
      _lastError = null;
      return 0;
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  public static int GetScriptTypeCount()
  {
    try
    {
      return RequireRegistry().Names.Count;
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  public static int GetScriptTypeName(int index, IntPtr buffer, int capacity)
  {
    try
    {
      ScriptRegistry registry = RequireRegistry();
      if (index < 0 || index >= registry.Names.Count)
      {
        throw new ArgumentOutOfRangeException(nameof(index));
      }
      return WriteUtf8(registry.Names[index], buffer, capacity);
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  public static int CreateScript(ulong entityId, IntPtr typeName, int length)
  {
    try
    {
      ScriptWorld world = RequireWorld();
      if (typeName == IntPtr.Zero || length <= 0)
      {
        throw new ArgumentException("script type is empty", nameof(typeName));
      }
      string type = Encoding.UTF8.GetString(new ReadOnlySpan<byte>((void*)typeName, length));
      world.Create(entityId, type);
      return 0;
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  public static int Start()
  {
    try
    {
      RequireWorld().Start();
      return 0;
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  public static int Update(float deltaSeconds)
  {
    try
    {
      RequireWorld().Update(deltaSeconds);
      return 0;
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  public static int Stop()
  {
    try
    {
      _world?.Stop();
      return 0;
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  public static int Shutdown()
  {
    try
    {
      _world?.Stop();
      _world = null;
      _registry = null;
      _bindings = null;
      _lastError = null;
      _failed = false;
      return 0;
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  public static int GetLastError(IntPtr buffer, int capacity)
  {
    return WriteUtf8(_lastError ?? string.Empty, buffer, capacity);
  }

  private static ScriptRegistry RequireRegistry()
  {
    return _registry ?? throw new InvalidOperationException("script host is not initialized");
  }

  private static ScriptWorld RequireWorld()
  {
    if (_failed)
    {
      throw new InvalidOperationException(_lastError ?? "script host failed");
    }
    return _world ?? throw new InvalidOperationException("script host is not initialized");
  }

  private static int Fail(Exception exception)
  {
    _lastError = exception.Message;
    _failed = true;
    if (_world != null)
    {
      try
      {
        _world.Stop();
      }
      catch (Exception stopException)
      {
        _lastError = $"{exception.Message} | {stopException.Message}";
      }
      _world = null;
    }
    _bindings?.Log(ScriptLogSeverity.Error, $"script host: {_lastError}");
    return 1;
  }

  private static int WriteUtf8(string text, IntPtr buffer, int capacity)
  {
    try
    {
      if (buffer == IntPtr.Zero || capacity <= 0)
      {
        return -1;
      }
      byte[] bytes = Encoding.UTF8.GetBytes(text);
      int count = Math.Min(bytes.Length, capacity - 1);
      new ReadOnlySpan<byte>(bytes, 0, count).CopyTo(new Span<byte>((void*)buffer, count));
      ((byte*)buffer)[count] = 0;
      return count;
    }
    catch (Exception exception)
    {
      _lastError = exception.Message;
      return -1;
    }
  }
}
