/**
 * ============================================================================
 * EEPROMManager.h
 * ============================================================================
 * Manages loading, validating, and saving of RobotParameters to the ATmega2560
 * EEPROM. Implements CRC16 validation to detect corruption.
 *
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

#include <Arduino.h>
#include "Config.h"

class EEPROMManager {
public:
    /**
     * @brief Initialize parameters from EEPROM. If corrupt or uninitialized,
     *        loads defaults and writes them to EEPROM.
     */
    static void init();

    /**
     * @brief Get immutable reference to current parameters.
     * @return const RobotParameters& 
     */
    static const RobotParameters& getParams();

    /**
     * @brief Save new parameters to EEPROM (updates CRC).
     * @param params The new parameters to save.
     */
    static void saveParams(const RobotParameters& params);

    /**
     * @brief Reset parameters to compiled defaults and save.
     */
    static void resetToDefaults();

private:
    static RobotParameters current_params_;

    static void loadDefaultParams(RobotParameters& params);
    static uint16_t calculateCRC16(const uint8_t* data, size_t length);
};

#endif // EEPROM_MANAGER_H
