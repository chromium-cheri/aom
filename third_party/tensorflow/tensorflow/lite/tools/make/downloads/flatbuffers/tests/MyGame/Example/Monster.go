// Code generated by the FlatBuffers compiler. DO NOT EDIT.

package Example

import (
	flatbuffers "github.com/google/flatbuffers/go"

	MyGame "MyGame"
)

/// an example documentation comment: monster object
type Monster struct {
	_tab flatbuffers.Table
}

func GetRootAsMonster(buf []byte, offset flatbuffers.UOffsetT) *Monster {
	n := flatbuffers.GetUOffsetT(buf[offset:])
	x := &Monster{}
	x.Init(buf, n+offset)
	return x
}

func (rcv *Monster) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *Monster) Table() flatbuffers.Table {
	return rcv._tab
}

func (rcv *Monster) Pos(obj *Vec3) *Vec3 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		x := o + rcv._tab.Pos
		if obj == nil {
			obj = new(Vec3)
		}
		obj.Init(rcv._tab.Bytes, x)
		return obj
	}
	return nil
}

func (rcv *Monster) Mana() int16 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(6))
	if o != 0 {
		return rcv._tab.GetInt16(o + rcv._tab.Pos)
	}
	return 150
}

func (rcv *Monster) MutateMana(n int16) bool {
	return rcv._tab.MutateInt16Slot(6, n)
}

func (rcv *Monster) Hp() int16 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(8))
	if o != 0 {
		return rcv._tab.GetInt16(o + rcv._tab.Pos)
	}
	return 100
}

func (rcv *Monster) MutateHp(n int16) bool {
	return rcv._tab.MutateInt16Slot(8, n)
}

func (rcv *Monster) Name() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(10))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *Monster) Inventory(j int) byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(14))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.GetByte(a + flatbuffers.UOffsetT(j*1))
	}
	return 0
}

func (rcv *Monster) InventoryLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(14))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) InventoryBytes() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(14))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *Monster) MutateInventory(j int, n byte) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(14))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.MutateByte(a+flatbuffers.UOffsetT(j*1), n)
	}
	return false
}

func (rcv *Monster) Color() Color {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(16))
	if o != 0 {
		return rcv._tab.GetInt8(o + rcv._tab.Pos)
	}
	return 8
}

func (rcv *Monster) MutateColor(n Color) bool {
	return rcv._tab.MutateInt8Slot(16, n)
}

func (rcv *Monster) TestType() byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(18))
	if o != 0 {
		return rcv._tab.GetByte(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateTestType(n byte) bool {
	return rcv._tab.MutateByteSlot(18, n)
}

func (rcv *Monster) Test(obj *flatbuffers.Table) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(20))
	if o != 0 {
		rcv._tab.Union(obj, o)
		return true
	}
	return false
}

func (rcv *Monster) Test4(obj *Test, j int) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(22))
	if o != 0 {
		x := rcv._tab.Vector(o)
		x += flatbuffers.UOffsetT(j) * 4
		obj.Init(rcv._tab.Bytes, x)
		return true
	}
	return false
}

func (rcv *Monster) Test4Length() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(22))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) Testarrayofstring(j int) []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(24))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.ByteVector(a + flatbuffers.UOffsetT(j*4))
	}
	return nil
}

func (rcv *Monster) TestarrayofstringLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(24))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

/// an example documentation comment: this will end up in the generated code
/// multiline too
func (rcv *Monster) Testarrayoftables(obj *Monster, j int) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(26))
	if o != 0 {
		x := rcv._tab.Vector(o)
		x += flatbuffers.UOffsetT(j) * 4
		x = rcv._tab.Indirect(x)
		obj.Init(rcv._tab.Bytes, x)
		return true
	}
	return false
}

func (rcv *Monster) TestarrayoftablesLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(26))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

/// an example documentation comment: this will end up in the generated code
/// multiline too
func (rcv *Monster) Enemy(obj *Monster) *Monster {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(28))
	if o != 0 {
		x := rcv._tab.Indirect(o + rcv._tab.Pos)
		if obj == nil {
			obj = new(Monster)
		}
		obj.Init(rcv._tab.Bytes, x)
		return obj
	}
	return nil
}

