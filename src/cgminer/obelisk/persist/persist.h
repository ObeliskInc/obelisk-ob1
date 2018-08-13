// saveThermalConfig saves the voltage and biases+dividers for the specified boardID.
ApiError saveThermalConfig(char *modelName, int chipsPerBoard, int boardID, double currentStringVoltage, int8_t *chipBiases, uint8_t *chipDividers);

// loadThermalConfig loads the voltage and biases+dividers for the specified boardID.
ApiError loadThermalConfig(char *modelName, int chipsPerBoard, int boardID, double *currentStringVoltage, int8_t *chipBiases, uint8_t *chipDividers);
