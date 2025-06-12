# tp4

Esta actividad práctica debe hacerse en un proyecto independiente llamado `firmware` dentro de este directorio. Se debe entregar el código fuente completo dentro de este directorio ignorando el directorio destino de la compilación (_build_ o _debug_ normalmente) a través de un _.gitignore_.

Este trabajo debe hacerse en una nueva rama llamada `tp4/v1`. Se puede hacer con el comando:

```bash
git checkout -b tp4/v1
```

## Consigna

Implementar una aplicación con un pseudo sistema de control de temperatura

> :warning: Para este trabajo práctico, es necesario tener:
> * Un LCD 16x2 o 20x4 con módulo de I2C
> * Un BMP280
> * Un pulsador + resistencia de pull-up/pull-down
> * Un LED + resistencia de polarización

### Primera versión

Gestionar los recursos del microcontrolador para que **compartiendo el mismo bus de I2C** se pueda ver la temperatura en grados Celsius y la presión en KPa con su unidad en las primeras dos líneas del display LCD. 

La resolución debe contar con múltiples tareas y manejo de colas y semáforos donde corresponda.

Una vez resuelta la consiga, hacer un commit de los cambios:

```bash
git add firmware
git commit -m "Primer consigna de TP4"
```

### Segunda versión

Agregar un pulsador que, al presionarlo, cambia entre pantallas del LCD. Por un lado, debe mostrarse lo planteado en la consigna anterior. Por otro lado, debe mostrarse un setpoint o referencia de temperatura fijo (como 25 grados por ejemplo) en la primera línea y el error entre la temperatura medida y la referencia en la segunda linea.

Generar una salida de PWM con ancho de pulso proporcional al error absoluto expresado en el LCD y conectarla a un LED. Debería brillar con más intensidad mientras menos error haya y apagarse del todo cuando el error sea nulo.

Considerar usar interrupciones y semáforos según corresponda.

Una vez resuelta la consiga, hacer un commit de los cambios:

```bash
git add firmware
git commit -m "Segunda consigna de TP4"
```

## Guía de código

Agregar dentro del directorio de firmware, una guía de código como imagen donde se puedan apreciar:

* Las tareas y sus prioridades junto con una descripción breve de qué hacen
* Interrupciones que se hayan configurado
* Recursos como colas y semáforos
* Relación entre todos estos recursos

Una guía de código de ejemplo puede verse en la página 6 de este [informe de ejemplo](/5_docs/info_catedra/td3_informe_ejemplo.pdf).

## Entrega

Pushear la rama `tp4/v1` al repositorio personal.

```bash
git push origin tp4/v1
```

Luego, crear un pull request a la rama correspondiente del repositorio de la cátedra con el título del pull request de _Entrega de TP4_.