func (rcv *Monster) Testnestedflatbuffer(j int) byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(30))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.GetByte(a + flatbuffers.UOffsetT(j*1))
	}
	return 0
}

func (rcv *Monster) TestnestedflatbufferLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(30))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) TestnestedflatbufferBytes() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(30))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *Monster) MutateTestnestedflatbuffer(j int, n byte) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(30))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.MutateByte(a+flatbuffers.UOffsetT(j*1), n)
	}
	return false
}

func (rcv *Monster) Testempty(obj *Stat) *Stat {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(32))
	if o != 0 {
		x := rcv._tab.Indirect(o + rcv._tab.Pos)
		if obj == nil {
			obj = new(Stat)
		}
		obj.Init(rcv._tab.Bytes, x)
		return obj
	}
	return nil
}

func (rcv *Monster) Testbool() bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(34))
	if o != 0 {
		return rcv._tab.GetBool(o + rcv._tab.Pos)
	}
	return false
}

func (rcv *Monster) MutateTestbool(n bool) bool {
	return rcv._tab.MutateBoolSlot(34, n)
}

func (rcv *Monster) Testhashs32Fnv1() int32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(36))
	if o != 0 {
		return rcv._tab.GetInt32(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateTesthashs32Fnv1(n int32) bool {
	return rcv._tab.MutateInt32Slot(36, n)
}

func (rcv *Monster) Testhashu32Fnv1() uint32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(38))
	if o != 0 {
		return rcv._tab.GetUint32(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateTesthashu32Fnv1(n uint32) bool {
	return rcv._tab.MutateUint32Slot(38, n)
}

func (rcv *Monster) Testhashs64Fnv1() int64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(40))
	if o != 0 {
		return rcv._tab.GetInt64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateTesthashs64Fnv1(n int64) bool {
	return rcv._tab.MutateInt64Slot(40, n)
}

func (rcv *Monster) Testhashu64Fnv1() uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(42))
	if o != 0 {
		return rcv._tab.GetUint64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateTesthashu64Fnv1(n uint64) bool {
	return rcv._tab.MutateUint64Slot(42, n)
}

func (rcv *Monster) Testhashs32Fnv1a() int32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(44))
	if o != 0 {
		return rcv._tab.GetInt32(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateTesthashs32Fnv1a(n int32) bool {
	return rcv._tab.MutateInt32Slot(44, n)
}

func (rcv *Monster) Testhashu32Fnv1a() uint32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(46))
	if o != 0 {
		return rcv._tab.GetUint32(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateTesthashu32Fnv1a(n uint32) bool {
	return rcv._tab.MutateUint32Slot(46, n)
}

func (rcv *Monster) Testhashs64Fnv1a() int64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(48))
	if o != 0 {
		return rcv._tab.GetInt64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateTesthashs64Fnv1a(n int64) bool {
	return rcv._tab.MutateInt64Slot(48, n)
}

func (rcv *Monster) Testhashu64Fnv1a() uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(50))
	if o != 0 {
		return rcv._tab.GetUint64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateTesthashu64Fnv1a(n uint64) bool {
	return rcv._tab.MutateUint64Slot(50, n)
}

func (rcv *Monster) Testarrayofbools(j int) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(52))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.GetBool(a + flatbuffers.UOffsetT(j*1))
	}
	return false
}

func (rcv *Monster) TestarrayofboolsLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(52))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) MutateTestarrayofbools(j int, n bool) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(52))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.MutateBool(a+flatbuffers.UOffsetT(j*1), n)
	}
	return false
}

func (rcv *Monster) Testf() float32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(54))
	if o != 0 {
		return rcv._tab.GetFloat32(o + rcv._tab.Pos)
	}
	return 3.14159
}

func (rcv *Monster) MutateTestf(n float32) bool {
	return rcv._tab.MutateFloat32Slot(54, n)
}

func (rcv *Monster) Testf2() float32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(56))
	if o != 0 {
		return rcv._tab.GetFloat32(o + rcv._tab.Pos)
	}
	return 3.0
}

func (rcv *Monster) MutateTestf2(n float32) bool {
	return rcv._tab.MutateFloat32Slot(56, n)
}

func (rcv *Monster) Testf3() float32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(58))
	if o != 0 {
		return rcv._tab.GetFloat32(o + rcv._tab.Pos)
	}
	return 0.0
}

