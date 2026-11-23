import struct,sys
def sstr(b,p):
    n=b[p]; return b[p+1:p+1+n],p+1+n
def parse(b):
    p=b.index(b'\n')+1
    ver,endian,uv,nblocks,bsver=struct.unpack_from('<IBIII',b,p); p+=17
    assert ver==0x14020007 and bsver==130,(hex(ver),bsver)
    for _ in range(4): _,p=sstr(b,p)
    nt=struct.unpack_from('<H',b,p)[0]; p+=2
    types=[]
    for _ in range(nt):
        n=struct.unpack_from('<I',b,p)[0]; types.append(b[p+4:p+4+n].decode()); p+=4+n
    tidx=struct.unpack_from('<%dH'%nblocks,b,p); p+=2*nblocks
    sizes_off=p
    sizes=struct.unpack_from('<%dI'%nblocks,b,p); p+=4*nblocks
    ns,ml=struct.unpack_from('<II',b,p); p+=8
    strs=[]
    for _ in range(ns):
        n=struct.unpack_from('<I',b,p)[0]; strs.append(b[p+4:p+4+n].decode('latin1')); p+=4+n
    ng=struct.unpack_from('<I',b,p)[0]; p+=4+4*ng
    offs=[]
    for s in sizes: offs.append(p); p+=s
    return dict(types=types,tidx=tidx,sizes=sizes,sizes_off=sizes_off,strs=strs,offs=offs,end=p,nblocks=nblocks)
def shape(b,o):
    p=o
    name,nx=struct.unpack_from('<iI',b,p); p+=8+4*nx
    ctrl,flags=struct.unpack_from('<iI',b,p); p+=8
    tr_off=p; tr=struct.unpack_from('<3f',b,p); p+=12+36
    scale_off=p; scale=struct.unpack_from('<f',b,p)[0]; p+=4
    coll=struct.unpack_from('<i',b,p)[0]; p+=4
    bs=struct.unpack_from('<4f',b,p); p+=16
    skin,shader,alpha=struct.unpack_from('<3i',b,p); p+=12
    vd,ntri,nv,ds=struct.unpack_from('<QIHI',b,p); geo_off=p+8; p+=18
    return dict(name=name,flags=flags,tr=tr,tr_off=tr_off,scale=scale,scale_off=scale_off,bs=bs,shader=shader,alpha=alpha,vd=hex(vd),ntri=ntri,nv=nv,ds=ds,geo_off=geo_off,data_off=p)
if __name__=='__main__':
    for f in sys.argv[1:]:
        b=open(f,'rb').read(); h=parse(b)
        print(f.split('/')[-1],len(b),h['end'],h['strs'])
        for i in range(h['nblocks']):
            t=h['types'][h['tidx'][i]]; print(' ',i,t,h['sizes'][i],end=' ')
            if 'TriShape' in t:
                s=shape(b,h['offs'][i]); print(h['strs'][s['name']] if s['name']>=0 else None,{k:s[k] for k in('flags','tr','scale','bs','shader','vd','ntri','nv','ds')}, 'rest',h['sizes'][i]-(s['data_off']-h['offs'][i])-s['ds'],end='')
            print()
