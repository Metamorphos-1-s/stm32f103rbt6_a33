using System.Reflection;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace A33.Instrument.Protocol;

public sealed record RegisterDefinition(
    [property: JsonPropertyName("address")] ushort Address,
    [property: JsonPropertyName("name")] string Name,
    [property: JsonPropertyName("register_count")] ushort RegisterCount,
    [property: JsonPropertyName("data_type")] string DataType,
    [property: JsonPropertyName("signed")] bool Signed,
    [property: JsonPropertyName("unit")] string Unit,
    [property: JsonPropertyName("access")] string Access,
    [property: JsonPropertyName("word_order_rule")] string WordOrderRule,
    [property: JsonPropertyName("description")] string Description,
    [property: JsonPropertyName("map_version")] string MapVersion)
{
    public bool IsReadOnly => string.Equals(Access, "read", StringComparison.OrdinalIgnoreCase);
}

public static class RegisterMap
{
    public const ushort Version = 0x0104;
    public static IReadOnlyList<RegisterDefinition> Definitions { get; } = LoadEmbedded();
    public static RegisterDefinition Get(string name) => Definitions.Single(x => x.Name == name);

    private static IReadOnlyList<RegisterDefinition> LoadEmbedded()
    {
        using var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("A33.Instrument.Protocol.register-map.json")
            ?? throw new InvalidOperationException("Embedded register map is missing.");
        return JsonSerializer.Deserialize<List<RegisterDefinition>>(stream)
            ?? throw new InvalidOperationException("Embedded register map is invalid.");
    }
}
