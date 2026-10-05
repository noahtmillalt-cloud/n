import zlib, struct
d=open('QuestItemList.swf','rb').read()
body=bytearray(zlib.decompress(d[8:]))
# Pattern inside Update(): ... GetMember(_alpha)=4E, PushDup=4C, Not=12, If 21 = 9D 02 00 15 00
pat=bytes([0x4E,0x4C,0x12,0x9D,0x02,0x00,0x15,0x00])
n=body.count(pat); print('matches',n)
assert n==1
i=body.find(pat)+3
body[i:i+5]=bytes([0x99,0x02,0x00,0x00,0x00])   # If -> Jump +0 (never skips the compass-facing check)
new=d[:3]+d[3:4]+struct.pack('<I',len(body)+8)+zlib.compress(bytes(body),9)
open('QHT_QuestItemList.swf','wb').write(new)
print(len(new))
