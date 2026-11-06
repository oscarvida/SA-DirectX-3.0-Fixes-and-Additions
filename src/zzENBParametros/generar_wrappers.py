"""Generate exact x86 signatures from the installed Microsoft DirectX header."""
import re
from pathlib import Path
root=Path(__file__).resolve().parent
header=Path(r'C:\dev\plugin-sdk\shared\dxsdk\d3dx9effect.h').read_text()
body=header.split('DECLARE_INTERFACE_(ID3DXEffect, ID3DXBaseEffect)')[1].split('};')[0]
rx=r'STDMETHOD(?:_\(([^,]+),\s*(\w+)\)|\((\w+)\))\((.*?)\)\s*PURE;'
out=['// Generated from Microsoft DirectX SDK d3dx9effect.h. Do not hand-edit.']
inst=[]
for index,m in enumerate(re.finditer(rx,body,re.S)):
    ret,name,args=m.group(1) or 'HRESULT',m.group(2) or m.group(3),' '.join(m.group(4).split())
    args=args.replace('THIS_ ','')
    if not name.startswith('Set') or not args.startswith('D3DXHANDLE'):continue
    parts=args.split(', ');names=[re.search(r'(\w+)$',a).group(1) for a in parts]
    sig=', '.join(['ID3DXEffect* effect']+parts)
    h=names[0];technique=name=='SetTechnique'
    out += [f'static {ret} WINAPI hook_{name}({sig}) {{',
            '    DWORD last=GetLastError();auto* s=state(effect);',
            f'    {h}=resolve(effect,s,{h},{str(technique).lower()});SetLastError(last);',
            f'    using Fn={ret}(WINAPI*)({sig});',
            f'    return original<Fn>(s,{index})(effect,{", ".join(names)});', '}']
    inst.append(f'    s->table[{index}]=reinterpret_cast<void*>(hook_{name});')
out+=['static void installWrappers(EffectState* s) {',*inst,'}']
assert index==78 and len(inst)==20,(index,len(inst))
(root/'parametros_wrappers.inc').write_text('\n'.join(out)+'\n',encoding='utf-8')
print('Generated',len(inst),'typed forwarding methods; 79-slot original interface')
