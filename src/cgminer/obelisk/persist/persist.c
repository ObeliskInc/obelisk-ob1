#include <stdio.h>
#include <stdint.h>
#include "../Ob1Defines.h"

ApiError loadThermalConfig(char *name, int chipsPerBoard, int boardID, double *currentStringVoltage, int8_t *chipBiases, uint8_t *chipDividers)
{
    char path[40];
    snprintf(path, sizeof(path), "settings_%s_%d.bin", name, boardID);
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return GENERIC_ERROR;
    }
    fread(currentStringVoltage, sizeof(double), 1, file);
    fread(chipBiases, sizeof(int8_t), chipsPerBoard, file);
    fread(chipDividers, sizeof(uint8_t), chipsPerBoard, file);
    fclose(file);
    if (ferror(file) != 0) {
        return GENERIC_ERROR;
    }
    return SUCCESS;
}

ApiError saveThermalConfig(char *name, int chipsPerBoard, int boardID, double currentStringVoltage, int8_t *chipBiases, uint8_t *chipDividers)
{
    char path[40];
    char tmppath[40];
    snprintf(path, sizeof(path), "settings_%s_%d.bin", name, boardID);
    snprintf(tmppath, sizeof(tmppath), "settings_%s_%d.bin_tmp", name, boardID);
    FILE *file = fopen(tmppath, "wb");
    if (file == NULL) {
        return GENERIC_ERROR;
    }
    fwrite(&currentStringVoltage, sizeof(double), 1, file);
    fwrite(chipBiases, sizeof(int8_t), chipsPerBoard, file);
    fwrite(chipDividers, sizeof(uint8_t), chipsPerBoard, file);
    fflush(file);
    fclose(file);
    if (ferror(file) != 0) {
        return GENERIC_ERROR;
    }
    if (rename(tmppath, path) != 0) {
        return GENERIC_ERROR;
    }
    return SUCCESS;
}