func (rcv *Monster) MutateTestf3(n float32) bool {
	return rcv._tab.MutateFloat32Slot(58, n)
}

func (rcv *Monster) Testarrayofstring2(j int) []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(60))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.ByteVector(a + flatbuffers.UOffsetT(j*4))
	}
	return nil
}

func (rcv *Monster) Testarrayofstring2Length() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(60))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) Testarrayofsortedstruct(obj *Ability, j int) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(62))
	if o != 0 {
		x := rcv._tab.Vector(o)
		x += flatbuffers.UOffsetT(j) * 8
		obj.Init(rcv._tab.Bytes, x)
		return true
	}
	return false
}

func (rcv *Monster) TestarrayofsortedstructLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(62))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) Flex(j int) byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(64))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.GetByte(a + flatbuffers.UOffsetT(j*1))
	}
	return 0
}

func (rcv *Monster) FlexLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(64))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) FlexBytes() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(64))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func (rcv *Monster) MutateFlex(j int, n byte) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(64))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.MutateByte(a+flatbuffers.UOffsetT(j*1), n)
	}
	return false
}

func (rcv *Monster) Test5(obj *Test, j int) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(66))
	if o != 0 {
		x := rcv._tab.Vector(o)
		x += flatbuffers.UOffsetT(j) * 4
		obj.Init(rcv._tab.Bytes, x)
		return true
	}
	return false
}

func (rcv *Monster) Test5Length() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(66))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) VectorOfLongs(j int) int64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(68))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.GetInt64(a + flatbuffers.UOffsetT(j*8))
	}
	return 0
}

func (rcv *Monster) VectorOfLongsLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(68))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) MutateVectorOfLongs(j int, n int64) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(68))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.MutateInt64(a+flatbuffers.UOffsetT(j*8), n)
	}
	return false
}

func (rcv *Monster) VectorOfDoubles(j int) float64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(70))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.GetFloat64(a + flatbuffers.UOffsetT(j*8))
	}
	return 0
}

func (rcv *Monster) VectorOfDoublesLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(70))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) MutateVectorOfDoubles(j int, n float64) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(70))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.MutateFloat64(a+flatbuffers.UOffsetT(j*8), n)
	}
	return false
}

func (rcv *Monster) ParentNamespaceTest(obj *MyGame.InParentNamespace) *MyGame.InParentNamespace {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(72))
	if o != 0 {
		x := rcv._tab.Indirect(o + rcv._tab.Pos)
		if obj == nil {
			obj = new(MyGame.InParentNamespace)
		}
		obj.Init(rcv._tab.Bytes, x)
		return obj
	}
	return nil
}

func (rcv *Monster) VectorOfReferrables(obj *Referrable, j int) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(74))
	if o != 0 {
		x := rcv._tab.Vector(o)
		x += flatbuffers.UOffsetT(j) * 4
		x = rcv._tab.Indirect(x)
		obj.Init(rcv._tab.Bytes, x)
		return true
	}
	return false
}

func (rcv *Monster) VectorOfReferrablesLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(74))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) SingleWeakReference() uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(76))
	if o != 0 {
		return rcv._tab.GetUint64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateSingleWeakReference(n uint64) bool {
	return rcv._tab.MutateUint64Slot(76, n)
}

func (rcv *Monster) VectorOfWeakReferences(j int) uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(78))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.GetUint64(a + flatbuffers.UOffsetT(j*8))
	}
	return 0
}

func (rcv *Monster) VectorOfWeakReferencesLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(78))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) MutateVectorOfWeakReferences(j int, n uint64) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(78))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.MutateUint64(a+flatbuffers.UOffsetT(j*8), n)
	}
	return false
}

func (rcv *Monster) VectorOfStrongReferrables(obj *Referrable, j int) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(80))
	if o != 0 {
		x := rcv._tab.Vector(o)
		x += flatbuffers.UOffsetT(j) * 4
		x = rcv._tab.Indirect(x)
		obj.Init(rcv._tab.Bytes, x)
		return true
	}
	return false
}

