-- automatically generated by the FlatBuffers compiler, do not modify

-- namespace: NamespaceA

local flatbuffers = require('flatbuffers')

local SecondTableInA = {} -- the module
local SecondTableInA_mt = {} -- the class metatable

function SecondTableInA.New()
    local o = {}
    setmetatable(o, {__index = SecondTableInA_mt})
    return o
end
function SecondTableInA.GetRootAsSecondTableInA(buf, offset)
    local n = flatbuffers.N.UOffsetT:Unpack(buf, offset)
    local o = SecondTableInA.New()
    o:Init(buf, n + offset)
    return o
end
function SecondTableInA_mt:Init(buf, pos)
    self.view = flatbuffers.view.New(buf, pos)
end
function SecondTableInA_mt:ReferToC()
    local o = self.view:Offset(4)
    if o ~= 0 then
        local x = self.view:Indirect(o + self.view.pos)
        local obj = require('NamespaceC.TableInC').New()
        obj:Init(self.view.bytes, x)
        return obj
    end
end
function SecondTableInA.Start(builder) builder:StartObject(1) end
function SecondTableInA.AddReferToC(builder, referToC) builder:PrependUOffsetTRelativeSlot(0, referToC, 0) end
function SecondTableInA.End(builder) return builder:EndObject() end

return SecondTableInA -- return the module