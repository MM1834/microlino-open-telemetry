#!/usr/bin/env python3
from __future__ import annotations
import argparse,re,shutil,sys
from pathlib import Path
PROTECTED_ROUTES=("VehicleApiListRoute","VehicleApiSnapshotRoute")
PUBLIC_ROUTE="VehicleApiHealthRoute"
AUTHORIZER="VehicleApiJwtAuthorizer"
def block(text,lid):
    m=re.search(rf"(?m)^  {re.escape(lid)}:\s*$",text)
    if not m: raise RuntimeError(f"Resource not found: {lid}")
    n=re.search(r"(?m)^  [A-Za-z][A-Za-z0-9]+:\s*$",text[m.end():])
    end=m.end()+n.start() if n else len(text)
    return m.start(),end,text[m.start():end]
def protect(b,lid):
    t=re.search(r"(?m)^      AuthorizationType:\s*JWT\s*$",b)
    a=re.search(rf"(?m)^      AuthorizerId:\s*!Ref\s+{AUTHORIZER}\s*$",b)
    if t and a:return b
    if t or a:raise RuntimeError(f"{lid} has partial authorization config")
    trg=re.search(r"(?m)^      Target:\s*.+$",b)
    if not trg:raise RuntimeError(f"{lid}: Target property not found")
    add=f"      AuthorizationType: JWT\n      AuthorizerId: !Ref {AUTHORIZER}\n"
    return b[:trg.start()]+add+b[trg.start():]
def verify_auth(text):
    _,_,b=block(text,AUTHORIZER)
    if not re.search(r"(?m)^    Type:\s*AWS::ApiGatewayV2::Authorizer\s*$",b):raise RuntimeError('Unexpected authorizer type')
    if not re.search(r"(?m)^      AuthorizerType:\s*JWT\s*$",b):raise RuntimeError('Authorizer is not JWT')
def verify_health(text):
    _,_,b=block(text,PUBLIC_ROUTE)
    if re.search(r"(?m)^      AuthorizationType:\s*JWT\s*$",b) or re.search(r"(?m)^      AuthorizerId:",b):raise RuntimeError('Health route must remain public')
def verify(text):
    verify_auth(text);verify_health(text)
    for lid in PROTECTED_ROUTES:
        _,_,b=block(text,lid)
        if not re.search(r"(?m)^      AuthorizationType:\s*JWT\s*$",b):raise RuntimeError(f'{lid}: JWT missing')
        if not re.search(rf"(?m)^      AuthorizerId:\s*!Ref\s+{AUTHORIZER}\s*$",b):raise RuntimeError(f'{lid}: AuthorizerId missing')
def main():
    p=argparse.ArgumentParser();p.add_argument('--template',default='cloud/aws/foundation/template.yaml');p.add_argument('--check',action='store_true');a=p.parse_args();path=Path(a.template)
    if not path.is_file():print(f'ERROR: template not found: {path}',file=sys.stderr);return 2
    original=path.read_text(encoding='utf-8')
    try:
        if a.check:verify(original);print('OK: route authorization configuration is correct');return 0
        verify_auth(original);verify_health(original)
        updated=original
        items=[]
        for lid in PROTECTED_ROUTES:
            s,e,b=block(updated,lid);items.append((s,e,b,lid))
        for s,e,b,lid in sorted(items,reverse=True):updated=updated[:s]+protect(b,lid)+updated[e:]
        verify(updated)
        if updated==original:print('No changes required.');return 0
        backup=path.with_suffix(path.suffix+'.spr-0004b2-2.bak');shutil.copy2(path,backup);path.write_text(updated,encoding='utf-8')
        print(f'Updated: {path}');print(f'Backup: {backup}');return 0
    except RuntimeError as e:
        print(f'ERROR: {e}',file=sys.stderr);print('No speculative changes were made.',file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
