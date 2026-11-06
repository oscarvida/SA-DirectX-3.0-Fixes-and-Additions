SA DIRECTX 3.0 — FIXES & ADDITIONS
Arreglos, efectos adicionales e integración: Oscar Vidal
Paquete preparado el 5 de noviembre de 2026

REVISIÓN r1 (5 de noviembre de 2026)
=====================================
El único cambio de código es LucesNocturnasENB.SA.asi (2.0k-r1): la carpeta de capturas ya no
tiene una ruta del autor por defecto y una ruta relativa se crea dentro de la
carpeta del juego. También se corrigen los saltos de línea de los textos.
Detalles y hashes en CHANGES_r1.txt.

QUÉ ES
======
Un complemento para GTA San Andreas de PC que añade correcciones y efectos
sobre SA DirectX 3.0, con la actualización no oficial de Inari Lú como base.
Mantiene ese preset y amplía su funcionamiento mediante plugins propios,
shaders modificados y ajustes de integración.

Esta descarga NO incluye el preset original ni el paquete de Inari Lú.
Ambos se consiguen e instalan por separado, antes de este complemento.
Tampoco incluye el juego, mapas, vehículos ni un paquete de mods completo.

QUÉ CAMBIA
==========
- ShadowCasterFix: corrige el recorte de sombras de objetos que salen del
  encuadre al girar la cámara. Actúa desde el lado del juego para conservar
  los objetos relevantes en la primera cascada de sombras complejas de ENB.
- LucesNocturnasENB: coronas de luz después de la composición de ENB; luces
  locales sobre suelo y superficies; faros, luces de freno y marcha atrás;
  haces de luz, respuesta en mojado y luces de disparos, explosiones y fuego.
- Niebla y cielo: densidad según clima/hora, cielo cubierto nocturno visible
  y cambios en la capa de nubes de día despejado.
- Lluvia: gotas iluminadas y refractivas, salpicaduras que consultan la
  profundidad y comprobaciones para interiores y zonas bajo techo.
- Humo y pasto: iluminación del humo y un mínimo de luz para evitar hierba
  excesivamente oscura, especialmente en mapas con poca luz de colisión.
- Interiores y amanecer: detección de interiores y correcciones de shaders,
  incluida la comprobación del sol para los rayos del amanecer.
- ENBShaderCache: reutiliza los efectos compilados para agilizar arranques
  posteriores. La primera compilación sigue siendo necesaria.
- enbseries_guardia: contempla un fallo conocido de creación de efectos
  internos de ENB; no es una solución general para cualquier cierre del juego.
- Complemento opcional: RendimientoHiloPrincipal reduce determinadas
  llamadas repetidas al sistema y búsquedas fallidas de archivos/carpetas.

REQUISITOS Y DESCARGAS
=====================
1. GTA San Andreas clásico de PC, ejecutable 1.0 US de 32 bits, en Windows.
   Esta entrega está orientada al modo de un jugador. No está preparada para
   Definitive Edition, Android/iOS, SA:MP o MTA.
2. ASI Loader y Mod Loader funcionando. CLEO debe estar instalado según las
   instrucciones del preset original. No se incluyen esos cargadores.
3. Los componentes heredados de DirectX 9 que necesita ENB, incluido
   d3dx9_43.dll. Tener DirectX 11/12 no sustituye necesariamente esos archivos.
4. SA DirectX 3.0 de Makarus, versión beta Single Player / Ultra,
   con ENBSeries 0.430, que es la base usada para esta entrega:
   https://www.patreon.com/c/sadx3/posts
5. SA DirectX 3.0 — Unofficial Update By Inari Lú, variante Ultra:
   https://www.gtainside.com/sanandreas/mods/213892-sa-directx-3-0-unofficial-update-by-inari-l/

Las dos primeras capas aportan el runtime, los recursos sin modificar y los
modelos corregidos de Inari. Aquí se distribuyen solo los archivos nuevos o
modificados que se aplican encima. Deferred but Forward SkyGFX y Proper
Shaders no se instalan como dependencias de este complemento.

INSTALACIÓN
===========
1. Cierra el juego. Respalda los archivos gráficos de tu instalación antes
   de reemplazarlos; guarda también tus INI personales si deseas conservarlos.
2. Instala SA DirectX 3.0 siguiendo las instrucciones de Makarus. Usa los
   archivos de Single Player / Ultra, no la carpeta entera del paquete.
3. Instala después la actualización de Inari Lú, variante Ultra, incluyendo
   sus recursos y modelos de preiluminación. Acepta sus reemplazos.
