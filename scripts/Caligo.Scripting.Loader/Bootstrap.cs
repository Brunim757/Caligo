using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace Caligo.Scripting.Loader;

public static class Bootstrap
{
  private static GameplayHost? _host;
  private static string? _lastError;

  [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
  public static int Initialize(IntPtr nativeBindings, IntPtr gameplayDirectory)
  {
    try
    {
      if (Volatile.Read(ref _host) != null)
      {
        throw new InvalidOperationException("script host is already initialized");
      }
      string? path = Marshal.PtrToStringUTF8(gameplayDirectory);
      if (string.IsNullOrWhiteSpace(path))
      {
        throw new ArgumentException("gameplay directory is empty", nameof(gameplayDirectory));
      }
      GameplayHost host = GameplayHost.Load(nativeBindings, path);
      Volatile.Write(ref _host, host);
      _lastError = null;
      return 0;
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
  public static int GetScriptTypeCount()
  {
    try
    {
      return RequireHost().ScriptTypeCount();
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
  public static int GetScriptTypeName(int index, IntPtr buffer, int capacity)
  {
    try
    {
      return RequireHost().ScriptTypeName(index, buffer, capacity);
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
  public static int CreateScript(ulong entityId, IntPtr typeName, int length)
  {
    try
    {
      return RequireHost().CreateScript(entityId, typeName, length);
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
  public static int Start()
  {
    try
    {
      return RequireHost().Start();
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
  public static int Update(float deltaSeconds)
  {
    try
    {
      return RequireHost().Update(deltaSeconds);
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
  public static int Stop()
  {
    try
    {
      return RequireHost().Stop();
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
  public static int Reset()
  {
    try
    {
      GameplayHost? host = Interlocked.Exchange(ref _host, null);
      string? shadowDirectory = host?.ShadowDirectory;
      host?.Dispose();
      host = null;
      FinishReset(shadowDirectory);
      _lastError = null;
      return 0;
    }
    catch (Exception exception)
    {
      return Fail(exception);
    }
  }

  [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
  public static int GetLastError(IntPtr buffer, int capacity)
  {
    try
    {
      GameplayHost? host = Volatile.Read(ref _host);
      if (host != null)
      {
        int written = host.ReadError(buffer, capacity);
        if (written > 0)
        {
          return written;
        }
      }
      return WriteUtf8(_lastError ?? string.Empty, buffer, capacity);
    }
    catch (Exception exception)
    {
      _lastError = exception.Message;
      return 0;
    }
  }

  private static GameplayHost RequireHost()
  {
    return Volatile.Read(ref _host) ??
           throw new InvalidOperationException("script host is not initialized");
  }

  private static int Fail(Exception exception)
  {
    _lastError = exception.Message;
    return -1;
  }

  private static int WriteUtf8(string text, IntPtr buffer, int capacity)
  {
    if (buffer == IntPtr.Zero || capacity <= 0)
    {
      return 0;
    }
    byte[] bytes = Encoding.UTF8.GetBytes(text);
    int count = Math.Min(bytes.Length, capacity - 1);
    Marshal.Copy(bytes, 0, buffer, count);
    unsafe
    {
      ((byte*)buffer)[count] = 0;
    }
    return count;
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  private static void FinishReset(string? shadowDirectory)
  {
    GC.Collect();
    GC.WaitForPendingFinalizers();
    GC.Collect();
    if (shadowDirectory != null)
    {
      GameplayHost.DeleteShadowDirectory(shadowDirectory);
    }
  }
}
