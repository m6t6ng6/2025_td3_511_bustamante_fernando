# git

Una guía rápida para git puede encontrarse en [este](./git-cheat-sheet-education.pdf) documento.

## Cómo obtener git

Depende del sistema operativo, las opciones son variadas.

### Windows

Se puede descargar la versión con setup desde la [página oficial](https://git-scm.com/downloads/win) e instalarlo. El proceso de instalación puede dejarse por defecto.

### Ubuntu/Debian

Para distros de Linux tipo Ubuntu o Debian solo hace falta escribir en consola:

```bash
sudo apt ipdate
sudo apt install git
```

### macOS

Teniendo previamente instalado [homebrew](https://brew.sh/), correr en la terminal:

```bash
brew install git
```

## Flujo de trabajo típico

Para cualquier trabajo, evaluación o entrega que haya que realizar, normalmente el procedimiento es el siguiente:

1. Asegurarse de que el repositorio personal está sincronizado con el de la cátedra usando la opción de _Sync fork_ en el repositorio personal.

![Sincronizar repositorio](https://docs.github.com/assets/cb-75605/mw-1440/images/help/repository/sync-fork-dropdown.webp)

2. Asegurarse de estar en la rama principal del repositorio principal escribiendo en la consola:

```bash
git checkout main
```

3. Traer la última versión del repositorio remoto al local en caso de que haya cambios:

```bash
git pull
```

4. Crear, a partir de la rama principal, una rama nueva para trabajar en la entrega. El nombre de la rama dependerá de la actividad:

```bash
git checkout -b [RAMA]
```

5. Una vez que se haya completado la actividad, agregar los archivos a git:

```bash
git add [ARCHIVOS]
```

6. Realizar el commit con el mensaje apropiado:

```bash
git commit -m "[MENSAJE]"
```

7. Pushear la rama con los cambios al repositorio personal remoto:

```bash
git push origin [RAMA]
```

8. Desde GitHub, buscar en el repositorio personal la opción de _Contribute_ para abrir una _Pull Request_ (PR).

![Contribuir](https://coderefinery.github.io/git-intro/_images/github-contribute.png)

9. Hacer el PR al repositorio de la cátedra, asegurando de que la rama que se compara (_compare_) es la de la entrega del repo personal y que la rama a la que se intentan hacer cambios en el repositorio de la cátedra (_base_) sean en la rama asignada.

![Pull Request](https://docs.github.com/assets/cb-87213/mw-1440/images/help/pull_requests/pull-request-review-edit-branch.webp)

> :warning: No se deben bajo ninguna circunstancia intentar hacer PRs a main. Cualquier intento es automáticamente rechazado.

10. Si el PR es aprobado y mergeado al repositorio de la cátedra, volver localmente a la rama principal:

```bash
git checkout main
```

11. Luego, mergear la rama de la entrega que ya fue aprobada:

```bash
git merge [RAMA]
```

12. Por último, pushear la rama principal que existe localmente al repositorio personal remoto, eso deja ambos repositorios con la última versión de la entrega:

```bash
git push origin main
```