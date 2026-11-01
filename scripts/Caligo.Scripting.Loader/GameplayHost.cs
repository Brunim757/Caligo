using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace Caligo.Scripting.Loader;

internal sealed class GameplayHost
{
  private const string ScriptingAssemblyFile = "Caligo.Scripting.dll";
  private const string GameplayAssemblyFile = "Caligo.Gameplay.dll";
  private const string EntryTypeName = "Caligo.Scripting.HostEntry";

  private delegate int InitializeFn(IntPtr nativeBindings, IntPtr gameplayPath);
  private delegate int QueryFn();
  private delegate int ScriptNameFn(int index, IntPtr buffer, int capacity);
  private delegate int CreateScriptFn(ulong entityId, IntPtr typeName, int length);
  private delegate int UpdateFn(float deltaSeconds);
  private delegate int ErrorFn(IntPtr buffer, int capacity);

  private readonly ScriptLoadContext _context;
  private readonly string _shadowDirectory;
  private readonly InitializeFn _initialize;
  private readonly QueryFn _scriptTypeCount;
  private readonly ScriptNameFn _scriptTypeName;
  private readonly CreateScriptFn _createScript;
  private readonly QueryFn _start;
  private readonly UpdateFn _update;
  private readonly QueryFn _stop;
  private readonly QueryFn _shutdown;
  private readonly ErrorFn _lastError;

  private GameplayHost(ScriptLoadContext context, string shadowDirectory, Type entry)
  {
    _context = context;
    _shadowDirectory = shadowDirectory;
    _initialize = Bind<InitializeFn>(entry, "Initialize");
    _scriptTypeCount = Bind<QueryFn>(entry, "GetScriptTypeCount");
    _scriptTypeName = Bind<ScriptNameFn>(entry, "GetScriptTypeName");
    _createScript = Bind<CreateScriptFn>(entry, "CreateScript");
    _start = Bind<QueryFn>(entry, "Start");
    _update = Bind<UpdateFn>(entry, "Update");
    _stop = Bind<QueryFn>(entry, "Stop");
    _shutdown = Bind<QueryFn>(entry, "Shutdown");
    _lastError = Bind<ErrorFn>(entry, "GetLastError");
  }

  public string ShadowDirectory => _shadowDirectory;

  public static GameplayHost Load(IntPtr nativeBindings, string gameplayDirectory)
  {
    if (nativeBindings == IntPtr.Zero)
    {
      throw new ArgumentException("native bindings are missing", nameof(nativeBindings));
    }
    if (!Directory.Exists(gameplayDirectory))
    {
      throw new DirectoryNotFoundException($"gameplay directory '{gameplayDirectory}' was not found");
    }

    string shadowDirectory = CreateShadowDirectory(gameplayDirectory);
    ScriptLoadContext? context = null;
    try
    {
      context = new ScriptLoadContext(shadowDirectory);
      string scriptingPath = Path.Combine(shadowDirectory, ScriptingAssemblyFile);
      string gameplayPath = Path.Combine(shadowDirectory, GameplayAssemblyFile);
      if (!File.Exists(scriptingPath))
      {
        throw new FileNotFoundException($"scripting assembly '{scriptingPath}' was not found");
      }
      if (!File.Exists(gameplayPath))
      {
        throw new FileNotFoundException($"gameplay assembly '{gameplayPath}' was not found");
      }

      Assembly assembly = context.LoadFromAssemblyPath(scriptingPath);
      Type entry = assembly.GetType(EntryTypeName, throwOnError: false) ??
                   throw new InvalidOperationException($"type '{EntryTypeName}' was not found");
      GameplayHost host = new(context, shadowDirectory, entry);
      int status;
      IntPtr pathPointer = Marshal.StringToCoTaskMemUTF8(gameplayPath);
      try
      {
        status = host._initialize(nativeBindings, pathPointer);
      }
      finally
      {
        Marshal.FreeCoTaskMem(pathPointer);
      }
      if (status != 0)
      {
        string message = host.ReadError();
        throw new InvalidOperationException(
            message.Length == 0 ? "script host refused the gameplay assembly" : message);
      }
      return host;
    }
    catch
    {
      context?.Unload();
      DeleteShadowDirectory(shadowDirectory);
      throw;
    }
  }

  public int ScriptTypeCount()
  {
    return _scriptTypeCount();
  }

  public int ScriptTypeName(int index, IntPtr buffer, int capacity)
  {
    return _scriptTypeName(index, buffer, capacity);
  }

  public int CreateScript(ulong entityId, IntPtr typeName, int length)
  {
    return _createScript(entityId, typeName, length);
  }

  public int Start()
  {
    return _start();
  }

  public int Update(float deltaSeconds)
  {
    return _update(deltaSeconds);
  }

  public int Stop()
  {
    return _stop();
  }

  public int ReadError(IntPtr buffer, int capacity)
  {
    return _lastError(buffer, capacity);
  }

  private string ReadError()
  {
    byte[] bytes = new byte[1024];
    int count;
    unsafe
    {
      fixed (byte* pointer = bytes)
      {
        count = _lastError((IntPtr)pointer, bytes.Length);
      }
    }
    return count > 0 ? Encoding.UTF8.GetString(bytes, 0, count) : string.Empty;
  }

  public void Dispose()
  {
    try
    {
      _shutdown();
    }
    finally
    {
      _context.Unload();
    }
  }

  private static T Bind<T>(Type entry, string name) where T : Delegate
  {
    MethodInfo method = entry.GetMethod(name, BindingFlags.Public | BindingFlags.Static) ??
                        throw new InvalidOperationException(
                            $"script host method '{name}' was not found");
    return (T)Delegate.CreateDelegate(typeof(T), method);
  }

  private static string CreateShadowDirectory(string gameplayDirectory)
  {
    string shadowRoot = Path.Combine(Path.GetTempPath(), "caligo", "scripting");
    string shadowDirectory = Path.Combine(shadowRoot, Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(shadowDirectory);
    try
    {
      foreach (string source in Directory.EnumerateFiles(gameplayDirectory))
      {
        string destination = Path.Combine(shadowDirectory, Path.GetFileName(source));
        File.Copy(source, destination, overwrite: true);
      }
    }
    catch
    {
      DeleteShadowDirectory(shadowDirectory);
      throw;
    }
    return shadowDirectory;
  }

  internal static void DeleteShadowDirectory(string directory)
  {
    try
    {
      if (Directory.Exists(directory))
      {
        Directory.Delete(directory, recursive: true);
      }
    }
    catch (IOException)
    {
    }
    catch (UnauthorizedAccessException)
    {
    }
  }
}
