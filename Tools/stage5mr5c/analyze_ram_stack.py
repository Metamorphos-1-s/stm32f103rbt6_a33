#!/usr/bin/env python3
import argparse
import json
import re
import subprocess
from collections import defaultdict
from pathlib import Path


def command(*args):
    return subprocess.check_output(args, text=True, errors="replace")


def parse_nm(elf, nm):
    symbols=[]
    pattern=re.compile(r"^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([bBdD])\s+(.+)$")
    for line in command(nm,"-S","--size-sort",str(elf)).splitlines():
        match=pattern.match(line.strip())
        if match:
            address,size,kind,name=match.groups()
            symbols.append({"name":name,"address":"0x%08X"%int(address,16),
                "size":int(size,16),"section":"bss" if kind.lower()=="b" else "data"})
    return sorted(symbols,key=lambda item:item["size"],reverse=True)


def parse_sections(elf,objdump):
    sections={}
    pattern=re.compile(r"^\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)")
    for line in command(objdump,"-h",str(elf)).splitlines():
        match=pattern.match(line)
        if match:
            name,size,address=match.groups()
            sections[name]={"address":"0x%08X"%int(address,16),"size":int(size,16)}
    return sections


def parse_map_modules(map_path):
    modules=defaultdict(int)
    pattern=re.compile(r"^\s+\.(?:bss|data)(?:\.\S+)?\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.+\.obj)$")
    pending=re.compile(r"^\s+\.(?:bss|data)\.\S+\s*$")
    continuation=re.compile(r"^\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.+\.obj)$")
    have_pending=False
    for line in map_path.read_text(encoding="utf-8",errors="replace").splitlines():
        match=pattern.match(line)
        if match:
            address,size,obj=match.groups()
            if int(address,16)>=0x20000000:
                modules[obj.replace("\\","/")]+=int(size,16)
            have_pending=False
        elif have_pending:
            match=continuation.match(line)
            if match:
                address,size,obj=match.groups()
                if int(address,16)>=0x20000000:
                    modules[obj.replace("\\","/")]+=int(size,16)
            have_pending=False
        else:
            have_pending=bool(pending.match(line))
    return [{"object":name,"ram_bytes":size} for name,size in
        sorted(modules.items(),key=lambda item:item[1],reverse=True)]


