#!/usr/bin/env python3
"""Export exact authored placement data from an uncompressed CE609 cache.

This reads data only. It does not grant redistribution rights, reauthor a route,
or establish compatibility by tag path alone.
"""
import argparse, hashlib, json, math, struct
from pathlib import Path

class CECache:
    def __init__(self, path):
        self.path = Path(path)
        if not 2048 <= self.path.stat().st_size <= 384 * 1024 * 1024:
            raise ValueError("cache size outside CE limit")
        self.data = self.path.read_bytes()
        if self.data[:4] != b"daeh" or self.data[2044:2048] != b"toof" or self.u32(4) != 609:
            raise ValueError("only uncompressed Halo CE609 caches are supported")
        self.tag, self.tag_size = struct.unpack_from("<II", self.data, 16)
        if self.tag < 2048 or self.tag_size <= 0 or self.tag + self.tag_size != len(self.data):
            raise ValueError("invalid cache tag region")
        ti, si, _, nt = struct.unpack_from("<IIII", self.data, self.tag)
        if nt <= 0 or nt > 65535:
            raise ValueError("invalid tag count")
        table = self.off(ti, nt * 32)
        self.tags = []
        for i in range(nt):
            group, _, _, ident, name, base, external, _ = struct.unpack_from("<IIIIIIII", self.data, table + i * 32)
            self.tags.append({"index":ident, "group":group.to_bytes(4,"big").decode("ascii",errors="replace"),
                              "path":self.string(name), "base":base, "external":external})
        self.scenario = self.tags[si & 65535]
        if self.scenario["index"] != si or self.scenario["group"] != "scnr":
            raise ValueError("invalid scenario identity")
    def u32(self, p):
        return struct.unpack_from("<I", self.data, p)[0]
    def off(self, address, size):
        p = self.tag + address - 0x40440000
        if size < 0 or not self.tag <= p <= p + size <= self.tag + self.tag_size:
            raise ValueError("tag pointer out of bounds")
        return p
    def string(self, address):
        p = self.off(address,1);end=self.data.find(b"\0",p,min(len(self.data),p+256))
        if end < 0:raise ValueError("unterminated tag path")
        return self.data[p:end].decode("ascii")
    def block(self, rel, size):
        n, address = struct.unpack_from("<iI",self.data,self.off(self.scenario["base"]+rel,12))
        if not 0 <= n <= 65535:raise ValueError("invalid reflexive count")
        p=self.off(address,n*size) if n else 0
        return [self.data[p+i*size:p+(i+1)*size] for i in range(n)]
    def palette(self, rel):
        rows=[]
        for row in self.block(rel,48):
            group,name,_,ident=struct.unpack_from("<IIII",row)
            path=self.string(name) if name else ""
            rows.append({"path":path,"group":group.to_bytes(4,"big").decode("ascii",errors="replace"),"tag_index":ident})
        return rows
    def export(self):
        def finite(values):
            if not all(math.isfinite(v) for v in values):raise ValueError("nonfinite placement")
            return list(values)
        palettes={k:self.palette(p) for k,p in [("scenery",0x21c),("vehicle",0x24c)]}
        scenery=[]
        for i,row in enumerate(self.block(0x210,72)):
            typ,name,flags,perm=struct.unpack_from("<hhhh",row)
            if not 0<=typ<len(palettes["scenery"]):raise ValueError("invalid scenery palette index")
            scenery.append({"original_index":i,**palettes["scenery"][typ],"name_index":name,"placement_flags":flags,
                            "permutation":perm,"position":finite(struct.unpack_from("<fff",row,8)),
                            "rotation_yaw_pitch_roll":finite(struct.unpack_from("<fff",row,20)),
                            "bsp_indices":self.urow(row,32),"appearance_player_index":struct.unpack_from("<b",row,36)[0]})
        starts=[]
        for i,row in enumerate(self.block(0x354,52)):
            pos=finite(struct.unpack_from("<fff",row));facing=struct.unpack_from("<f",row,12)[0]
            team,bsp,t0,t1,t2,t3=struct.unpack_from("<hhhhhh",row,16)
            starts.append({"original_index":i,"position":pos,"facing":facing,"team":team,"bsp_index":bsp,"game_types":[t0,t1,t2,t3]})
        flags=[]
        for i,row in enumerate(self.block(0x378,148)):
            typ,ordinal=struct.unpack_from("<hh",row,16)
            flags.append({"original_index":i,"position":finite(struct.unpack_from("<fff",row)),
                          "facing":struct.unpack_from("<f",row,12)[0],"type":typ,"ordinal":ordinal})
        vehicles=[]
        for i,row in enumerate(self.block(0x240,120)):
            typ=struct.unpack_from("<h",row)[0]
            if not 0<=typ<len(palettes["vehicle"]):raise ValueError("invalid vehicle palette index")
            vehicles.append({"original_index":i,**palettes["vehicle"][typ],
                             "position":finite(struct.unpack_from("<fff",row,8)),
                             "rotation_yaw_pitch_roll":finite(struct.unpack_from("<fff",row,20)),
                             "multiplayer_flags":struct.unpack_from("<H",row,90)[0],
                             "multiplayer_team_index":struct.unpack_from("<b",row,88)[0],
                             "name_index":struct.unpack_from("<h",row,2)[0],
                             "placement_flags":struct.unpack_from("<H",row,4)[0],
                             "permutation":struct.unpack_from("<h",row,6)[0],
                             "body_vitality":struct.unpack_from("<f",row,72)[0],
                             "unit_flags":struct.unpack_from("<I",row,76)[0]})
        checkpoints=sorted((f for f in flags if f["type"]==3),key=lambda f:(f["ordinal"],f["original_index"]))
        return {"schema_version":1,"source_cache":{"path":str(self.path),"sha256":hashlib.sha256(self.data).hexdigest(),
                "bytes":len(self.data),"version":609,"scenario_tag":self.scenario["path"]},
                "scope":"Exact authored data; private conversion only; redistribution permission not inferred",
                "scenery":scenery,"player_starts":starts,"netgame_flags":flags,
                "ordered_checkpoints":checkpoints,"race_vehicle_markers":[f for f in flags if f["type"]==4],
                "placed_vehicles":vehicles,"palettes":palettes,
                "tag_inventory":[{k:t[k] for k in ("index","group","path","external")} for t in self.tags]}
    @staticmethod
    def urow(row,p):return struct.unpack_from("<H",row,p)[0]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("cache",type=Path);p.add_argument("output",type=Path)
    a=p.parse_args();result=CECache(a.cache).export()
    a.output.write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({"output":str(a.output),"scenery":len(result["scenery"]),
                     "starts":len(result["player_starts"]),"checkpoints":len(result["ordered_checkpoints"]),
                     "race_vehicle_markers":len(result["race_vehicle_markers"]),
                     "vehicles":len(result["placed_vehicles"])}))
if __name__=="__main__":main()
