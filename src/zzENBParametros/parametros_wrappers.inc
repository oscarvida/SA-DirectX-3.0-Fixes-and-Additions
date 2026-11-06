// Generated from Microsoft DirectX SDK d3dx9effect.h. Do not hand-edit.
static HRESULT WINAPI hook_SetValue(ID3DXEffect* effect, D3DXHANDLE hParameter, LPCVOID pData, UINT Bytes) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, LPCVOID pData, UINT Bytes);
    return original<Fn>(s,20)(effect,hParameter, pData, Bytes);
}
static HRESULT WINAPI hook_SetBool(ID3DXEffect* effect, D3DXHANDLE hParameter, BOOL b) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, BOOL b);
    return original<Fn>(s,22)(effect,hParameter, b);
}
static HRESULT WINAPI hook_SetBoolArray(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST BOOL* pb, UINT Count) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST BOOL* pb, UINT Count);
    return original<Fn>(s,24)(effect,hParameter, pb, Count);
}
static HRESULT WINAPI hook_SetInt(ID3DXEffect* effect, D3DXHANDLE hParameter, INT n) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, INT n);
    return original<Fn>(s,26)(effect,hParameter, n);
}
static HRESULT WINAPI hook_SetIntArray(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST INT* pn, UINT Count) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST INT* pn, UINT Count);
    return original<Fn>(s,28)(effect,hParameter, pn, Count);
}
static HRESULT WINAPI hook_SetFloat(ID3DXEffect* effect, D3DXHANDLE hParameter, FLOAT f) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, FLOAT f);
    return original<Fn>(s,30)(effect,hParameter, f);
}
static HRESULT WINAPI hook_SetFloatArray(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST FLOAT* pf, UINT Count) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST FLOAT* pf, UINT Count);
    return original<Fn>(s,32)(effect,hParameter, pf, Count);
}
static HRESULT WINAPI hook_SetVector(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector);
    return original<Fn>(s,34)(effect,hParameter, pVector);
}
static HRESULT WINAPI hook_SetVectorArray(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector, UINT Count) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector, UINT Count);
    return original<Fn>(s,36)(effect,hParameter, pVector, Count);
}
static HRESULT WINAPI hook_SetMatrix(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix);
    return original<Fn>(s,38)(effect,hParameter, pMatrix);
}
static HRESULT WINAPI hook_SetMatrixArray(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count);
    return original<Fn>(s,40)(effect,hParameter, pMatrix, Count);
}
static HRESULT WINAPI hook_SetMatrixPointerArray(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count);
    return original<Fn>(s,42)(effect,hParameter, ppMatrix, Count);
}
static HRESULT WINAPI hook_SetMatrixTranspose(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix);
    return original<Fn>(s,44)(effect,hParameter, pMatrix);
}
static HRESULT WINAPI hook_SetMatrixTransposeArray(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count);
    return original<Fn>(s,46)(effect,hParameter, pMatrix, Count);
}
static HRESULT WINAPI hook_SetMatrixTransposePointerArray(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count);
    return original<Fn>(s,48)(effect,hParameter, ppMatrix, Count);
}
static HRESULT WINAPI hook_SetString(ID3DXEffect* effect, D3DXHANDLE hParameter, LPCSTR pString) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, LPCSTR pString);
    return original<Fn>(s,50)(effect,hParameter, pString);
}
static HRESULT WINAPI hook_SetTexture(ID3DXEffect* effect, D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 pTexture) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 pTexture);
    return original<Fn>(s,52)(effect,hParameter, pTexture);
}
static HRESULT WINAPI hook_SetArrayRange(ID3DXEffect* effect, D3DXHANDLE hParameter, UINT uStart, UINT uEnd) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, UINT uStart, UINT uEnd);
    return original<Fn>(s,56)(effect,hParameter, uStart, uEnd);
}
static HRESULT WINAPI hook_SetTechnique(ID3DXEffect* effect, D3DXHANDLE hTechnique) {
    DWORD last=GetLastError();auto* s=state(effect);
    hTechnique=resolve(effect,s,hTechnique,true);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hTechnique);
    return original<Fn>(s,58)(effect,hTechnique);
}
static HRESULT WINAPI hook_SetRawValue(ID3DXEffect* effect, D3DXHANDLE hParameter, LPCVOID pData, UINT ByteOffset, UINT Bytes) {
    DWORD last=GetLastError();auto* s=state(effect);
    hParameter=resolve(effect,s,hParameter,false);SetLastError(last);
    using Fn=HRESULT(WINAPI*)(ID3DXEffect* effect, D3DXHANDLE hParameter, LPCVOID pData, UINT ByteOffset, UINT Bytes);
    return original<Fn>(s,78)(effect,hParameter, pData, ByteOffset, Bytes);
}
static void installWrappers(EffectState* s) {
    s->table[20]=reinterpret_cast<void*>(hook_SetValue);
    s->table[22]=reinterpret_cast<void*>(hook_SetBool);
    s->table[24]=reinterpret_cast<void*>(hook_SetBoolArray);
    s->table[26]=reinterpret_cast<void*>(hook_SetInt);
    s->table[28]=reinterpret_cast<void*>(hook_SetIntArray);
    s->table[30]=reinterpret_cast<void*>(hook_SetFloat);
    s->table[32]=reinterpret_cast<void*>(hook_SetFloatArray);
    s->table[34]=reinterpret_cast<void*>(hook_SetVector);
    s->table[36]=reinterpret_cast<void*>(hook_SetVectorArray);
    s->table[38]=reinterpret_cast<void*>(hook_SetMatrix);
    s->table[40]=reinterpret_cast<void*>(hook_SetMatrixArray);
    s->table[42]=reinterpret_cast<void*>(hook_SetMatrixPointerArray);
    s->table[44]=reinterpret_cast<void*>(hook_SetMatrixTranspose);
    s->table[46]=reinterpret_cast<void*>(hook_SetMatrixTransposeArray);
    s->table[48]=reinterpret_cast<void*>(hook_SetMatrixTransposePointerArray);
    s->table[50]=reinterpret_cast<void*>(hook_SetString);
    s->table[52]=reinterpret_cast<void*>(hook_SetTexture);
    s->table[56]=reinterpret_cast<void*>(hook_SetArrayRange);
    s->table[58]=reinterpret_cast<void*>(hook_SetTechnique);
    s->table[78]=reinterpret_cast<void*>(hook_SetRawValue);
}
