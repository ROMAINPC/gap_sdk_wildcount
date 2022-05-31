

#ifndef __BSP__FLASH__SDFLASH_H__
#define __BSP__FLASH__SDFLASH_H__

#include "bsp/flash.h"

/**
 * @addtogroup Flash
 * @{
 */

/**
 * @defgroup sdflash sdflash
 *
 */

/**
 * @addtogroup sdflash
 * @{
 */

/**@{*/

/** \struct sdflash_conf
 * \brief sdflash configuration structure.
 *
 * This structure is used to pass the desired sdflash configuration to the
 * runtime when opening the device.
 */
struct pi_sdflash_conf {
    struct pi_flash_conf flash; /*!< Generic flash configuration. */
    int spi_cs;                 /*!< Chip select where the flash is connected.*/
    int spi_itf;                /*!< Chip select where the flash is connected.*/
    uint32_t baudrate;          /*!< baudrate of the underlying interface. */
};

/** \brief Initialize an sdflash configuration with default values.
 *
 * The structure containing the configuration must be kept alive until the
 * sdflash device is opened.
 *
 * \param conf A pointer to the sdflash configuration.
 */
void pi_sdflash_conf_init(struct pi_sdflash_conf *conf);

//!@}

/**
 * @} end of sdflash
 */

/**
 * @} end of Flash
 */

#endif
