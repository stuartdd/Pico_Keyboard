#define CONFIG_FILE "eeprom.dat"  // Name of the Config file
#define CONFIG_LEN 10             // Number of bytes in config file
#define CONFIG_ROTATE 0           // Rotate flag is [0]
#define CONFIG_MENU_MAX 1         // Menu number of lines is [1]
#define CONFIG_DOWN_UP 2          // Swap scroll buttons
#define CONFIG_DIAG 3             // Swap scroll buttons


#define CONFIG_VALUE_TRUE '1'   // A NON '0' character indicates flag is true
#define CONFIG_VALUE_FALSE '0'  // A '0' character indicates flag is false
#define CONFIG_VALUE_CLEAR '0'  // A '0' character indicates default va;ue

uint8_t configFlags[CONFIG_LEN];  // Bytes for config data
bool configLoaded = false;        // Config data is loaded

bool writeConfigData() {
  File f = FatFS.open(CONFIG_FILE, "w");
  if (f) {
    f.write(configFlags, CONFIG_LEN);
    f.close();
    return true;
  } else {
    return false;
  }
}

bool readConfigData() {
  File f = FatFS.open(CONFIG_FILE, "r");
  for (int i = 0; i < CONFIG_LEN; i++) {
    configFlags[i] = CONFIG_VALUE_CLEAR;
  }
  if (f) {
    int pos = 0;
    while (f.available()) {
      configFlags[pos] = char(f.read());
      pos++;
      if (pos >= CONFIG_LEN) {
        break;
      }
    }
    f.close();
    configLoaded = true;
    return true;
  } else {
    return false;
  }
}

// Read int from config flags
uint8_t getConfigUint(int pos) {
  return configFlags[pos] - 48;
}

// Read a bool from config flags
// '0' = false. Any other value is true
bool getConfigBool(int pos) {
  return configFlags[pos] != CONFIG_VALUE_FALSE;
}

// Write value to config flags.
// Input Values are 0..n
// Actual values written are character values 0..n + 48. The value of '0'.
void setConfigUint(int pos, uint8_t v) {
  uint8_t vv = getConfigUint(pos);
  if (v != vv) {
    configFlags[pos] = v + 48;
    writeConfigData();
  }
}

// Set a boolean value in the config flags.
// false == '0'
// true == '1'
void setConfigBool(int pos, bool v) {
  bool vv = getConfigBool(pos);
  if (v != vv) {
    if (v) {
      configFlags[pos] = CONFIG_VALUE_TRUE;
    } else {
      configFlags[pos] = CONFIG_VALUE_FALSE;
    }
    writeConfigData();
  }
}

void flipConfigBool(int pos) {
  setConfigBool(pos, !getConfigBool(pos));
}
