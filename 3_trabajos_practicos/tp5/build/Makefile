# Nombre del modulo del kernel
MNAME := kernel_module
# Directorio donde se encuentra el kernel
KDIR := /lib/modules/$(shell uname -r)/build
# Directorio destino de archivos de compilacion
MDIR := $(PWD)/build

# Codigo fuente para compilar - SOLO kernel_module.c
obj-m += ${MNAME}.o

# Compilacion completa
all:
	@mkdir -p build;                        \
	rsync -a --exclude 'build/' . build/;   \
	cd build;                               \
	make -C ${KDIR} M=${MDIR} modules

# Limpia el directorio de compilacion
clean:
	@cd build;      \
	make -C ${KDIR} M=${MDIR} clean;        \
	cd ..;  \
	rm -rf build

# ==========================================
# REGLAS UTILES
# ==========================================

# Carga el modulo en el kernel
insmod:
	@-sudo rmmod ${MNAME} 2>/dev/null || true
	@sudo insmod build/${MNAME}.ko
	@echo "Módulo cargado. Verificando..."
	@make lsmod

# Quita el modulo del kernel
rmmod:
	@-sudo rmmod ${MNAME} 2>/dev/null && echo "Módulo descargado" || echo "Módulo no estaba cargado"

# Muestra los mensajes del kernel
dmesg:
	@dmesg -T | grep "utn-fra-td3" || echo "No hay mensajes recientes"

# Verifica si el modulo esta cargado
lsmod:
	@if lsmod | grep -q "${MNAME}"; then \
		echo "${MNAME} está cargado"; \
	else \
		echo "${MNAME} NO está cargado"; \
	fi

# Prueba completa
test: clean all insmod
	@sleep 3
	@make dmesg
	@sleep 2
	@make rmmod
	@make dmesg
