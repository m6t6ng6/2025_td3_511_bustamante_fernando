# tp2

Esta actividad práctica debe hacerse en un proyecto independiente llamado `firmware` dentro de este directorio. Se debe entregar el código fuente completo dentro de este directorio ignorando el directorio destino de la compilación (_build_ o _debug_ normalmente) a través de un _.gitignore_.

Este trabajo debe hacerse en una nueva rama llamada `tp2/v1`. Se puede hacer con el comando:

```bash
git checkout -b tp2/v1
```

## Consigna

Implementar una aplicación que haga uso del ADC para leer y mostrar por consola la temperatura. Como sensor, es posible usar el sensor interno que tiene el microcontrolador si se dispone. En caso contrario, usar un sensor de temperatura externo analógico.

### Primera versión

Implementar una tarea que lea el valor del ADC del sensor y lo pase por una cola a otra tarea que se encargue de mostrar la temperatura en Celsius por consola.

Una vez resuelta la consiga, hacer un commit de los cambios:

```bash
git add firmware
git commit -m "Primer consigna de TP2"
```

### Segunda versión

Trabajar con el ADC para que funcione por interrupciones. Una tarea debe disparar periódicamente la conversión del ADC y desde una interrupción se envíe el valor por una cola que lea la tarea que muestra por consola.

Una vez resuelta la consiga, hacer un commit de los cambios:

```bash
git add firmware
git commit -m "Segunda consigna de TP2"
```

## Entrega

Pushear la rama `tp2/v1` al repositorio personal.

```bash
git push origin tp2/v1
```

Luego, crear un pull request a la rama correspondiente del repositorio de la cátedra con el título del pull request de _Entrega de TP2_.