using System.Reflection;
using System.Runtime.Loader;

namespace Caligo.Scripting.Loader;

internal sealed class ScriptLoadContext : AssemblyLoadContext
{
  private readonly string _directory;

  public ScriptLoadContext(string directory)
      : base("Caligo.Scripting", isCollectible: true)
  {
    _directory = directory;
  }

  protected override Assembly? Load(AssemblyName assemblyName)
  {
    foreach (Assembly loaded in Assemblies)
    {
      if (AssemblyName.ReferenceMatchesDefinition(assemblyName, loaded.GetName()))
      {
        return loaded;
      }
    }
    string name = assemblyName.Name ?? string.Empty;
    if (name.Length == 0 || name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
    {
      return null;
    }
    string candidate = Path.Combine(_directory, name + ".dll");
    return File.Exists(candidate) ? LoadFromAssemblyPath(candidate) : null;
  }
}
