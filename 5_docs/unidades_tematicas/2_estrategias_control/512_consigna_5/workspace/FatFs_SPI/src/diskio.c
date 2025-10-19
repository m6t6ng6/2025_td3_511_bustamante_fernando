// sdmm.c - Adaptado para Raspberry Pi Pico SPI hardware

#include "ff.h"
#include "diskio.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define SPI_PORT spi0
#define PIN_MISO 4
#define PIN_CS   1
#define PIN_SCK  2
#define PIN_MOSI 3

#define CS_H() gpio_put(PIN_CS, 1)
#define CS_L() gpio_put(PIN_CS, 0)

static DSTATUS Stat = STA_NOINIT;
static BYTE CardType;

void spi_sd_init(void) {
    spi_init(SPI_PORT, 1000 * 1000); // 1 MHz al inicio
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    CS_H();
}

static void dly_us(UINT n) {
    sleep_us(n);
}

static void xmit_mmc(const BYTE* buff, UINT bc) {
    spi_write_blocking(SPI_PORT, buff, bc);
}

static void rcvr_mmc(BYTE *buff, UINT bc) {
    for (UINT i = 0; i < bc; i++) {
        BYTE dummy = 0xFF;
        spi_write_read_blocking(SPI_PORT, &dummy, &buff[i], 1);
    }
}

static int wait_ready(void) {
    BYTE d;
    UINT tmr;
    for (tmr = 5000; tmr; tmr--) {
        rcvr_mmc(&d, 1);
        if (d == 0xFF) break;
        dly_us(100);
    }
    return tmr ? 1 : 0;
}

static void deselect(void) {
    BYTE d;
    CS_H();
    rcvr_mmc(&d, 1);
}

static int select(void) {
    BYTE d;
    CS_L();
    rcvr_mmc(&d, 1);
    if (wait_ready()) return 1;
    deselect();
    return 0;
}

static int rcvr_datablock(BYTE *buff, UINT btr) {
    BYTE d[2];
    UINT tmr;
    for (tmr = 1000; tmr; tmr--) {
        rcvr_mmc(d, 1);
        if (d[0] != 0xFF) break;
        dly_us(100);
    }
    if (d[0] != 0xFE) return 0;
    rcvr_mmc(buff, btr);
    rcvr_mmc(d, 2);
    return 1;
}

static int xmit_datablock(const BYTE *buff, BYTE token) {
    BYTE d[2];
    if (!wait_ready()) return 0;
    d[0] = token;
    xmit_mmc(d, 1);
    if (token != 0xFD) {
        xmit_mmc(buff, 512);
        rcvr_mmc(d, 2);
        rcvr_mmc(d, 1);
        if ((d[0] & 0x1F) != 0x05) return 0;
    }
    return 1;
}

static BYTE send_cmd(BYTE cmd, DWORD arg) {
    BYTE n, d, buf[6];
    if (cmd & 0x80) {
        cmd &= 0x7F;
        n = send_cmd(55, 0);
        if (n > 1) return n;
    }
    if (cmd != 12) {
        deselect();
        if (!select()) return 0xFF;
    }
    buf[0] = 0x40 | cmd;
    buf[1] = (BYTE)(arg >> 24);
    buf[2] = (BYTE)(arg >> 16);
    buf[3] = (BYTE)(arg >> 8);
    buf[4] = (BYTE)arg;
    n = (cmd == 0) ? 0x95 : (cmd == 8 ? 0x87 : 0x01);
    buf[5] = n;
    xmit_mmc(buf, 6);
    if (cmd == 12) rcvr_mmc(&d, 1);
    n = 10;
    do rcvr_mmc(&d, 1); while ((d & 0x80) && --n);
    return d;
}

DSTATUS disk_status(BYTE drv) {
    if (drv) return STA_NOINIT;
    return Stat;
}

