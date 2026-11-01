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
      _bindings = new NativeBindingsAdapter(*(NativeBindings*)nativeBindings);
      ScriptRegistry registry = new();
      module.Configure(registry);
      _registry = registry;
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
    return _registry?.Names.Count ?? 0;
  }

  public static int GetScriptTypeName(int index, IntPtr buffer, int capacity)
  {
    try
    {
      if (_registry == null)
      {
        throw new InvalidOperationException("script host is not initialized");
      }
      if (index < 0 || index >= _registry.Names.Count)
      {
        throw new ArgumentOutOfRangeException(nameof(index));
      }
      return WriteUtf8(_registry.Names[index], buffer, capacity);
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
      if (_registry == null || _bindings == null)
      {
        throw new InvalidOperationException("script host is not initialized");
      }
      if (typeName == IntPtr.Zero || length <= 0)
      {
        throw new ArgumentException("script type is empty", nameof(typeName));
      }
      string type = Encoding.UTF8.GetString(new ReadOnlySpan<byte>((void*)typeName, length));
      _world ??= new ScriptWorld(_registry, _bindings);
      _world.Create(entityId, type);
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
      _world?.Start();
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
      _world?.Update(deltaSeconds);
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
      _world = null;
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

  private static int Fail(Exception exception)
  {
    _lastError = exception.Message;
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
