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

## Infrared remote decoder

Currently, NEC protocol is only supported. Here are the following commands ID
supported:

Brand / Mode        | Code
--------------------|:----:
NEC E553            | 0
Samsung BN59-01175N | 1

Command     | Code 0 | Code 1
------------|:------:|:------:
Play/Pause  | 0x0D   | 0x47
Previous    | 0x1C   | 0x45
Next        | 0x18   | 0x48
Volume Up   | 0x0C   | 0x0F
Volume Down | 0x10   | 0x07
Mute        | 0x04   | 0x0B