func (rcv *Monster) VectorOfStrongReferrablesLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(80))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) CoOwningReference() uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(82))
	if o != 0 {
		return rcv._tab.GetUint64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateCoOwningReference(n uint64) bool {
	return rcv._tab.MutateUint64Slot(82, n)
}

func (rcv *Monster) VectorOfCoOwningReferences(j int) uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(84))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.GetUint64(a + flatbuffers.UOffsetT(j*8))
	}
	return 0
}

func (rcv *Monster) VectorOfCoOwningReferencesLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(84))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) MutateVectorOfCoOwningReferences(j int, n uint64) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(84))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.MutateUint64(a+flatbuffers.UOffsetT(j*8), n)
	}
	return false
}

func (rcv *Monster) NonOwningReference() uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(86))
	if o != 0 {
		return rcv._tab.GetUint64(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateNonOwningReference(n uint64) bool {
	return rcv._tab.MutateUint64Slot(86, n)
}

func (rcv *Monster) VectorOfNonOwningReferences(j int) uint64 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(88))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.GetUint64(a + flatbuffers.UOffsetT(j*8))
	}
	return 0
}

func (rcv *Monster) VectorOfNonOwningReferencesLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(88))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) MutateVectorOfNonOwningReferences(j int, n uint64) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(88))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.MutateUint64(a+flatbuffers.UOffsetT(j*8), n)
	}
	return false
}

func (rcv *Monster) AnyUniqueType() byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(90))
	if o != 0 {
		return rcv._tab.GetByte(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateAnyUniqueType(n byte) bool {
	return rcv._tab.MutateByteSlot(90, n)
}

func (rcv *Monster) AnyUnique(obj *flatbuffers.Table) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(92))
	if o != 0 {
		rcv._tab.Union(obj, o)
		return true
	}
	return false
}

func (rcv *Monster) AnyAmbiguousType() byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(94))
	if o != 0 {
		return rcv._tab.GetByte(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Monster) MutateAnyAmbiguousType(n byte) bool {
	return rcv._tab.MutateByteSlot(94, n)
}

func (rcv *Monster) AnyAmbiguous(obj *flatbuffers.Table) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(96))
	if o != 0 {
		rcv._tab.Union(obj, o)
		return true
	}
	return false
}

func (rcv *Monster) VectorOfEnums(j int) Color {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(98))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.GetInt8(a + flatbuffers.UOffsetT(j*1))
	}
	return 0
}

func (rcv *Monster) VectorOfEnumsLength() int {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(98))
	if o != 0 {
		return rcv._tab.VectorLen(o)
	}
	return 0
}

func (rcv *Monster) MutateVectorOfEnums(j int, n Color) bool {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(98))
	if o != 0 {
		a := rcv._tab.Vector(o)
		return rcv._tab.MutateInt8(a+flatbuffers.UOffsetT(j*1), n)
	}
	return false
}

