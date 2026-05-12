# LCD Temperature and Humidity Display Design

**Date:** 2026-05-12

## Goal

Display live temperature and humidity readings from the existing DHT20 sensor on a 16x2 I2C LCD at address `0x27`.

## Scope

This design only adds LCD output for:

- Temperature
- Humidity

It does not add:

- System state display
- New FreeRTOS tasks
- Web UI changes
- Queue redesign

## Existing Context

The project already has:

- A `temp_humi_monitor` FreeRTOS task in `src/temp_humi_monitor.cpp`
- DHT20 sensor reads on the I2C bus with `Wire.begin(11, 12)`
- A bundled `LiquidCrystal_I2C` library in `lib/LCD`
- Queue-based publishing of temperature and humidity values to the rest of the system

The current task is the most direct integration point because it already owns sensor acquisition and update timing.

## Chosen Approach

Initialize and update the LCD inside `temp_humi_monitor`.

### Why this approach

- It keeps the LCD synchronized with the exact sensor values just read from DHT20
- It avoids adding another task, queue consumer, or synchronization path
- It minimizes the code change to a single implementation file
- It matches the current project style, where sensor-specific behavior is colocated with the sensor task

## Hardware Assumptions

- LCD type: `1602`
- Interface: `I2C`
- Address: `0x27`
- Shared I2C bus with DHT20
- Existing I2C pins remain `SDA=11`, `SCL=12`

## Display Format

The LCD shows two fixed rows:

Row 1:
`Temp: xx.x C`

Row 2:
`Humi: yy.y %`

Values refresh on the same interval as the DHT20 task, currently every 5 seconds.

## Error Handling

If DHT20 returns invalid data (`NaN`), the LCD must not show stale numeric formatting from the failed sample. Instead it should display:

Row 1:
`Temp: Error`

Row 2:
`Humi: Error`

The task should continue running and retry on the next scheduled read.

## File Changes

Modify:

- `src/temp_humi_monitor.cpp`

No new files are required for the runtime feature.

## Initialization Flow

Inside `temp_humi_monitor`:

1. Start the shared I2C bus with `Wire.begin(11, 12)`
2. Initialize DHT20
3. Construct and initialize `LiquidCrystal_I2C lcd(0x27, 16, 2)`
4. Enable LCD backlight
5. Print an initial startup message or first valid reading

## Update Flow

On each task loop:

1. Read temperature and humidity from DHT20
2. Validate the values
3. Push values to existing RTOS queues as already implemented
4. Update the LCD rows with either numeric values or `Error`
5. Delay for the existing sample interval

## Concurrency Notes

No additional mutex is planned for the LCD because the display is written from a single task under this design. If future features introduce LCD writes from other tasks, ownership will need to be revisited.

## Testing Strategy

### Build Verification

- Build the firmware with PlatformIO and confirm compilation succeeds

### Runtime Verification

- Confirm LCD powers on and backlight is enabled
- Confirm line 1 shows temperature
- Confirm line 2 shows humidity
- Confirm values update every 5 seconds
- Confirm invalid DHT20 reads show `Error` on both lines

## Risks

- LCD and DHT20 share the same I2C bus; incorrect initialization ordering could break one or both devices
- String formatting must fit inside 16 characters per row
- Some LCD modules may require a different address even if labeled similarly; this design assumes `0x27` is correct

## Out of Scope

- Auto-detecting LCD address
- Displaying additional sensors
- Animated or scrolling LCD content
- Refactoring the task architecture
