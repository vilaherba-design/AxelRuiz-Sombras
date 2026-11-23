#!/usr/bin/env python3
"""Strip the WATER node from FO4 terrain LOD meshes (.btr) made by xLODGen / vanilla.
Usage: lod_sin_agua.py <src meshes/terrain dir> <dst meshes/terrain dir>
Writes only the files that had water; files with an unexpected layout are reported and left out."""
import os, struct, sys

def sstr(b, p):
    return p + 1 + b[p]

def strip(b):
    """Returns new bytes, or None if the file has no WATER node. Raises ValueError on unknown layout."""
    p0 = b.index(b'\n') + 1
    ver, endian, uv, nblocks, bsver = struct.unpack_from('<IBIII', b, p0)
    if ver != 0x14020007 or bsver != 130 or endian != 1:
        raise ValueError('version')
    p = p0 + 17
    for _ in range(4):
        p = sstr(b, p)
    types_off = p
    nt = struct.unpack_from('<H', b, p)[0]; p += 2
    types = []
    for _ in range(nt):
        n = struct.unpack_from('<I', b, p)[0]; types.append(b[p + 4:p + 4 + n]); p += 4 + n
    tidx = list(struct.unpack_from('<%dH' % nblocks, b, p)); p += 2 * nblocks
    sizes = list(struct.unpack_from('<%dI' % nblocks, b, p)); p += 4 * nblocks
    strs_off = p
    ns, ml = struct.unpack_from('<II', b, p); p += 8
    strs = []
    for _ in range(ns):
        n = struct.unpack_from('<I', b, p)[0]; strs.append(b[p + 4:p + 4 + n]); p += 4 + n
    ng = struct.unpack_from('<I', b, p)[0]; p += 4 + 4 * ng
    blocks_off = p
    blocks = []
    for s in sizes:
        blocks.append(b[p:p + s]); p += s
    footer = b[p:]
    if footer != struct.pack('<Ii', 1, 0):
        raise ValueError('footer')
    tn = [types[i] for i in tidx]

    def node(blk):  # BSMultiBoundNode, no extra data
        name, nx = struct.unpack_from('<iI', blk, 0)
        if nx != 0: raise ValueError('node extra data')
        nch = struct.unpack_from('<I', blk, 72)[0]
        ch = list(struct.unpack_from('<%di' % nch, blk, 76))
        mb, cull = struct.unpack_from('<iI', blk, 76 + 4 * nch)
        if len(blk) != 84 + 4 * nch: raise ValueError('node size')
        return name, ch, mb, cull

    if tn[0] != b'BSMultiBoundNode': raise ValueError('root type')
    rname, rch, rmb, rcull = node(blocks[0])
    water = [c for c in rch if c >= 0 and tn[c] == b'BSMultiBoundNode']
    if not water:
        return None
    if len(water) != 1: raise ValueError('several child nodes')
    w = water[0]
    wname, wch, wmb, wcull = node(blocks[w])
    if wname < 0 or strs[wname] != b'WATER': raise ValueError('child node is not WATER')
    remove = {w}
    for c in wch:
        if tn[c] not in (b'BSTriShape', b'BSSubIndexTriShape'): raise ValueError('water child type')
        remove.add(c)
        nx = struct.unpack_from('<I', blocks[c], 4)[0]
        if nx != 0: raise ValueError('shape extra data')
        coll = struct.unpack_from('<i', blocks[c], 68)[0]
        skin, shader, alpha = struct.unpack_from('<3i', blocks[c], 88)
        if coll >= 0 or skin >= 0: raise ValueError('water shape refs')
        for r in (shader, alpha):
            if r >= 0:
                if tn[r] not in (b'BSEffectShaderProperty', b'BSWaterShaderProperty', b'NiAlphaProperty'): raise ValueError('water prop type')
                remove.add(r)
    if wmb >= 0:
        if tn[wmb] != b'BSMultiBound' or len(blocks[wmb]) != 4: raise ValueError('water multibound')
        remove.add(wmb)
        d = struct.unpack_from('<i', blocks[wmb], 0)[0]
        if d >= 0: remove.add(d)
    first = min(remove)
    keep = [i for i in range(nblocks) if i not in remove]
    remap = {old: new for new, old in enumerate(keep)}
    out = []
    for i in keep:
        blk = blocks[i]
        if i == 0:
            ch = [c for c in rch if c != w]
            if any(c >= first for c in ch): raise ValueError('root child after water')
            if rmb >= 0 and rmb not in remap: raise ValueError('root multibound removed')
            blk = blk[:72] + struct.pack('<I', len(ch)) + struct.pack('<%di' % len(ch), *ch) + struct.pack('<iI', remap.get(rmb, -1), rcull)
        elif i > first:
            # only reference-free or single-reference blocks may sit after the removed ones
            if tn[i] == b'BSMultiBound' and len(blk) == 4:
                d = struct.unpack_from('<i', blk, 0)[0]
                if d >= 0 and d not in remap: raise ValueError('multibound data removed')
                blk = struct.pack('<i', remap.get(d, -1))
            elif tn[i] != b'BSMultiBoundAABB':
                raise ValueError('unexpected block after water: %r' % tn[i])
        out.append(blk)
    # rebuild type table with only the used types
    used = []
    for i in keep:
        if tn[i] not in used: used.append(tn[i])
    hdr = bytearray(b[:p0]) + struct.pack('<IBIII', ver, endian, uv, len(keep), bsver) + b[p0 + 17:types_off]
    hdr += struct.pack('<H', len(used))
    for t in used: hdr += struct.pack('<I', len(t)) + t
    hdr += struct.pack('<%dH' % len(keep), *[used.index(tn[i]) for i in keep])
    hdr += struct.pack('<%dI' % len(keep), *[len(x) for x in out])
    hdr += b[strs_off:blocks_off]
    return bytes(hdr) + b''.join(out) + footer

def main(src, dst):
    done = same = bad = 0
    for root, _, files in os.walk(src):
        for f in files:
            if not f.lower().endswith('.btr'): continue
            sp = os.path.join(root, f)
            b = open(sp, 'rb').read()
            try:
                nb = strip(b)
            except Exception as e:
                bad += 1; print('SKIP', sp, e); continue
            if nb is None: same += 1; continue
            if strip(nb) is not None: raise SystemExit('self-check failed ' + sp)
            dp = os.path.join(dst, os.path.relpath(sp, src))
            os.makedirs(os.path.dirname(dp), exist_ok=True)
            open(dp, 'wb').write(nb); done += 1
    print('written', done, 'no water', same, 'skipped', bad)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
