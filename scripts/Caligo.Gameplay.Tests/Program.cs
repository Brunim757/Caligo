using System.Numerics;
using Caligo.Scripting;

namespace Caligo.Gameplay.Tests;

internal static class Program
{
  private static int _failures;

  private static int Main()
  {
    Run("registry lists game scripts", RegistryListsGameScripts);
    Run("registry rejects duplicate registration", RegistryRejectsDuplicateRegistration);
    Run("registry rejects invalid names", RegistryRejectsInvalidName);
    Run("world creates scripts for entities", WorldCreatesScriptsForEntities);
    Run("world rejects duplicate attachment", WorldRejectsDuplicateAttachment);
    Run("world rejects unknown scripts", WorldRejectsUnknownScript);
    Run("world rejects invalid delta", WorldRejectsInvalidDelta);
    Run("spinner rotates around Y", SpinnerRotatesAroundY);
    Run("player mover advances forward", PlayerMoverAdvancesForward);
    Run("player mover normalizes diagonals", PlayerMoverNormalizesDiagonals);

    if (_failures == 0)
    {
      Console.WriteLine("gameplay tests passed");
      return 0;
    }
    Console.Error.WriteLine($"gameplay tests failed: {_failures}");
    return 1;
  }

  private static void RegistryListsGameScripts()
  {
    ScriptRegistry registry = new();
    new Caligo.Gameplay.GameModule().Configure(registry);
    Check(registry.Names.Count == 2, "two scripts are registered");
    Check(registry.Names[0] == "PlayerMover", "scripts are sorted by name");
    Check(registry.Names[1] == "Spinner", "Spinner is registered");
  }

  private static void RegistryRejectsDuplicateRegistration()
  {
    ScriptRegistry registry = new();
    registry.Register<Spinner>();
    Throws<InvalidOperationException>(() => registry.Register<Spinner>(),
                                      "duplicate registration is rejected");
  }

  private static void RegistryRejectsInvalidName()
  {
    ScriptRegistry registry = new();
    Throws<InvalidOperationException>(() => registry.Register<_HiddenScript>(),
                                      "script names must start with a letter");
  }

  private static void WorldCreatesScriptsForEntities()
  {
    FakeBindings bindings = new();
    bindings.Transforms[7] = Transform.Identity;
    ScriptWorld world = CreateWorld(bindings, "Spinner");
    world.Start();
    world.Update(0.0f);
    Check(bindings.Severities.Count == 0, "creating a script does not log errors");
  }

  private static void WorldRejectsDuplicateAttachment()
  {
    FakeBindings bindings = new();
    bindings.Transforms[1] = Transform.Identity;
    ScriptWorld world = CreateWorld(bindings, "Spinner");
    Throws<InvalidOperationException>(() => world.Create(1, "Spinner"),
                                      "the same script cannot be attached twice");
  }

  private static void WorldRejectsUnknownScript()
  {
    FakeBindings bindings = new();
    bindings.Transforms[1] = Transform.Identity;
    ScriptWorld world = CreateWorld(bindings, "Spinner");
    Throws<InvalidOperationException>(() => world.Create(1, "Missing"),
                                      "unknown scripts are rejected");
  }

  private static void WorldRejectsInvalidDelta()
  {
    FakeBindings bindings = new();
    bindings.Transforms[1] = Transform.Identity;
    ScriptWorld world = CreateWorld(bindings, "Spinner");
    world.Start();
    Throws<ArgumentOutOfRangeException>(() => world.Update(-0.5f), "negative delta is rejected");
    Throws<ArgumentOutOfRangeException>(() => world.Update(float.NaN), "nan delta is rejected");
  }

  private static void SpinnerRotatesAroundY()
  {
    FakeBindings bindings = new();
    bindings.Transforms[3] = Transform.Identity;
    ScriptWorld world = CreateWorld(bindings, "Spinner");
    world.Start();
    world.Update(1.0f);
    Transform transform = bindings.Transforms[3];
    Check(MathF.Abs(transform.Rotation.Y) > 0.69f, "spinner rotated a quarter turn");
    Check(MathF.Abs(transform.Rotation.W - 0.7071f) < 0.01f, "spinner rotation is normalized");
    Check(transform.Position == Vector3.Zero, "spinner does not move");
  }

  private static void PlayerMoverAdvancesForward()
  {
    FakeBindings bindings = new();
    bindings.Transforms[5] = new Transform(new Vector3(0.0f, 2.5f, 0.0f), Quaternion.Identity,
                                           Vector3.One);
    bindings.Down.Add(ScriptKey.W);
    ScriptWorld world = CreateWorld(bindings, "PlayerMover");
    world.Start();
    world.Update(1.0f);
    Transform transform = bindings.Transforms[5];
    Check(MathF.Abs(transform.Position.Z - 4.0f) < 0.001f, "player moved four units on Z");
    Check(transform.Position.Y == 2.5f, "player keeps its height");
    Check(MathF.Abs(transform.Position.X) < 0.001f, "player does not drift sideways");
  }

  private static void PlayerMoverNormalizesDiagonals()
  {
    FakeBindings bindings = new();
    bindings.Transforms[9] = Transform.Identity;
    bindings.Down.Add(ScriptKey.W);
    bindings.Down.Add(ScriptKey.D);
    ScriptWorld world = CreateWorld(bindings, "PlayerMover");
    world.Start();
    world.Update(1.0f);
    Vector3 position = bindings.Transforms[9].Position;
    float distance = position.Length();
    Check(MathF.Abs(distance - 4.0f) < 0.001f, "diagonal movement keeps the same speed");
    Check(position.X > 0.0f && position.Z > 0.0f, "diagonal movement uses both axes");
  }

  private static ScriptWorld CreateWorld(FakeBindings bindings, string scriptName)
  {
    ScriptRegistry registry = new();
    registry.Register<Spinner>();
    registry.Register<PlayerMover>();
    ScriptWorld world = new(registry, bindings);
    foreach (ulong entityId in bindings.Transforms.Keys)
    {
      world.Create(entityId, scriptName);
    }
    return world;
  }

  private static void Run(string name, Action test)
  {
    int before = _failures;
    try
    {
      test();
    }
    catch (Exception exception)
    {
      Fail($"{name}: {exception.GetType().Name}: {exception.Message}");
      return;
    }
    if (_failures == before)
    {
      Console.WriteLine($"ok: {name}");
    }
  }

  private static void Check(bool condition, string message)
  {
    if (!condition)
    {
      Fail(message);
    }
  }

  private static void Throws<T>(Action action, string message) where T : Exception
  {
    try
    {
      action();
    }
    catch (T)
    {
      return;
    }
    catch (Exception exception)
    {
      Fail($"{message}: threw {exception.GetType().Name}");
      return;
    }
    Fail($"{message}: no exception was thrown");
  }

  private static void Fail(string message)
  {
    ++_failures;
    Console.Error.WriteLine($"fail: {message}");
  }

  private sealed class _HiddenScript : ScriptBehaviour
  {
    public _HiddenScript()
    {
    }
  }
}