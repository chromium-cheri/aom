// <auto-generated>
//  automatically generated by the FlatBuffers compiler, do not modify
// </auto-generated>

namespace MyGame.Example
{

using global::System;
using global::FlatBuffers;

public struct Stat : IFlatbufferObject
{
  private Table __p;
  public ByteBuffer ByteBuffer { get { return __p.bb; } }
  public static Stat GetRootAsStat(ByteBuffer _bb) { return GetRootAsStat(_bb, new Stat()); }
  public static Stat GetRootAsStat(ByteBuffer _bb, Stat obj) { return (obj.__assign(_bb.GetInt(_bb.Position) + _bb.Position, _bb)); }
  public void __init(int _i, ByteBuffer _bb) { __p.bb_pos = _i; __p.bb = _bb; }
  public Stat __assign(int _i, ByteBuffer _bb) { __init(_i, _bb); return this; }

  public string Id { get { int o = __p.__offset(4); return o != 0 ? __p.__string(o + __p.bb_pos) : null; } }
#if ENABLE_SPAN_T
  public Span<byte> GetIdBytes() { return __p.__vector_as_span(4); }
#else
  public ArraySegment<byte>? GetIdBytes() { return __p.__vector_as_arraysegment(4); }
#endif
  public byte[] GetIdArray() { return __p.__vector_as_array<byte>(4); }
  public long Val { get { int o = __p.__offset(6); return o != 0 ? __p.bb.GetLong(o + __p.bb_pos) : (long)0; } }
  public bool MutateVal(long val) { int o = __p.__offset(6); if (o != 0) { __p.bb.PutLong(o + __p.bb_pos, val); return true; } else { return false; } }
  public ushort Count { get { int o = __p.__offset(8); return o != 0 ? __p.bb.GetUshort(o + __p.bb_pos) : (ushort)0; } }
  public bool MutateCount(ushort count) { int o = __p.__offset(8); if (o != 0) { __p.bb.PutUshort(o + __p.bb_pos, count); return true; } else { return false; } }

  public static Offset<Stat> CreateStat(FlatBufferBuilder builder,
      StringOffset idOffset = default(StringOffset),
      long val = 0,
      ushort count = 0) {
    builder.StartObject(3);
    Stat.AddVal(builder, val);
    Stat.AddId(builder, idOffset);
    Stat.AddCount(builder, count);
    return Stat.EndStat(builder);
  }

  public static void StartStat(FlatBufferBuilder builder) { builder.StartObject(3); }
  public static void AddId(FlatBufferBuilder builder, StringOffset idOffset) { builder.AddOffset(0, idOffset.Value, 0); }
  public static void AddVal(FlatBufferBuilder builder, long val) { builder.AddLong(1, val, 0); }
  public static void AddCount(FlatBufferBuilder builder, ushort count) { builder.AddUshort(2, count, 0); }
  public static Offset<Stat> EndStat(FlatBufferBuilder builder) {
    int o = builder.EndObject();
    return new Offset<Stat>(o);
  }
};


}
