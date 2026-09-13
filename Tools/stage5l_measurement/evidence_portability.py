#!/usr/bin/env python3
import argparse, hashlib, json, subprocess
from pathlib import Path

def digest(data): return hashlib.sha256(data).hexdigest().upper()
def canonical_json(value): return (json.dumps(value,indent=2)+"\n").encode("utf-8")
def eol_variants(data):
    lf=data.replace(b"\r\n",b"\n")
    variants={"git_bytes":data,"lf":lf,"crlf":lf.replace(b"\n",b"\r\n")}
    variants["lf_no_final_newline"]=lf[:-1] if lf.endswith(b"\n") else lf
    crlf=variants["crlf"];variants["crlf_no_final_newline"]=crlf[:-2] if crlf.endswith(b"\r\n") else crlf
    return variants
def classify(data,length,sha):
    matches=[name for name,value in eol_variants(data).items() if len(value)==length and digest(value)==sha]
    return matches[0] if matches else "non_eol_difference"
def git_bytes(revision,path):
    return subprocess.check_output(["git","show",revision+":"+str(path).replace("\\","/")])
def audit_run(manifest_path,revision=None):
    read=lambda p:git_bytes(revision,p) if revision else p.read_bytes()
    manifest=json.loads(read(manifest_path).decode("utf-8"));items=[];valid=True
    for item in manifest["files"]:
        path=manifest_path.parent/item["path"];data=read(path);kind=classify(data,item["length"],item["sha256"])
        exact=kind=="git_bytes";valid &= exact
        variants=eol_variants(data)
        items.append({"path":item["path"],"manifest_length":item["length"],"manifest_sha256":item["sha256"],"git_length":len(data),"git_sha256":digest(data),"crlf_length":len(variants["crlf"]),"crlf_sha256":digest(variants["crlf"]),"classification":kind})
    return {"run_id":manifest.get("run_id",manifest_path.parent.name),"historical_manifest":str(manifest_path).replace("\\","/"),"historical_manifest_valid_against_git_bytes":valid,"files":items}
def build_v2(run):
    entries=[]
    for item in run["files"]: entries.append({"path":item["path"],"length":item["git_length"],"sha256":item["git_sha256"]})
    return {"schema_version":2,"run_id":run["run_id"],"classification":"RECOVERED ENGINEERING EVIDENCE","historical_capture_manifest_status":"HISTORICAL CAPTURE MANIFEST; INVALID AGAINST CURRENT GIT BYTES DUE TO EOL NORMALIZATION" if not run["historical_manifest_valid_against_git_bytes"] else "HISTORICAL CAPTURE MANIFEST VALID","hardware_capture_rerun":False,"measurement_values_modified":False,"repository_byte_repair_layer":True,"files":entries}
def audit_tree(root,write_v2=False,revision=None):
    manifests=sorted(Path(root).rglob("manifest.json"));runs=[]
    for path in manifests:
        run=audit_run(path,revision);runs.append(run)
        if write_v2:(path.parent/"repository_manifest_v2.json").write_bytes(canonical_json(build_v2(run)))
    non_eol=[(r["run_id"],f["path"]) for r in runs for f in r["files"] if f["classification"]=="non_eol_difference"]
    return {"schema_version":1,"root":str(root).replace("\\","/"),"git_revision":revision,"manifest_count":len(runs),"exact_pass_count":sum(r["historical_manifest_valid_against_git_bytes"] for r in runs),"exact_fail_count":sum(not r["historical_manifest_valid_against_git_bytes"] for r in runs),"non_eol_differences":non_eol,"runs":runs}
def validate_v2(root):
    files=list(Path(root).rglob("repository_manifest_v2.json"))
    for path in files:
        m=json.loads(path.read_bytes().decode("utf-8"))
        for item in m["files"]:
            data=(path.parent/item["path"]).read_bytes()
            if len(data)!=item["length"] or digest(data)!=item["sha256"]:raise ValueError(str(path)+":"+item["path"])
    print("REPOSITORY_MANIFEST_V2 PASS",len(files));return len(files)
def main():
    p=argparse.ArgumentParser();p.add_argument("command",choices=("audit","build-v2","validate-v2"));p.add_argument("--root",required=True);p.add_argument("--output");p.add_argument("--git-revision");a=p.parse_args()
    if a.command=="validate-v2":validate_v2(a.root);return 0
    result=audit_tree(a.root,a.command=="build-v2",a.git_revision)
    if result["non_eol_differences"]:raise RuntimeError("non-EOL differences: "+repr(result["non_eol_differences"]))
    data=canonical_json(result)
    if a.output:Path(a.output).write_bytes(data)
    else:print(data.decode("utf-8"))
    return 0
if __name__=="__main__":raise SystemExit(main())
