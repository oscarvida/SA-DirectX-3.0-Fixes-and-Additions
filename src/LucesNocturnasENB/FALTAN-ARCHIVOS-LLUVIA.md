# Archivos de lluvia no incluidos / Rain files not included

Para respetar el trabajo de **Proper Shaders** (Junior_Djjr y Jéssica Natália), este repositorio **no** incluye:

- `lluvia_tex.h` — contiene incrustadas las texturas `RainDrop.dds` / `RainDrop_nm.dds` de Proper Shaders 08-26.
- `lluvia.hlsl` y su bytecode compilado (`lluvia_psc.h`, `lluvia_psg.h`, `lluvia_pss.h`, `lluvia_vsg.h`,
  `lluvia_vsp.h`, `lluvia_vss.h`) — el sombreador de las gotas se basa en el RainStreaks de Proper Shaders.

Por eso `LucesNocturnasENB` no compila desde este repositorio tal cual. El binario completo
(`LucesNocturnasENB.SA.asi` 2.0k-r1) está en el ZIP de [Releases](../../../releases), y su SHA-256 en `FILES.sha256`.
El resto de plugins compila y reproduce los binarios publicados.

---

To respect the work of **Proper Shaders** (Junior_Djjr and Jéssica Natália), the rain texture header and the rain
shader derived from its RainStreaks shader are not published here, so LucesNocturnasENB does not build from this
repository as-is. The complete binary is in the Releases ZIP.