DSTATUS disk_initialize(BYTE drv) {
    BYTE n, ty, cmd, buf[4];
    UINT tmr;
    DSTATUS s;

    if (drv) return RES_NOTRDY;
    spi_sd_init();
    dly_us(10000);
    for (n = 10; n; n--) rcvr_mmc(buf, 1);

    ty = 0;
    if (send_cmd(0, 0) == 1) {
        if (send_cmd(8, 0x1AA) == 1) {
            rcvr_mmc(buf, 4);
            if (buf[2] == 0x01 && buf[3] == 0xAA) {
                for (tmr = 1000; tmr; tmr--) {
                    if (send_cmd(41 | 0x80, 1UL << 30) == 0) break;
                    dly_us(1000);
                }
                if (tmr && send_cmd(58, 0) == 0) {
                    rcvr_mmc(buf, 4);
                    ty = (buf[0] & 0x40) ? 6 : 2;
                }
            }
        } else {
            if (send_cmd(41 | 0x80, 0) <= 1) {
                ty = 2; cmd = 41 | 0x80;
            } else {
                ty = 1; cmd = 1;
            }
            for (tmr = 1000; tmr; tmr--) {
                if (send_cmd(cmd, 0) == 0) break;
                dly_us(1000);
            }
            if (!tmr || send_cmd(16, 512) != 0) ty = 0;
        }
    }
    CardType = ty;
    s = ty ? 0 : STA_NOINIT;
    Stat = s;
    deselect();
    return s;
}

DRESULT disk_read(BYTE drv, BYTE *buff, LBA_t sector, UINT count) {
    BYTE cmd;
    DWORD sect = (DWORD)sector;
    if (disk_status(drv) & STA_NOINIT) return RES_NOTRDY;
    if (!(CardType & 4)) sect *= 512;
    cmd = (count > 1) ? 18 : 17;
    if (send_cmd(cmd, sect) == 0) {
        do {
            if (!rcvr_datablock(buff, 512)) break;
            buff += 512;
        } while (--count);
        if (cmd == 18) send_cmd(12, 0);
    }
    deselect();
    return count ? RES_ERROR : RES_OK;
}

DRESULT disk_write(BYTE drv, const BYTE *buff, LBA_t sector, UINT count) {
    DWORD sect = (DWORD)sector;
    if (disk_status(drv) & STA_NOINIT) return RES_NOTRDY;
    if (!(CardType & 4)) sect *= 512;
    if (count == 1) {
        if ((send_cmd(24, sect) == 0) && xmit_datablock(buff, 0xFE))
            count = 0;
    } else {
        if (CardType & 2) send_cmd(23 | 0x80, count);
        if (send_cmd(25, sect) == 0) {
            do {
                if (!xmit_datablock(buff, 0xFC)) break;
                buff += 512;
            } while (--count);
            if (!xmit_datablock(0, 0xFD)) count = 1;
        }
    }
    deselect();
    return count ? RES_ERROR : RES_OK;
}

DRESULT disk_ioctl(BYTE drv, BYTE ctrl, void *buff) {
    DRESULT res;
    BYTE csd[16];
    DWORD cs;
    if (disk_status(drv) & STA_NOINIT) return RES_NOTRDY;
    res = RES_ERROR;
    switch (ctrl) {
    case CTRL_SYNC:
        if (select()) res = RES_OK;
        break;
    case GET_SECTOR_COUNT:
        if ((send_cmd(9, 0) == 0) && rcvr_datablock(csd, 16)) {
            if ((csd[0] >> 6) == 1) {
                cs = ((DWORD)(csd[7] & 0x3F) << 16) | ((WORD)csd[8] << 8) | csd[9];
                *(LBA_t*)buff = (cs + 1) << 10;
            } else {
                res = RES_PARERR;
            }
            res = RES_OK;
        }
        break;
    case GET_BLOCK_SIZE:
        *(DWORD*)buff = 128;
        res = RES_OK;
        break;
    default:
        res = RES_PARERR;
    }
    deselect();
    return res;
}
