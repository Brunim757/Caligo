using System.Collections.ObjectModel;

namespace Caligo.Scripting;

public sealed class ScriptRegistry
{
  private readonly Dictionary<string, Func<ScriptBehaviour>> _factories = new(StringComparer.Ordinal);
  private readonly List<string> _names = new();
  private readonly ReadOnlyCollection<string> _nameView;

  public ScriptRegistry()
  {
    _nameView = _names.AsReadOnly();
  }

  public IReadOnlyList<string> Names => _nameView;

  public void Register<T>() where T : ScriptBehaviour, new()
  {
    string name = typeof(T).Name;
    if (!ScriptNameRules.IsValid(name))
    {
      throw new InvalidOperationException($"script name '{name}' is not valid");
    }
    if (_factories.ContainsKey(name))
    {
      throw new InvalidOperationException($"script '{name}' is already registered");
    }
    Func<ScriptBehaviour> factory = () => new T();
    _factories.Add(name, factory);
    string? fullName = typeof(T).FullName;
    if (!string.IsNullOrEmpty(fullName) && !string.Equals(fullName, name, StringComparison.Ordinal))
    {
      _factories.Add(fullName, factory);
    }
    _names.Add(name);
    _names.Sort(StringComparer.Ordinal);
  }

  internal ScriptBehaviour Create(string name)
  {
    ArgumentException.ThrowIfNullOrEmpty(name);
    if (!_factories.TryGetValue(name, out Func<ScriptBehaviour>? factory))
    {
      throw new InvalidOperationException($"script '{name}' is not registered");
    }
    return factory();
  }
}
