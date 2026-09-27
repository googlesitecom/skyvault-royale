# Avisos de terceros / Third-party notices

SKYVAULT Royale es 100% código y arte original (todo el arte del juego —
personajes, armas, terreno, vegetación, sonidos — se genera por código).
Para las piezas auxiliares de infraestructura se usan estas librerías,
cada una con su licencia permisiva correspondiente:

## Incluidas en el repositorio (`third_party/`)

| Librería | Uso | Licencia |
|---|---|---|
| [miniaudio](https://miniaud.io/) (`miniaudio.h/.c`) | salida de audio (escritorio) | MIT-0 / Public Domain |
| [stb_image.h](https://github.com/nothings/stb) | decodificación de imágenes del loader GLB | Public Domain / MIT |
| [font8x8](https://github.com/dhepper/font8x8) (`font8x8_basic.h`) | fuente de mapa de bits para la UI | MIT (Daniel Hepper) |

Los avisos de copyright completos están al inicio de cada archivo.

## Descargadas al compilar (CMake FetchContent, no se distribuyen aquí)

| Librería | Uso | Licencia |
|---|---|---|
| [GLFW](https://www.glfw.org) 3.4 | ventana, entrada y contexto OpenGL | zlib |
| [GLM](https://github.com/g-truc/glm) 1.0.1 | matemáticas de vectores/matrices | MIT |

## Toolchain

| Herramienta | Uso |
|---|---|
| [Emscripten](https://emscripten.org) | compilación a WebAssembly/WebGL2 |
| GCC / CMake | compilación nativa y sistema de build |
