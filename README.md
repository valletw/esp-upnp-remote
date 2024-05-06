# ESP32 UPnP remote controller

The purpose of this project is to create an UPnP control point controlled by
infrared or bluetooth remote. It works with custom
[esp-ir-receiver](https://github.com/valletw/esp-ir-receiver) based on ESP32 chip.

## Software design

```mermaid
flowchart
    IR_R([IR Remote])
    BT_R([BT Remote])
    IR_D(IR Decoder)
    BT_S(BT/BLE Stack)
    Commands(Commands Processing)
    UPnP_CP(UPnP Control Point)
    UPnP_R([UPnP Renderer])

    IR_R -. "Infra-Red" .-> IR_D
    BT_R -. Bluetooth .-> BT_S
    IR_D --> Commands
    BT_S --> Commands
    Commands --> UPnP_CP
    UPnP_CP -. WiFi .-> UPnP_R
```

## Build and program

For fast development, this project uses PlatformIO tools.

```shell
# Build project.
pio run

# Clean build files.
pio run --target clean

# Upload firmware.
pio run --target upload

# Erase device.
pio run --target erase
```

## Supported commands

The following control commands are:

- **Play/Pause**: Resume/Suspend the stream
- **Previous**: Request previous stream
- **Next**: Request next stream
- **Volume up**: Increase the volume
- **Volume down**: Decrease the volume
- **Mute**: Cut off the volume
