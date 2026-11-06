#pragma once
#include <cstdint>
#include <algorithm>

// 0.4 (29-10-2026): luz de la hierba de CPlantMgr. Cada mata toma el color de plants.dat por la luz de la cara de
// colision sobre la que crece (tColLighting: dia en los 4 bits bajos, noche en los altos; 0x59F0C0, en linea en
// CPlantMgr::Render 0x5DBCDB). Las colisiones de Liberty City de GTA Mixed traen 4/1 en el 88 % de la hierba alta
// (valor por defecto del conversor; San Andreas lleva 15 de dia): con ENB la hierba salia negra a pleno sol. De noche,
// el PC no suma el ambiente que la PS2 anadia a la hierba (grassAddAmbient de SkyGFX): 0-3 de 15 = negro.
// Se suben los dos valores a un minimo; nunca se bajan. El orden dia/noche de la cara se conserva.
namespace pasto {
inline uint8_t remap(uint8_t v,int diaMin,int nocheMin) {
    int d=std::max(int(v&15),std::clamp(diaMin,0,15)), n=std::max(int(v>>4),std::clamp(nocheMin,0,15));
    return uint8_t(d|(n<<4));
}
inline void table(uint8_t* t,int diaMin,int nocheMin) { for(int i=0;i<256;++i)t[i]=remap(uint8_t(i),diaMin,nocheMin); }
}
