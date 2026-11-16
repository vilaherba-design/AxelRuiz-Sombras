# AxelRuiz-Sombras

Framework de shaders para **Fallout 4 VR 1.2.72**, en la línea de Community Shaders (Skyrim): un plugin F4SE
que intercepta el renderer D3D11 del juego y añade efectos (*features*) independientes que se activan por INI.

**Estado: fase 1 (núcleo invisible), versión 0.1.0.** El plugin captura el dispositivo D3D11, registra cada
shader y cada render target en su log (`Documents\My Games\Fallout4VR\F4SE\AxelRuizSombras.log`) y entrega al
juego su propia copia idéntica de cada shader para demostrar que se pueden sustituir. No cambia nada en pantalla.
Opciones en `F4SE\Plugins\AxelRuizSombras.ini`.

Plan por fases: [docs/PLAN.md](docs/PLAN.md). Notas técnicas del renderer VR: [docs/RE-NOTES.md](docs/RE-NOTES.md).

## Requisitos

- Fallout 4 VR 1.2.72, F4SEVR, VR Address Library for F4SEVR.
- Probado con OpenComposite + VDXR (SteamVR sin probar).

## Compilar

Visual Studio 2022 Build Tools (v143), CMake 3.25+, vcpkg (`VCPKG_ROOT`).

```
git clone --recursive https://github.com/vilaherba-design/AxelRuiz-Sombras
cmake -S AxelRuiz-Sombras -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

En el PC de Axel: `.\build.ps1 -Src <checkout>` compila en `%TEMP%` y deja solo el zip en `C:\Users\cereBRO\Modding`.
Tests sin juego (Linux): `tests/run.sh [Shaders012_VR.fxp catalogo.csv]`.

Clona en una ruta corta: cl.exe no abre las cabeceras de CommonLibF4VR si la ruta pasa de 260 caracteres.

## Licencia y créditos

GPL-3.0 (ver `LICENSE`). Proyecto propio, inspirado en y en deuda con:

- doodlum y los contribuidores de [Community Shaders](https://github.com/doodlum/skyrim-community-shaders) (Skyrim).
- northaxosky y Dear-Modding-FO4: [Fallout 4 Community Shaders](https://github.com/northaxosky/fallout4-community-shaders) y su CommonLibF4.
- brunocatani: [CSAH VR](https://github.com/brunocatani/CSAH-VR), cuya investigación del renderer de FO4VR es la base de la fase 0.
- Los mantenedores de [CommonLibF4VR](https://github.com/ArthurHub/CommonLibF4VR) (submódulo `lib/CommonLibF4VR`, commit 9057052) y de F4SEVR.

Todo código reutilizado de esos proyectos conserva su licencia y su atribución en el propio archivo.
