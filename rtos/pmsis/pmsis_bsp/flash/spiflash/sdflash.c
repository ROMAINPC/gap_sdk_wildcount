/*
 * Authors: RomainPC, ThaisB, SimonP / Wildcount project.
 */

#include "bsp/flash/sdflash.h"

#include "bsp/bsp.h"
#include "pmsis.h"
#include "pmsis/drivers/spi.h"

// select chip
//#define MX25

// select single or quad line
//#define SINGLE_LINE

static int sdflash_open(struct pi_device *bsp_flash_dev) {
    printf("sdflash_open NOT YET IMPLEMENTED\n");
    return -1;
}

void sdflash_close(struct pi_device *bsp_flash_dev) {
    printf("sdflash_close NOT YET IMPLEMENTED\n");
    return;
}

static inline int sdflash_erase_chip(struct pi_device *device) {
    printf("sdflash_erase_chip NOT YET IMPLEMENTED\n");
    return -1;
}

static inline int sdflash_erase_sector(struct pi_device *device,
                                       uint32_t pi_flash_addr) {
    printf("sdflash_erase_sector NOT YET IMPLEMENTED\n");
    return -1;
}

static inline int sdflash_erase(struct pi_device *device,
                                uint32_t pi_flash_addr, int size) {
    printf("sdflash_erase NOT YET IMPLEMENTED\n");
    return -1;
}

static int sdflash_read(struct pi_device *device, uint32_t pi_flash_addr,
                        void *data, uint32_t size) {
    printf("sdflash_read NOT YET IMPLEMENTED\n");
    return -1;
}

static int sdflash_program(struct pi_device *bsp_flash_dev, uint32_t flash_addr,
                           const void *data, uint32_t size) {
    printf("sdflash_program NOT YET IMPLEMENTED\n");
    return -1;
}

static inline int sdflash_reg_set(struct pi_device *device,
                                  uint32_t pi_flash_addr, uint8_t *value) {
    printf("sdflash_reg_set NOT YET IMPLEMENTED\n");
    return -1;
}

static inline int sdflash_reg_get(struct pi_device *device,
                                  uint32_t pi_flash_addr, uint8_t *value) {
    printf("sdflash_reg_get NOT YET IMPLEMENTED\n");
    return -1;
}

int sdflash_copy(struct pi_device *device, uint32_t flash_addr, void *buffer,
                 uint32_t size, int ext2loc) {
    printf("sdflash_copy NOT YET IMPLEMENTED\n");
    return -1;
}

int sdflash_copy_2d(struct pi_device *device, uint32_t flash_addr, void *buffer,
                    uint32_t size, uint32_t stride, uint32_t length,
                    int ext2loc) {
    printf("sdflash_copy_2d NOT YET IMPLEMENTED\n");
    return -1;
}

static int32_t sdflash_ioctl(struct pi_device *device, uint32_t cmd,
                             void *arg) {
    printf("sdflash_ioctl NOT YET IMPLEMENTED\n");
    return -1;
}

static void sdflash_erase_async(struct pi_device *device, uint32_t addr,
                                int size, pi_task_t *task) {
    printf("sdflash_erase_async NOT YET IMPLEMENTED\n");
    return;
}

static void sdflash_reg_set_async(struct pi_device *device, uint32_t addr,
                                  uint8_t *value, pi_task_t *task) {
    printf("sdflash_reg_set_async NOT YET IMPLEMENTED\n");
    return;
}

static void sdflash_reg_get_async(struct pi_device *device, uint32_t addr,
                                  uint8_t *value, pi_task_t *task) {
    printf("sdflash_reg_get_async NOT YET IMPLEMENTED\n");
    return;
}

static int sdflash_copy_async(struct pi_device *device, uint32_t flash_addr,
                              void *buffer, uint32_t size, int ext2loc,
                              pi_task_t *task) {
    printf("sdflash_copy_async NOT YET IMPLEMENTED\n");
    return -1;
}

static int sdflash_copy_2d_async(struct pi_device *device, uint32_t flash_addr,
                                 void *buffer, uint32_t size, uint32_t stride,
                                 uint32_t length, int ext2loc,
                                 pi_task_t *task) {
    printf("sdflash_copy_2d_async NOT YET IMPLEMENTED\n");
    return -1;
}

static void sdflash_program_async(struct pi_device *device, uint32_t addr,
                                  const void *data, uint32_t size,
                                  pi_task_t *task) {
    printf("sdflash_program_async NOT YET IMPLEMENTED\n");
    return;
}

static void sdflash_erase_chip_async(struct pi_device *device,
                                     pi_task_t *task) {
    printf("sdflash_erase_chip_async NOT YET IMPLEMENTED\n");
    return;
}

static void sdflash_erase_sector_async(struct pi_device *device, uint32_t addr,
                                       pi_task_t *task) {
    printf("sdflash_erase_sector_async NOT YET IMPLEMENTED\n");
    return;
}
static void sdflash_read_async(struct pi_device *device, uint32_t addr,
                               void *data, uint32_t size, pi_task_t *task) {
    printf("sdflash_read_async NOT YET IMPLEMENTED\n");
    return;
}

static pi_flash_api_t sdflash_api = {
    .open = &sdflash_open,
    .close = &sdflash_close,
    .ioctl = &sdflash_ioctl,
    .read_async = &sdflash_read_async,
    .program_async = &sdflash_program_async,
    .erase_chip_async = &sdflash_erase_chip_async,
    .erase_sector_async = &sdflash_erase_sector_async,
    .erase_async = &sdflash_erase_async,
    .reg_set_async = &sdflash_reg_set_async,
    .reg_get_async = &sdflash_reg_get_async,
    .copy_async = &sdflash_copy_async,
    .copy_2d_async = &sdflash_copy_2d_async,
    .read = &sdflash_read,
    .program = &sdflash_program,
    .erase_chip = &sdflash_erase_chip,
    .erase_sector = &sdflash_erase_sector,
    .erase = &sdflash_erase,
    .reg_set = &sdflash_reg_set,
    .reg_get = &sdflash_reg_get,
    .copy = &sdflash_copy,
    .copy_2d = &sdflash_copy_2d,
};

void pi_sdflash_conf_init(struct pi_sdflash_conf *conf) {
    conf->flash.api = &sdflash_api;
    bsp_sdflash_conf_init(conf);
    __flash_conf_init(&conf->flash);
    // try to reach max freq on gapoc_a
    // It does not work if we are too close to 50MHz on SPI
    conf->baudrate = 5 * 1000 * 1000;
}
