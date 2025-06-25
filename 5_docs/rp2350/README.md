# Visual Studio Code para Raspberry Pi Pico

El entorno de desarrollo para la Raspberry Pi Pico y  Raspberry Pi Pico 2 va a ser Visual Studio Code. Podemos descargarlo desde este [link](https://code.visualstudio.com/).

Desde 2024, Raspberry Pi desarrolló una extensión oficial para las Raspberry Pi Pico en Visual Studio Code. Podemos buscarla e instalarla desde el menú de extensiones que se encuentra a la izquierda buscando _Raspberry Pi Pico_.

![Raspberry Pi Pico extension](https://www.raspberrypi.com/app/uploads/2024/10/extension-1024x640.png)

> :warning: Esta extensión requiere unas dependencias previas para algunos sistemas operativos. Ver [dependencias](#dependencias).

Una vez que tengamos instalada la extensión, vamos a al menú de esta y vamos a crear un proyecto:

![Create a Raspberry Pi Pico project](https://www.raspberrypi.com/app/uploads/2024/10/create.png)

Del menú de creación de proyecto vamos a tener que elegir:

* Nombre del proyecto
* Tipo de placa que debe corresponder con la placa que vayamos a usar
* Arquitectura (solo para Pico 2 y Pico 2W) que solamente vamos a tildar si usamos arquitectura RISC-V
* Ubicación que es donde va a terminar creándose el proyecto. Normalmente lo vamos a crear dentro de alguno de los directorios del repositorio
* Versión de SDK de la Pico (actualmente v2.1.1)
* Features que nos sirve para habilitar periféricos particulares pero normalmente vamos a habilitarlos desde otro lado
* Soporte para mensajes por consola (stdio) por UART o USB que también habilitaremos desde otro lado
* Opciones adicionales de generación de código que normalmente no vamos a usar
* Debugger que siempre vamos a estar eligiendo CMSIS-DAP

Una vez que elegimos estas cosas, creamos el proyecto y, si es la primera vez que creamos uno, esperamos a que se descargue todo el SDK para el microcontrolador que vamos a usar.

## Estructura de proyecto

Un proyecto creado tiene esta estructura:

```
.
├── .vscode/
│   └── ...
├── build/ 
│   └── ...
├── .gitignore
├── CMakeLists.txt
├── pico_sdk_import.cmake
└── main.c
```

En esta estructura, son particularmente importantes los archivos _CMakeLists.txt_ y el _main.c_, siendo el último donde vamos a escribir nuestra aplicación principal.

> ⚙️ El _CMakeLists.txt_ es un archivo importante para poder configurar dependencias entre bibliotecas y nuestra aplicación. Para más información leer [este](./cmakelists.md) documento.

## Dependencias

### Windows

Para Windows no hay dependencias pero si hay que posteriormente instalar el driver de USB para la Raspberry Pi Pico. Una vez que la tengamos conectada, podemos descargar [Zadig](https://zadig.akeo.ie/) y actualizar el driver al apropiado.

### Linux

Las dependencias se pueden instalar por consola con:

```bash
sudo apt install python3 git tar build-essential
```

### macOS

Instalamos xcode con:

```bash
xcode-select --install
```