def parse_callgraphs(root):
    stack={};simple=defaultdict(list);edges=[];indirect=[]
    node_re=re.compile(r'node: \{ title: "([^"]+)" label: "([^"\\]*(?:\\.[^"\\]*)*)"')
    edge_re=re.compile(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"')
    for path in Path(root).rglob("*.ci"):
        text=path.read_text(encoding="utf-8",errors="replace")
        for title,label in node_re.findall(text):
            name=label.split("\\n",1)[0]
            match=re.search(r"\\n(\d+) bytes \(([^)]+)\)",label)
            if match:
                stack[title]=int(match.group(1));simple[name].append(title)
        for source,target in edge_re.findall(text):
            edges.append((source,target,str(path)))

    def resolve(name):
        if name in stack:return name
        matches=simple.get(name,[])
        return matches[0] if len(matches)==1 else None

    graph=defaultdict(set)
    for source,target,path in edges:
        src=resolve(source);dst=resolve(target)
        if target=="__indirect_call":
            indirect.append({"caller":source,"source":path})
        elif src is not None and dst is not None:
            graph[src].add(dst)

    cycles=[]
    memo={}
    def longest(node,visiting=()):
        if node in memo:return memo[node]
        if node in visiting:
            cycles.append([*visiting,node]);return (0,[])
        best=(0,[])
        for child in graph.get(node,()):
            child_value,child_path=longest(child,(*visiting,node))
            if child_value>best[0]:best=(child_value,child_path)
        result=(stack.get(node,0)+best[0],[node,*best[1]])
        memo[node]=result;return result

    reports=[]
    for title,value in stack.items():
        total,path=longest(title)
        reports.append({"function":title,"local_stack_bytes":value,
            "static_chain_bytes":total,"path":path})
    reports.sort(key=lambda item:item["static_chain_bytes"],reverse=True)
    irq=[item for item in reports if item["function"].endswith("Handler") or
         "_IRQHandler" in item["function"]]
    main=next((item for item in reports if item["function"]=="main"),None)
    return reports,irq,main,indirect,cycles


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--elf",required=True);parser.add_argument("--map",required=True)
    parser.add_argument("--callgraph-root",required=True);parser.add_argument("--output",required=True)
    parser.add_argument("--nm",default="arm-none-eabi-nm")
    parser.add_argument("--objdump",default="arm-none-eabi-objdump")
    parser.add_argument("--observed-msp",type=lambda value:int(value,0))
    parser.add_argument("--observed-static-end",type=lambda value:int(value,0))
    args=parser.parse_args();out=Path(args.output);out.mkdir(parents=True,exist_ok=True)
    elf=Path(args.elf);map_path=Path(args.map)
    symbols=parse_nm(elf,args.nm);sections=parse_sections(elf,args.objdump)
    module_ram=parse_map_modules(map_path)
    chains,irq,main_chain,indirect,cycles=parse_callgraphs(args.callgraph_root)
    ram_origin=0x20000000;ram_end=0x20005000;reserved_stack=1024;reserved_heap=0
    data=sections.get(".data",{"address":"0x20000000","size":0})
    bss=sections.get(".bss",{"address":"0x20000000","size":0})
    bss_end=int(bss["address"],16)+bss["size"]
    aligned_static_end=(bss_end+7)&~7
    reserved_end=aligned_static_end+reserved_heap+reserved_stack
    unallocated=ram_end-reserved_end
    static_to_estack=ram_end-aligned_static_end
    ram_layout={"schema_version":1,"classification":"LINKER_MAP_STATIC_ANALYSIS",
        "elf":str(elf).replace("\\","/"),"ram_origin":"0x%08X"%ram_origin,
        "ram_end":"0x%08X"%ram_end,"ram_total_bytes":ram_end-ram_origin,
        "data":data,"bss":bss,"noinit":sections.get(".noinit",{"present":False,"size":0}),
        "aligned_static_end":"0x%08X"%aligned_static_end,
        "min_heap_bytes":reserved_heap,"min_stack_bytes":reserved_stack,
        "reserved_heap_stack_end":"0x%08X"%reserved_end,
        "estack":"0x%08X"%ram_end,"unallocated_after_reserved_stack_bytes":unallocated,
        "total_static_to_estack_bytes":static_to_estack,
        "linker_ram_used_bytes":reserved_end-ram_origin,
        "linker_ram_free_bytes":unallocated,"module_ram":module_ram}
    (out/"ram_layout.json").write_text(json.dumps(ram_layout,indent=2)+"\n",encoding="utf-8")
    largest={"schema_version":1,"symbols":symbols[:80],
        "r5_state":next((s for s in symbols if s["name"]=="s_r5_drift"),None),
        "weight_engine":next((s for s in symbols if s["name"]=="s_engine"),None),
        "critical_buffers":[s for s in symbols if any(word in s["name"].lower()
            for word in ("dma","rx","tx","buffer","queue","cache","server"))]}
    (out/"largest_ram_symbols.json").write_text(json.dumps(largest,indent=2)+"\n",encoding="utf-8")
    max_irq=max(irq,key=lambda item:item["static_chain_bytes"],default=None)
    main_bytes=main_chain["static_chain_bytes"] if main_chain else None
    irq_bytes=max_irq["static_chain_bytes"] if max_irq else None
    conservative=None if main_bytes is None or irq_bytes is None else main_bytes+32+irq_bytes
    indirect_allowance=256
    bounded_conservative=None if conservative is None else conservative+indirect_allowance
    msp_evidence=None
    if args.observed_msp is not None:
        msp_evidence={"classification":"INSTANTANEOUS_SWD_SAMPLE_NOT_WATERMARK",
            "msp":"0x%08X"%args.observed_msp,
            "depth_from_estack_bytes":ram_end-args.observed_msp,
            "firmware":"R5B 0x0511 before R5C reflash",
            "static_end":"0x%08X"%args.observed_static_end if args.observed_static_end else None,
            "distance_above_static_end_bytes":None if args.observed_static_end is None else args.observed_msp-args.observed_static_end}
    stack_report={"schema_version":1,"classification":"STATIC_STACK_ANALYSIS_NOT_RUNTIME_WATERMARK",
        "declared_stack_bytes":reserved_stack,"hardware_exception_frame_bytes":32,
        "main_static_chain":main_chain,"largest_irq_static_chain":max_irq,
        "conservative_main_plus_one_irq_bytes":conservative,
        "indirect_call_allowance_bytes":indirect_allowance,
        "bounded_conservative_stack_bytes":bounded_conservative,
        "margin_within_declared_stack_bytes":None if bounded_conservative is None else reserved_stack-bounded_conservative,
        "margin_before_static_collision_bytes":None if bounded_conservative is None else static_to_estack-bounded_conservative,
        "instantaneous_msp_evidence":msp_evidence,
        "largest_chains":chains[:30],"irq_chains":irq,
        "indirect_call_sites":indirect,"cycle_count":len(cycles),
        "limitations":["GCC callgraph does not resolve function-pointer targets.",
            "Assembly startup usage and asynchronous nesting are not directly measured.",
            "All enabled peripheral IRQs are configured at preemption priority 5 and cannot preempt each other.",
            "This report is not a fill-pattern runtime stack-watermark measurement."]}
    (out/"stack_watermark.json").write_text(json.dumps(stack_report,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"ram":ram_layout,"main":main_chain,"max_irq":max_irq,
        "conservative":bounded_conservative,"indirect_count":len(indirect)},indent=2))


if __name__=="__main__":raise SystemExit(main())