func MonsterStart(builder *flatbuffers.Builder) {
	builder.StartObject(48)
}
func MonsterAddPos(builder *flatbuffers.Builder, pos flatbuffers.UOffsetT) {
	builder.PrependStructSlot(0, flatbuffers.UOffsetT(pos), 0)
}
func MonsterAddMana(builder *flatbuffers.Builder, mana int16) {
	builder.PrependInt16Slot(1, mana, 150)
}
func MonsterAddHp(builder *flatbuffers.Builder, hp int16) {
	builder.PrependInt16Slot(2, hp, 100)
}
func MonsterAddName(builder *flatbuffers.Builder, name flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(3, flatbuffers.UOffsetT(name), 0)
}
func MonsterAddInventory(builder *flatbuffers.Builder, inventory flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(5, flatbuffers.UOffsetT(inventory), 0)
}
func MonsterStartInventoryVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(1, numElems, 1)
}
func MonsterAddColor(builder *flatbuffers.Builder, color int8) {
	builder.PrependInt8Slot(6, color, 8)
}
func MonsterAddTestType(builder *flatbuffers.Builder, testType byte) {
	builder.PrependByteSlot(7, testType, 0)
}
func MonsterAddTest(builder *flatbuffers.Builder, test flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(8, flatbuffers.UOffsetT(test), 0)
}
func MonsterAddTest4(builder *flatbuffers.Builder, test4 flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(9, flatbuffers.UOffsetT(test4), 0)
}
func MonsterStartTest4Vector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(4, numElems, 2)
}
func MonsterAddTestarrayofstring(builder *flatbuffers.Builder, testarrayofstring flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(10, flatbuffers.UOffsetT(testarrayofstring), 0)
}
func MonsterStartTestarrayofstringVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(4, numElems, 4)
}
func MonsterAddTestarrayoftables(builder *flatbuffers.Builder, testarrayoftables flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(11, flatbuffers.UOffsetT(testarrayoftables), 0)
}
func MonsterStartTestarrayoftablesVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(4, numElems, 4)
}
func MonsterAddEnemy(builder *flatbuffers.Builder, enemy flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(12, flatbuffers.UOffsetT(enemy), 0)
}
func MonsterAddTestnestedflatbuffer(builder *flatbuffers.Builder, testnestedflatbuffer flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(13, flatbuffers.UOffsetT(testnestedflatbuffer), 0)
}
func MonsterStartTestnestedflatbufferVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(1, numElems, 1)
}
func MonsterAddTestempty(builder *flatbuffers.Builder, testempty flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(14, flatbuffers.UOffsetT(testempty), 0)
}
func MonsterAddTestbool(builder *flatbuffers.Builder, testbool bool) {
	builder.PrependBoolSlot(15, testbool, false)
}
func MonsterAddTesthashs32Fnv1(builder *flatbuffers.Builder, testhashs32Fnv1 int32) {
	builder.PrependInt32Slot(16, testhashs32Fnv1, 0)
}
func MonsterAddTesthashu32Fnv1(builder *flatbuffers.Builder, testhashu32Fnv1 uint32) {
	builder.PrependUint32Slot(17, testhashu32Fnv1, 0)
}
func MonsterAddTesthashs64Fnv1(builder *flatbuffers.Builder, testhashs64Fnv1 int64) {
	builder.PrependInt64Slot(18, testhashs64Fnv1, 0)
}
func MonsterAddTesthashu64Fnv1(builder *flatbuffers.Builder, testhashu64Fnv1 uint64) {
	builder.PrependUint64Slot(19, testhashu64Fnv1, 0)
}
func MonsterAddTesthashs32Fnv1a(builder *flatbuffers.Builder, testhashs32Fnv1a int32) {
	builder.PrependInt32Slot(20, testhashs32Fnv1a, 0)
}
func MonsterAddTesthashu32Fnv1a(builder *flatbuffers.Builder, testhashu32Fnv1a uint32) {
	builder.PrependUint32Slot(21, testhashu32Fnv1a, 0)
}
func MonsterAddTesthashs64Fnv1a(builder *flatbuffers.Builder, testhashs64Fnv1a int64) {
	builder.PrependInt64Slot(22, testhashs64Fnv1a, 0)
}
func MonsterAddTesthashu64Fnv1a(builder *flatbuffers.Builder, testhashu64Fnv1a uint64) {
	builder.PrependUint64Slot(23, testhashu64Fnv1a, 0)
}
func MonsterAddTestarrayofbools(builder *flatbuffers.Builder, testarrayofbools flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(24, flatbuffers.UOffsetT(testarrayofbools), 0)
}
func MonsterStartTestarrayofboolsVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(1, numElems, 1)
}
func MonsterAddTestf(builder *flatbuffers.Builder, testf float32) {
	builder.PrependFloat32Slot(25, testf, 3.14159)
}
func MonsterAddTestf2(builder *flatbuffers.Builder, testf2 float32) {
	builder.PrependFloat32Slot(26, testf2, 3.0)
}
func MonsterAddTestf3(builder *flatbuffers.Builder, testf3 float32) {
	builder.PrependFloat32Slot(27, testf3, 0.0)
}
func MonsterAddTestarrayofstring2(builder *flatbuffers.Builder, testarrayofstring2 flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(28, flatbuffers.UOffsetT(testarrayofstring2), 0)
}
func MonsterStartTestarrayofstring2Vector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(4, numElems, 4)
}
func MonsterAddTestarrayofsortedstruct(builder *flatbuffers.Builder, testarrayofsortedstruct flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(29, flatbuffers.UOffsetT(testarrayofsortedstruct), 0)
}
func MonsterStartTestarrayofsortedstructVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(8, numElems, 4)
}
func MonsterAddFlex(builder *flatbuffers.Builder, flex flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(30, flatbuffers.UOffsetT(flex), 0)
}
func MonsterStartFlexVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(1, numElems, 1)
}
func MonsterAddTest5(builder *flatbuffers.Builder, test5 flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(31, flatbuffers.UOffsetT(test5), 0)
}
func MonsterStartTest5Vector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(4, numElems, 2)
}
func MonsterAddVectorOfLongs(builder *flatbuffers.Builder, vectorOfLongs flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(32, flatbuffers.UOffsetT(vectorOfLongs), 0)
}
func MonsterStartVectorOfLongsVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(8, numElems, 8)
}
func MonsterAddVectorOfDoubles(builder *flatbuffers.Builder, vectorOfDoubles flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(33, flatbuffers.UOffsetT(vectorOfDoubles), 0)
}
func MonsterStartVectorOfDoublesVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(8, numElems, 8)
}
func MonsterAddParentNamespaceTest(builder *flatbuffers.Builder, parentNamespaceTest flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(34, flatbuffers.UOffsetT(parentNamespaceTest), 0)
}
func MonsterAddVectorOfReferrables(builder *flatbuffers.Builder, vectorOfReferrables flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(35, flatbuffers.UOffsetT(vectorOfReferrables), 0)
}
func MonsterStartVectorOfReferrablesVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(4, numElems, 4)
}
func MonsterAddSingleWeakReference(builder *flatbuffers.Builder, singleWeakReference uint64) {
	builder.PrependUint64Slot(36, singleWeakReference, 0)
}
func MonsterAddVectorOfWeakReferences(builder *flatbuffers.Builder, vectorOfWeakReferences flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(37, flatbuffers.UOffsetT(vectorOfWeakReferences), 0)
}
func MonsterStartVectorOfWeakReferencesVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(8, numElems, 8)
}
func MonsterAddVectorOfStrongReferrables(builder *flatbuffers.Builder, vectorOfStrongReferrables flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(38, flatbuffers.UOffsetT(vectorOfStrongReferrables), 0)
}
func MonsterStartVectorOfStrongReferrablesVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(4, numElems, 4)
}
func MonsterAddCoOwningReference(builder *flatbuffers.Builder, coOwningReference uint64) {
	builder.PrependUint64Slot(39, coOwningReference, 0)
}
func MonsterAddVectorOfCoOwningReferences(builder *flatbuffers.Builder, vectorOfCoOwningReferences flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(40, flatbuffers.UOffsetT(vectorOfCoOwningReferences), 0)
}
func MonsterStartVectorOfCoOwningReferencesVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(8, numElems, 8)
}
func MonsterAddNonOwningReference(builder *flatbuffers.Builder, nonOwningReference uint64) {
	builder.PrependUint64Slot(41, nonOwningReference, 0)
}
func MonsterAddVectorOfNonOwningReferences(builder *flatbuffers.Builder, vectorOfNonOwningReferences flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(42, flatbuffers.UOffsetT(vectorOfNonOwningReferences), 0)
}
func MonsterStartVectorOfNonOwningReferencesVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(8, numElems, 8)
}
func MonsterAddAnyUniqueType(builder *flatbuffers.Builder, anyUniqueType byte) {
	builder.PrependByteSlot(43, anyUniqueType, 0)
}
func MonsterAddAnyUnique(builder *flatbuffers.Builder, anyUnique flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(44, flatbuffers.UOffsetT(anyUnique), 0)
}
func MonsterAddAnyAmbiguousType(builder *flatbuffers.Builder, anyAmbiguousType byte) {
	builder.PrependByteSlot(45, anyAmbiguousType, 0)
}
func MonsterAddAnyAmbiguous(builder *flatbuffers.Builder, anyAmbiguous flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(46, flatbuffers.UOffsetT(anyAmbiguous), 0)
}
func MonsterAddVectorOfEnums(builder *flatbuffers.Builder, vectorOfEnums flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(47, flatbuffers.UOffsetT(vectorOfEnums), 0)
}
func MonsterStartVectorOfEnumsVector(builder *flatbuffers.Builder, numElems int) flatbuffers.UOffsetT {
	return builder.StartVector(1, numElems, 1)
}
func MonsterEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT {
	return builder.EndObject()
}
