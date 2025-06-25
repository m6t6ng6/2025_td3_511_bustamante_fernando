# cmakelists

CMake es una herramienta de automatización de compilación que genera archivos para buildear específicos para el sistema a partir de una configuración abstracta. 

En nuestros proyectos para la Raspberry Pi Pico, CMake es especialmente importante porque permite organizar y gestionar eficientemente múltiples archivos fuente, bibliotecas y configuraciones específicas del hardware y del SDK de Raspberry Pi. El archivo CMakeLists.txt es donde se define cómo se debe compilar el proyecto: qué archivos incluir, qué dependencias usar, qué estándares seguir, etc.

## Contenidos

- [Lista de comandos de CMake](#lista-de-comandos-de-cmake)
   * [project(project_name)](#projectproject_name)
   * [add_subdirectory(source_dir binary_dir)](#add_subdirectorysource_dir-binary_dir)
   * [add_executable(name sources)](#add_executablename-sources)
   * [add_library(name type sources)](#add_libraryname-type-sources)
   * [target_link_libraries(target item)](#target_link_librariestarget-item)
   * [target_include_directories(target scope item)](#target_include_directoriestarget-scope-item)
   * [include(file)](#includefile)
- [CMakeLists.txt para bibliotecas](#cmakeliststxt-para-bibliotecas)
   * [Estructura para biblioteca](#estructura-para-biblioteca)
   * [Contenido de CMakeLists.txt](#contenido-de-cmakeliststxt)
   * [Incluir biblioteca en el proyecto](#incluir-biblioteca-en-el-proyecto)
- [Soporte para mensajes por consola](#soporte-para-mensajes-por-consola)

## Lista de comandos de CMake

No es el objetivo de este documento listar todos los comandos de CMake, para eso está disponible la lista completa en la documentación [aquí](https://cmake.org/cmake/help/latest/manual/cmake-commands.7.html).

Vamos a limitarnos a mencionar algunos comandos que comúnmente vamos a usar nosotros en la cátedra en nuestro CMakeLists.txt.

### project(project_name)

Inicializa el nombre del proyecto con `project_name` y setea la variable `PROJECT_NAME` para todo el archivo. Este coincide con el nombre que le dimos al proyecto al crearlo desde la extensión. Ejemplo:

```cmake
# El proyecto pasa a llamarse firmware
project(firmware)
```

Más info: [project](https://cmake.org/cmake/help/latest/command/project.html).

### add_subdirectory(source_dir binary_dir)

Añade a la compilación un directorio donde se encuentra otro `CMakeLists.txt` con la instrucciones de cómo compilarlo. Este comando es especialmente útil cuando incluímos alguna biblioteca.

Se debe especificar la ruta (absoluta o relativa al directorio actual) con `source_dir` donde se encuentra este directorio que se quiere incluir. También se debe especificar con `binary_dir` la ruta donde se dejan los archivos de salida del compilador. Ejemplo:

```cmake
# Incluye el directorio freertos que está un directorio atrás, los archivos del compilador los deja en build/freertos
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../freertos ${CMAKE_BINARY_DIR}/freertos)
```

Más info: [add_subdirectory](https://cmake.org/cmake/help/latest/command/add_subdirectory.html).

### add_executable(name sources)

Crea un archivo ejecutable (compilado) con el nombre `name` a partir de los archivos de código fuente que se listen en `sources`. Ejemplo:

```cmake
# Crea un ejecutable main a partir de main.c
add_executable(main main.c)
```

Más info: [add_executable](https://cmake.org/cmake/help/latest/command/add_executable.html#command:add_executable).

### add_library(name type sources)

Crea una biblioteca con el nombre `name` a partir de los archivos de código fuente especificados en `sources`. En `type` se puede indicar el tipo de biblioteca, siendo `SHARED` la que es de nuestro interés ya que sirve para configurar que se va a linkear con otros targets. Ejemplo:

```cmake
# Crea la biblioteca estática bmp280 a partir de bmp280.c
add_library(bmp280 STATIC bmp280.c)
```

Más info: [add_library](https://cmake.org/cmake/help/latest/command/add_library.html#command:add_library).

### target_link_libraries(target item)

Linkea a `target` que debe ser el nombre de un [ejecutable](#add_executablename-sources) o de una [biblioteca](#add_libraryname-type-sources) las bibliotecas indicadas en `item`. Este comando es útil cuando tenemos un proyecto o una biblioteca que depende de otras bibliotecas. Ejemplos:

```cmake
# La biblioteca bmp280 depende de la biblioteca de hardware_i2c
target_link_libraries(bmp280 hardware_i2c)
```

```cmake
# El proyecto firmware depende de freertos y de hardware_adc
target_link_libraries(firmware freertos hardware_adc)
```

Más info: [target_link_libraries](https://cmake.org/cmake/help/latest/command/target_link_libraries.html).

### target_include_directories(target scope item)

Este comando agrega a `target` que puede ser el nombre de un [ejecutable](#add_executablename-sources) o de una [biblioteca](#add_libraryname-type-sources) los directorios con `.h` que se listen en `item`.

El parámetro `scope` define si los directorios que se incluyan pueden ser vistos únicamente por `target` o por cualquier otro proyecto que lo incluya. Los casos que vamos a ver son:

* `PUBLIC` que permite que los directorios que se incluyen en target puedan ser vistos por otro target. Útil a nivel biblioteca. Ejemplo:

```cmake
# Los archivos dentro de inc van a ser vistos por cualquiera que incluya la biblioteca bmp280
target_include_directories(bmp280 PUBLIC inc)
```

* `PRIVATE` que restringe los archivos incluídos sólo al target del que se está hablando. Ejemplo:

```cmake
# El directorio inc solo puede ser visto por el proyecto firmware
target_include_directories(firmware PRIVATE inc)
```

Más info: [target_include_directories](https://cmake.org/cmake/help/latest/command/target_include_directories.html).

### include(file)

Carga y corre el código de CMake que se encuentre en el archivo especificado en `file`. Esto es útil para incluir comandos de compilación de alguna biblioteca que tengamos o un SDK. La ruta al archivo puede ser relativa al directorio actual o absoluta. Ejemplo:

```cmake
# Incluye el SDK de la Pico desde el archivo
include(pico_sdk_import.cmake)
```

Más info: [include](https://cmake.org/cmake/help/latest/command/include.html).

## CMakeLists.txt para bibliotecas

Muchas veces va a ser necesario escribir código para un sensor, pantalla o actuador. Lo más recomendable es poder aislar el código en un directorio con su propio CMakeLists.txt y luego incluirlo desde el proyecto.

### Estructura para biblioteca

No es totalmente requerido que se estructure todo como se va a explicar pero es lo que más recomendamos desde la cátedra como estándar. Suponiendo que sea una biblioteca para un lcd, tendremos:

```
lcd
├── inc
│   └── lcd.h
├── src
│   └── lcd.c
└── CMakeLists.txt
```

### Contenido de CMakeLists.txt

Independientemente del contenido del .h y .c, los comandos que vamos a necesitar de CMake son [add_library](#add_libraryname-type-sources), [target_include_directories](#target_include_directoriestarget-scope-item) y [target_link_libraries](#target_link_librariestarget-item). Sería típico que en el CMakeLists.txt tengamos algo como esto:

```cmake
# Crea la biblioteca LCD y le agrega el lcd.c
add_library(lcd STATIC src/lcd.c)

# Agrega el directorio de includes a la biblioteca de forma pública
target_include_directories(lcd PUBLIC inc)

# Añade dependencias de la biblioteca al SDK de la Pico
target_link_libraries(lcd hardware_i2c)
```

### Incluir biblioteca en el proyecto

Suponiendo una estructura de proyecto como esta:

```firmware
├── .vscode/
├── pico_sdk_import.cmake
├── firmware.c
└── CMakeLists.txt
lcd
├── inc
│   └── lcd.h
├── src
│   └── lcd.c
└── CMakeLists.txt
```

Donde firmware es el nombre del proyecto, simplemente tenemos que agregar la biblioteca desde el CMakeLists.txt de firmware con los comandos [add_subdirectory](#add_subdirectorysource_dir-binary_dir) y [target_link_libraries](#target_link_librariestarget-item):

```cmake
# Agrega el subdirectorio desde la ruta del CMakeLists.txt de firmware
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lcd ${CMAKE_BINARY_DIR}/lcd)

# Añade la dependencia de firmware a lcd
target_link_libraries(firmware lcd)
```

## Soporte para mensajes por consola

En nuestros proyectos, vamos a tener la posiblidad de usar la consola para leer y mostrar mensajes a partir de las funciones estándares de `stdio.h`. Sin embargo, es necesario en nuestro CMakeLists.txt habilitar el soporte para estos mensajes desde las líneas:

```cmake
# Deshabilita mensajes por UART para el proyecto firmware
pico_enable_stdio_uart(firmare 0)
# Habilita mensajes por USB para el proyecto firmware
pico_enable_stdio_usb(firmware 1)
```

Más info: [pico_enable_stdio_usb](https://www.raspberrypi.com/documentation/pico-sdk/runtime.html#group_pico_stdio_usb).
Más info: [pico_enable_stdio_uart](https://www.raspberrypi.com/documentation/pico-sdk/runtime.html#group_pico_stdio_uart).
