using A33.Instrument.Protocol;

namespace A33.Instrument.Core;

public static class InstrumentRegisterDecoder
{
    public static InstrumentSnapshot DecodeRealtime(ushort startAddress, ReadOnlySpan<ushort> values, WordOrder order, InstrumentSnapshot? previous = null)
    {
        int Offset(string name) => RegisterMap.Get(name).Address - startAddress;
        var unit = (byte)values[Offset("active_unit")]; var decimals = (byte)values[Offset("decimal_places")];
        var displayCount = RegisterValueCodec.Int32(values[Offset("display_weight")..], order);
        var flags = RegisterValueCodec.UInt32(values[Offset("status_flags")..], order);
        var now = DateTimeOffset.Now;
        return new InstrumentSnapshot(
            DisplayCountToMicrograms(displayCount, unit, decimals),
            RegisterValueCodec.Int64(values[Offset("net_mass_ug")..], order),
            RegisterValueCodec.Int64(values[Offset("gross_mass_ug")..], order),
            RegisterValueCodec.Int64(values[Offset("tare_mass_ug")..], order),
            unit, decimals, (byte)values[Offset("division")],
            (flags & (1u << 4)) != 0, (flags & (1u << 5)) != 0, (flags & (1u << 6)) != 0, (flags & (1u << 7)) != 0,
            RegisterValueCodec.Int32(values[Offset("raw_value")..], order), RegisterValueCodec.Int32(values[Offset("filtered_raw")..], order),
            RegisterValueCodec.UInt32(values[Offset("sample_sequence")..], order), previous?.CheckweighState ?? 0, previous?.DisplayLocked ?? false,
            previous?.ConfigDirty ?? false, previous?.FaultMask ?? 0, now);
    }

    public static long DisplayCountToMicrograms(int displayCount, byte unit, byte decimals)
    {
        long scale = unit switch { 0 => 1_000_000_000L, 1 => 1_000_000L, 2 => 453_592_370L, _ => throw new ArgumentOutOfRangeException(nameof(unit)) };
        long divisor = 1; for (var i = 0; i < decimals; i++) divisor = checked(divisor * 10);
        return checked(displayCount * scale / divisor);
    }
}

public static class MassFormatter
{
    public static string Format(long micrograms, byte unit, byte decimals)
    {
        long scale = unit switch { 0 => 1_000_000_000L, 1 => 1_000_000L, 2 => 453_592_370L, _ => 1_000_000_000L };
        var negative = micrograms < 0; var absolute = (ulong)(negative ? -(micrograms + 1) + 1 : micrograms); ulong factor = 1;
        for (var i = 0; i < decimals; i++) factor *= 10;
        var displayed = absolute * factor / (ulong)scale; var whole = displayed / factor; var fraction = displayed % factor;
        return $"{(negative ? "-" : "")}{whole}{(decimals > 0 ? $".{fraction.ToString().PadLeft(decimals, '0')}" : "")}";
    }
    public static string UnitName(byte unit) => unit switch { 0 => "kg", 1 => "g", 2 => "lb", _ => "?" };
}
