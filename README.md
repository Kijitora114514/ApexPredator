# ApexPredator ZMK configuration

This repository contains the original o67r configuration and an out-of-tree
Zephyr board/shield pair for initial bring-up of an FDK HY0020 (nRF52832)
keyboard controller.

## HY0020 build target

- Zephyr board name: `hy0020`
- Hardware Model v2 target: `hy0020/nrf52832`
- Standalone test shield: `hy0020_test`
- Future split peripheral shield: `hy0020_test_peripheral`
- SoC: Nordic nRF52832 QFAA, Cortex-M4F, 512 KiB flash, 64 KiB SRAM
- Firmware transport: Bluetooth LE only
- Programming: direct SWD; no USB and no bootloader

The standalone target is the first bring-up target. It scans all 42 matrix
positions and reports ordinary BLE keyboard usages. The peripheral target uses
the same matrix and keymap, but additionally enables ZMK split mode without the
central role.

## Versions and reference boards

`config/west.yml` tracks ZMK `main`. At the time this board was added, ZMK
`main` selected the ZMK Zephyr fork revision `v4.1.0+zmk-fixes` in
`app/west.yml`. This is Zephyr Hardware Model v2, so the HY0020 board uses
`board.yml`, an SoC qualifier, and qualifier-specific DTS/defconfig files.

The board structure and nRF52832 SoC selection were based on these Zephyr 4.1
boards:

- `zephyr/boards/nordic/nrf52dk`: `board.yml`, `Kconfig.nrf52dk`, and the
  `nrf52dk_nrf52832` DTS/defconfig
- `zephyr/boards/nordic/thingy52`: a single-module nRF52832 board using the
  `nrf52832` qualifier
- `zephyr/dts/arm/nordic/nrf52832_qfaa.dtsi`: SoC flash, SRAM, GPIO, clock,
  radio, UICR, and debug hardware

No nRF52840 board definition, legacy Hardware Model v1 board, UF2 definition,
or bootloader partition was copied.

## Clock and power configuration

HY0020 is specified with a 32 MHz high-frequency crystal and a 32.768 kHz
low-frequency crystal. The board enables the Nordic clock node and selects
`CONFIG_CLOCK_CONTROL_NRF_K32SRC_XTAL=y`. The high-frequency radio clock uses
the SoC HFXO path. `NRF5X_REG_MODE_LDO` explicitly keeps the initial build in
LDO mode; the derived `SOC_DCDC_NRF52X` Kconfig symbol is not assigned directly.

P0.21 is configured as nRESET through UICR. SWDIO and SWDCLK are dedicated
debug pads controlled by the Arm debug port and are deliberately not claimed
as GPIO devices in Devicetree. NFC is disabled; P0.09/P0.10 are not used by
the initial shield.

## Flash layout

| Address range | Size | Purpose |
|---|---:|---|
| `0x00000000`–`0x00077FFF` | 480 KiB | ZMK application (`image-0`) |
| `0x00078000`–`0x0007FFFF` | 32 KiB | ZMK Bluetooth/settings storage |

`zephyr,code-partition` points to `image-0`, whose address is zero. There is
no bootloader offset, secondary image slot, USB DFU partition, or UF2 image.

## Matrix wiring

The diode direction is `col2row`. Every number below is an explicit Nordic
GPIO0 pin number.

| Matrix signal | HY0020 pin | Matrix signal | HY0020 pin |
|---|---|---|---|
| col0 | P0.16 | row0 | P0.28 |
| col1 | P0.18 | row1 | P0.30 |
| col2 | P0.20 | row2 | P0.02 |
| col3 | P0.12 | row3 | P0.03 |
| col4 | P0.07 | row4 | P0.04 |
| col5 | P0.08 | row5 | P0.05 |
| col6 | P0.06 | | |

The 6×7 matrix transform contains all 42 row/column positions. The test layer
assigns A–Z, 0–9, four arrow keys, Enter, and Space, with no unused positions.

## Local pristine build

Run these commands from this repository. They initialize a west workspace in
the repository and download the ZMK/Zephyr projects selected by the manifest.

```sh
west init -l config
west update
west zephyr-export
west build -p always -s zmk/app -d build/hy0020_test \
  -b hy0020/nrf52832 -- \
  -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD" \
  -DSHIELD=hy0020_test
```

The optional split peripheral build is:

```sh
west build -p always -s zmk/app -d build/hy0020_test_peripheral \
  -b hy0020/nrf52832 -- \
  -DZMK_CONFIG="$PWD/config" \
  -DZMK_EXTRA_MODULES="$PWD" \
  -DSHIELD=hy0020_test_peripheral
```

Expected direct-programming output:

```text
build/hy0020_test/zephyr/zmk.hex
```

The general `build.yaml` matrix includes both targets. A separate
`build-hy0020.yaml` matrix and `.github/workflows/build-hy0020.yml` workflow
isolate HY0020 validation from unrelated keyboard targets. The reusable action
is configured with `fallback_binary: hex`, so its merged `hy0020-firmware`
artifact contains `hy0020_test.hex` and `hy0020_test_peripheral.hex`.

## SWD programming with a Raspberry Pi Pico probe

Flash the Pico with Raspberry Pi Debug Probe/CMSIS-DAP firmware, then connect
SWDIO, SWDCLK, GND, and VTREF/3.3 V as appropriate for the test board. Do not
power the target from two supplies simultaneously.

With OpenOCD and a CMSIS-DAP Pico probe:

```sh
openocd -f interface/cmsis-dap.cfg -f target/nrf52.cfg \
  -c "adapter speed 1000" \
  -c "program build/hy0020_test/zephyr/zmk.hex verify reset exit"
```

For older Picoprobe firmware/OpenOCD packages, replace the interface file with
`interface/picoprobe.cfg`. A mass erase is not part of the normal update command;
use one only when protection or stale settings specifically require it.

## Bring-up checks still requiring hardware

- Confirm the HY0020 module variant really populates both specified crystals.
- Confirm P0.21 is physically connected to the module reset pad before writing
  UICR reset configuration.
- Verify diode polarity on the assembled matrix matches `col2row`.
- Measure BLE operation and matrix scanning on the 3.0–3.3 V test board before
  enabling DC/DC or optimizing power consumption.
