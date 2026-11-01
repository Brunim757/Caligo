using Caligo.Scripting;

namespace Caligo.Gameplay;

public sealed class GameModule : IGameModule
{
  public void Configure(ScriptRegistry registry)
  {
    ArgumentNullException.ThrowIfNull(registry);
    registry.Register<Spinner>();
    registry.Register<PlayerMover>();
  }
}
