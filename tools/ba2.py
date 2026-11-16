import struct,zlib,sys,os
p,out=sys.argv[1],sys.argv[2]
d=open(p,"rb").read()
mag,ver,typ,n,nto=struct.unpack_from("<4sI4sIQ",d,0)
assert mag==b"BTDX" and typ==b"GNRL",(mag,typ)
recs=[struct.unpack_from("<I4sIIQIII",d,24+36*i) for i in range(n)]
o=nto; names=[]
for i in range(n):
    l=struct.unpack_from("<H",d,o)[0]; names.append(d[o+2:o+2+l].decode()); o+=2+l
for (nh,ext,dh,fl,off,ps,us,al),nm in zip(recs,names):
    data=d[off:off+ps] if ps else d[off:off+us]
    if ps: data=zlib.decompress(data)
    fp=os.path.join(out,nm.replace("\\","/")); os.makedirs(os.path.dirname(fp),exist_ok=True)
    open(fp,"wb").write(data); print(nm,len(data))
