# freertos

Este directorio contiene la biblioteca de FreeRTOS (v202210.01 LTS) que vamos a usar en la cátedra.

> :warning: **No se debe modificar ningún archivo de este directorio**

Para agregar esta biblioteca en el proyecto, incluir en el `CMakeLists.txt` general lo siguiente:

```cmake
# Añadir la subcarpeta donde está la biblioteca FreeRTOS
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../freertos ${CMAKE_BINARY_DIR}/freertos)
# Agrega dependencia al proyecto
target_link_libraries(firmware freertos)
```