4. Abre Copy-to-game-folder dentro de esta descarga. Copia SU CONTENIDO a
   la carpeta donde está gta_sa.exe y acepta los reemplazos. No copies la
   carpeta contenedora como una subcarpeta del juego.

   Ejemplo de ubicación final:
   GTA San Andreas\gta_sa.exe
   GTA San Andreas\ENBShaderCache.asi
   GTA San Andreas\enbseries\AportesENB.fx
   GTA San Andreas\modloader\_ARREGLO sombras ENB al girar la camara\ShadowCasterFix.asi

5. Abre el juego para comprobar la instalación. El primer arranque puede
   tardar más al compilar shaders. En los siguientes se reutiliza la caché.
6. Optional-performance es independiente. Primero prueba el apartado gráfico;
   si quieres añadirlo, copia el contenido de Optional-performance a la misma
   raíz del juego. Así se añade solo su carpeta modloader.

Orden: DirectX 3.0 original -> actualización de Inari Lú -> este complemento.
No vuelvas a aplicar las dos dependencias encima sin reinstalar después el
complemento: reemplazarían algunos de sus shaders.

Los README, las descripciones de LibertyCity, Screenshots y FILES.sha256 son
documentación/material de presentación; no se copian a la carpeta del juego.
No instales simultáneamente otro motor gráfico que sustituya las mismas
pasadas de renderizado, como Deferred but Forward SkyGFX o Proper Shaders.

CONTROLES INCLUIDOS
==================
Insert        Alternar ShadowCasterFix.
Inicio        Alternar las luces locales de LucesNocturnasENB.
F7            Alternar el orden de dibujado de las coronas.
F8            Alternar las funciones de iluminación de la rama 2.0.
Shift+Enter   Abrir el editor de ENB.
Shift+F12     Activar/desactivar ENB.

Son los valores de los INI incluidos. Si otro mod usa esas teclas, cambia
TeclaAlternar en ShadowCasterFix.ini o [Teclas] en LucesNocturnasENB.SA.ini.

AJUSTES, DIAGNÓSTICO Y DESINSTALACIÓN
===================================
- Los INI de cada plugin están junto a su ASI. Edita los valores con el juego
  cerrado. Activo=0 desactiva los módulos que usan esa clave; ENBShaderCache
  utiliza Enabled=0. Conserva las secciones y nombres de las claves.
- ShadowCasterFix.ini: Ajuste=B es el modo configurado. El arreglo se limita
  a la primera cascada; las sombras lejanas pueden seguir recortándose y hay
  un coste de renderizado. No se promete una ganancia de FPS general.
- LucesNocturnasENB.SA.ini: GananciaFarolas/GananciaFaros regulan intensidad;
  los apartados del INI permiten ajustar luces, haces y lluvia por separado.
- HumoLucesENB.SA.ini contiene [Humo] y [Pasto]. Los mínimos de luz del pasto
  se pueden ajustar para otros mapas.
- ENBShaderCache.ini: Carpeta=enbseries\cache_efectos. Si cambias el runtime
  de DirectX o necesitas regenerar la caché, elimina solo esa carpeta con el
  juego cerrado; se volverá a crear. No se incluyen cachés ya generadas.
- Consulta ENBShaderCache.log, enbseries_guardia.log, ShadowCasterFix.log y
  los registros de los plugins junto a sus archivos si un efecto no aparece.
- Para quitarlo, restaura los archivos que respaldaste y retira los plugins
  nuevos de este complemento. Borrar sus ASI no restaura los shaders que se
  reemplazaron. FILES.sha256 enumera los archivos entregados.
- Para retirar solo el opcional, borra su carpeta _ARREGLO rendimiento hilo
  principal de modloader o pon Activo=0 en RendimientoHiloPrincipal.ini.

CAPTURAS Y ALCANCE
=================
Las 12 imágenes de Screenshots proceden de grabaciones reales. Algunas usan
GTA Mixed y otros recursos visuales de la instalación del autor, que no forman
parte de esta descarga. Los pares OFF/ON identifican el estado mostrado;
no son una garantía de resultados idénticos en cualquier instalación.
El rendimiento, los modelos que emiten luz y el aspecto final dependen del
hardware y de los demás mods. El paquete se ha revisado en disco; no se ha
realizado en esta preparación una prueba nueva de instalación limpia en GTA.

CRÉDITOS
========
Oscar Vidal: plugins, correcciones, efectos adicionales y su integración.
Makarus: SA DirectX 3.0, preset necesario y base de los shaders modificados.
Inari Lú: actualización no oficial necesaria y sus correcciones originales.
Boris Vorontsov: ENBSeries.
Junior_Djjr: Complex Shadows Culling Fix, referencia técnica previa.
stephi.tl: Deferred but Forward SkyGFX, referencia para el estudio de efectos.
Junior_Djjr y Jéssica Natália: Proper Shaders, referencia para efectos de
iluminación y lluvia estudiados durante el desarrollo.
Las referencias técnicas anteriores no convierten esos mods en dependencias.
