# Limpiar
sudo rmmod utn-fra-td3-tp5 2>/dev/null
make clean
make all

# Probar GPIO 18
sudo insmod build/utn-fra-td3-tp5.ko gpio_led=18

# Verificar
dmesg | tail -5

# Si hay error, ver el error específico:
dmesg | tail -10
