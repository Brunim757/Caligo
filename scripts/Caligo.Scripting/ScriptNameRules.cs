namespace Caligo.Scripting;

internal static class ScriptNameRules
{
  public static bool IsValid(string name)
  {
    if (string.IsNullOrEmpty(name) || name.Length > 128)
    {
      return false;
    }
    if (!IsAsciiLetter(name[0]))
    {
      return false;
    }
    for (int index = 1; index < name.Length; ++index)
    {
      char character = name[index];
      if (IsAsciiLetter(character) || IsAsciiDigit(character) || character == '_')
      {
        continue;
      }
      return false;
    }
    return true;
  }

  private static bool IsAsciiLetter(char character)
  {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
  }

  private static bool IsAsciiDigit(char character)
  {
    return character >= '0' && character <= '9';
  }
}
