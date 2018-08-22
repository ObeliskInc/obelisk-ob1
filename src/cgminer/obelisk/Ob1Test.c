
#include "Ob1Utils.h"
#include "Ob1API.h"
#include "CSS_SC1Defines.h"
#include "CSS_DCR1Defines.h"
#include <stdio.h>

void testRegWriteRead();
void testHashboardStatus();

// Simple functional test of the utils functions and API functions
void ob1Test()
{
    ob1Initialize();

    // testRegWriteRead();

    testHashboardStatus();
}

// Write a unique value to each M0 register then read them back
void testRegWriteRead()
{
    printf("Starting testRegWriteRead()...\n");
    uint64_t counter = 0;
    clock_t transfer_time = 0;
    for (uint8_t board = 0; board < ob1GetNumHashboardSlots(); board++) {
        for (uint8_t chip = 0; chip < 15; chip++) {
            for (uint8_t engine = 0; engine < ob1GetNumEnginesPerChip(); engine++) {
                switch (ob1GetHashboardModel()) {
                case MODEL_SC1: {
                    // printf("SC1: Writing board=%d chip-%d engine=%d counter=%d\n", board, chip, engine, counter);
                    ApiError result = ob1SpiWriteReg(board, chip, engine, E_SC1_REG_M0, &counter, &transfer_time);
                    if (result != SUCCESS) {
                        printf("SC1 ERROR in %s: Could not write M0 register: board=%d chip-%d engine=%d counter=%d\n", __FUNCTION__, board, chip, engine, counter);
                        return;
                    }
                    break;
                }
                case MODEL_DCR1: {
                    uint32_t counter32bit = (uint32_t)counter;
                    // printf("DCR1: Writing board=%d chip-%d engine=%d counter=%d\n", board, chip, engine, counter32bit);
                    ApiError result = ob1SpiWriteReg(board, chip, engine, E_DCR1_REG_M0, &counter32bit, &transfer_time);
                    if (result != SUCCESS) {
                        printf("DCR1 ERROR in %s: Could not write M0 register: board=%d chip-%d engine=%d counter=%d\n", __FUNCTION__, board, chip, engine, counter);
                        return;
                    }

                    // Now read it back and compare
                }
                }

                counter++;
            }
        }
    }

    counter = 0;
    for (uint8_t board = 0; board < ob1GetNumHashboardSlots(); board++) {
        for (uint8_t chip = 0; chip < 15; chip++) {
            for (uint8_t engine = 0; engine < ob1GetNumEnginesPerChip(); engine++) {
                switch (ob1GetHashboardModel()) {
                case MODEL_SC1: {
                    // Now read it back and compare
                    // printf("SC1: Writing board=%d chip-%d engine=%d counter=%d\n", board, chip, engine, counter);
                    uint64_t readValue = 0;
                    ApiError result = ob1SpiReadReg(board, chip, engine, E_SC1_REG_M0, &readValue, &transfer_time);
                    if (result != SUCCESS) {
                        printf("SC1 ERROR in %s: Could not read M0 register: board=%d chip-%d engine=%d counter=%d\n", __FUNCTION__, board, chip, engine, counter);
                        return;
                    }
                    if (readValue != counter) {
                        printf("SC1 ERROR in %s: Read value did not match counter: board=%d chip-%d engine=%d counter=%d\n", __FUNCTION__, board, chip, engine, counter);
                        return;
                    }
                    break;
                }
                case MODEL_DCR1: {
                    uint32_t counter32bit = (uint32_t)counter;
                    // printf("DCR1: Reading board=%d chip-%d engine=%d counter=%d\n", board, chip, engine, counter32bit);
                    uint32_t readValue = 0;
                    ApiError result = ob1SpiReadReg(board, chip, engine, E_SC1_REG_M0, &readValue, &transfer_time);
                    if (result != SUCCESS) {
                        printf("DCR1 ERROR in %s: Could not read M0 register: board=%d chip-%d engine=%d counter=%d\n", __FUNCTION__, board, chip, engine, counter32bit);
                    }
                    if (readValue != counter32bit) {
                        printf("DCR1 ERROR in %s: Read value did not match counter: board=%d chip-%d engine=%d counter=%d\n", __FUNCTION__, board, chip, engine, counter32bit);
                        return;
                    }
                    break;

                    // Now read it back and compare
                }
                }

                counter++;
            }
        }
    }
    // Successfully wrote and read back every engine's M0 register
    printf("DONE!\n");
}

void testHashboardStatus()
{
    for (uint8_t board = 0; board < ob1GetNumHashboardSlots(); board++) {
        HashboardStatus status = ob1GetHashboardStatus(board);

        printf("Hashboard %d Status\n", board + 1);
        printf("    status.chipTemp=%f\n", status.chipTemp);
        printf("    status.boardTemp=%f\n", status.boardTemp);
        printf("    status.powerSupplyTemp=%f\n", status.powerSupplyTemp);
        printf("    status.asicV1=%f\n", status.asicV1);
        printf("    status.asicV15=%f\n", status.asicV15);
        printf("    status.asicV15IO=%f\n\n", status.asicV15IO);
    }
}
