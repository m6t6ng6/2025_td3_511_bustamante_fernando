#!/bin/bash

# Script de prueba para módulo LED de UTN-FRA-TD3-TP5
# Autor: [Tu Nombre]
# Descripción: Prueba automática del módulo con GPIO por defecto y personalizado

SCRIPT_NAME="test_led_module.sh"
MODULE_NAME="utn-fra-td3-tp5"
MODULE_PATH="./build/${MODULE_NAME}.ko"
DEFAULT_GPIO=17
TEST_DURATION=10

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Función para imprimir mensajes
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Función para limpiar módulos existentes
cleanup_module() {
    print_info "Limpiando módulos existentes..."
    
    # Verificar si el módulo está cargado
    if lsmod | grep -q "${MODULE_NAME}"; then
        print_warning "Módulo ${MODULE_NAME} encontrado cargado. Descargando..."
        sudo rmmod "${MODULE_NAME}" 2>/dev/null
        
        if [ $? -eq 0 ]; then
            print_success "Módulo ${MODULE_NAME} descargado correctamente"
        else
            print_error "Error al descargar el módulo ${MODULE_NAME}"
            return 1
        fi
    else
        print_info "Módulo ${MODULE_NAME} no está cargado"
    fi
    
    # Limpiar compilación
    print_info "Limpiando directorio de compilación..."
    make clean > /dev/null 2>&1
    
    return 0
}

# Función para compilar el módulo
compile_module() {
    print_info "Compilando módulo..."
    
    if make all > /dev/null 2>&1; then
        print_success "Módulo compilado correctamente"
        return 0
    else
        print_error "Error en la compilación del módulo"
        return 1
    fi
}

# Función para cargar módulo con GPIO específico
load_module() {
    local gpio=$1
    print_info "Cargando módulo con GPIO: ${gpio}"
    
    if sudo insmod "${MODULE_PATH}" gpio_led="${gpio}"; then
        print_success "Módulo cargado correctamente con GPIO ${gpio}"
        return 0
    else
        print_error "Error al cargar el módulo con GPIO ${gpio}"
        return 1
    fi
}

# Función para verificar que el módulo está funcionando
verify_module() {
    local gpio=$1
    print_info "Verificando funcionamiento del módulo con GPIO ${gpio}..."
    
    # Esperar un momento para que las tareas empiecen
    sleep 2
    
    # Verificar mensajes en dmesg (sin limpiar el buffer)
    local message_count=$(dmesg | grep -c "LED ON - GPIO ${gpio}")
    local message_count_off=$(dmesg | grep -c "LED OFF - GPIO ${gpio}")
    local total_messages=$((message_count + message_count_off))
    
    if [ "${total_messages}" -ge 2 ]; then
        print_success "Módulo funcionando correctamente. Se detectaron ${total_messages} cambios de estado LED"
        return 0
    else
        print_error "No se detectaron cambios de estado del LED (ON: ${message_count}, OFF: ${message_count_off})"
        # Mostrar últimos mensajes para debug
        print_info "Últimos mensajes del kernel:"
        dmesg | grep "GPIO ${gpio}" | tail -5
        return 1
    fi
}

# Función para ejecutar prueba con duración específica
run_test() {
    local gpio=$1
    local duration=$2
    
    print_info "Iniciando prueba de ${duration} segundos con GPIO ${gpio}"
    
    # Limpiar buffer dmesg antes de empezar la prueba
    sudo dmesg -c > /dev/null
    
    # Mostrar mensajes en tiempo real
    print_info "Monitorizando mensajes del kernel..."
    
    # Ejecutar dmesg -w en background
    (timeout ${duration} dmesg -w 2>/dev/null) &
    local monitor_pid=$!
    
    # Esperar a que termine la prueba
    print_info "Prueba en curso... (${duration} segundos)"
    for ((i=duration; i>0; i--)); do
        echo -ne "Tiempo restante: ${i} segundos\r"
        sleep 1
    done
    echo -ne "\n"
    
    # Terminar el monitor
    kill $monitor_pid 2>/dev/null
    wait $monitor_pid 2>/dev/null
    
    print_success "Prueba completada"
}

# Función para obtener GPIO del usuario
get_user_gpio() {
    while true; do
        read -p "Ingrese el número de GPIO para la prueba (0-53): " user_gpio
        
        # Validar que sea un número
        if ! [[ "$user_gpio" =~ ^[0-9]+$ ]]; then
            print_error "Por favor ingrese un número válido"
            continue
        fi
        
        # Validar rango
        if [ "$user_gpio" -lt 0 ] || [ "$user_gpio" -gt 53 ]; then
            print_error "El GPIO debe estar entre 0 y 53"
            continue
        fi
        
        echo "$user_gpio"
        break
    done
}

# Función principal
main() {
    print_info "Iniciando script de prueba: ${SCRIPT_NAME}"
    echo "================================================"
    
    # Paso 1: Limpieza
    if ! cleanup_module; then
        print_error "Fallo en la limpieza. Saliendo..."
        exit 1
    fi
    
    # Paso 2: Compilación
    if ! compile_module; then
        print_error "Fallo en la compilación. Saliendo..."
        exit 1
    fi
    
    # PRUEBA 1: GPIO por defecto (17)
    echo "================================================"
    print_info "PRUEBA 1: GPIO por defecto (${DEFAULT_GPIO})"
    echo "================================================"
    
    if load_module "${DEFAULT_GPIO}"; then
        run_test "${DEFAULT_GPIO}" "${TEST_DURATION}"
        verify_module "${DEFAULT_GPIO}"
    else
        print_error "No se pudo realizar la prueba con GPIO por defecto"
    fi
    
    # Descargar módulo antes de la siguiente prueba
    sudo rmmod "${MODULE_NAME}" 2>/dev/null
    sleep 2
    
    # PRUEBA 2: GPIO personalizado
    echo "================================================"
    print_info "PRUEBA 2: GPIO personalizado"
    echo "================================================"
    
    user_gpio=$(get_user_gpio)
    
    print_info "GPIO seleccionado: ${user_gpio}"
    
    if load_module "${user_gpio}"; then
        run_test "${user_gpio}" "${TEST_DURATION}"
        verify_module "${user_gpio}"
    else
        print_error "No se pudo realizar la prueba con GPIO ${user_gpio}"
    fi
    
    # Limpieza final
    echo "================================================"
    print_info "Limpieza final..."
    sudo rmmod "${MODULE_NAME}" 2>/dev/null
    make clean > /dev/null 2>&1
    
    print_success "Script de prueba completado"
    echo "================================================"
}

# Manejo de señales
trap 'echo -e "\n${YELLOW}[INTERRUPT]${NC} Script interrumpido por el usuario"; sudo rmmod "${MODULE_NAME}" 2>/dev/null; exit 1' INT TERM

# Verificar que estamos en el directorio correcto
if [ ! -f "gpio_driver.h" ]; then
    print_error "No se encuentra gpio_driver.h. Ejecute el script desde el directorio del proyecto."
    exit 1
fi

# Verificar que tenemos permisos de sudo
if ! sudo -n true 2>/dev/null; then
    print_info "Se necesitan permisos de sudo. Por favor ingrese su contraseña:"
    if ! sudo -v; then
        print_error "Error de autenticación sudo"
        exit 1
    fi
fi

# Ejecutar función principal
main "$@"
