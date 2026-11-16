# Catalog every DXBC blob inside a Bethesda .fxp: stage, input/output signatures, hash. Writes only metadata.
import struct,sys,hashlib,collections,csv
STAGE={0:"PS",1:"VS",2:"GS",3:"HS",4:"DS",5:"CS"}
def sig(b,off):
    n,_=struct.unpack_from("<II",b,off); out=[]
    for i in range(n):
        name_off,idx,sv,ct,reg,mask=struct.unpack_from("<IIIIIB",b,off+8+24*i)
        nm=b[off+name_off:b.index(b"\0",off+name_off)].decode()
        out.append(f"{nm}{idx}")
    return out
def parse(b):
    n=struct.unpack_from("<I",b,28)[0]; info={"isgn":[],"osgn":[],"stage":"?"}
    for i in range(n):
        co=struct.unpack_from("<I",b,32+4*i)[0]; fc=b[co:co+4]; body=co+8
        if fc in (b"ISGN",b"ISG1"): info["isgn"]=sig(b,body) if fc==b"ISGN" else ["ISG1"]
        elif fc in (b"OSGN",b"OSG5",b"OSG1"): info["osgn"]=sig(b,body) if fc==b"OSGN" else [fc.decode()]
        elif fc in (b"SHEX",b"SHDR"):
            tok=struct.unpack_from("<I",b,body)[0]; info["stage"]=STAGE.get(tok>>16,"?")
    return info
d=open(sys.argv[1],"rb").read(); rows=[]; i=0
while True:
    i=d.find(b"DXBC",i)
    if i<0: break
    size=struct.unpack_from("<I",d,i+24)[0]; b=d[i:i+size]
    try: info=parse(b)
    except Exception as e: i+=4; continue
    rows.append(dict(offset=i,size=size,sha1=hashlib.sha1(b).hexdigest()[:16],stage=info["stage"],
        isgn=" ".join(info["isgn"]),osgn=" ".join(info["osgn"]),eye="EYEINDEX0" in info["isgn"] or "EYEINDEX0" in info["osgn"]))
    i+=size
with open(sys.argv[2],"w",newline="") as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
c=collections.Counter(r["stage"] for r in rows); print("blobs",len(rows),dict(c))
ps=[r for r in rows if r["stage"]=="PS"]
print("PS layouts",len(set(r["isgn"] for r in ps)),"PS with EYEINDEX",sum(r["eye"] for r in ps))
vs=[r for r in rows if r["stage"]=="VS"]; print("VS with EYEINDEX out",sum(r["eye"] for r in vs),"of",len(vs))
oc=collections.Counter(sum(1 for t in r["osgn"].split() if t.startswith("SV_Target")) for r in ps); print("PS by #SV_Target",dict(sorted(oc.items())))
print("unique blobs",len(set(r["sha1"] for r in rows)))
