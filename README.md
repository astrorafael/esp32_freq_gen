# Frequency Generator

4-channel square wave frequency generator based on ESP32's versatile RMT hardware module.

![Console](doc/screenshot1.png?raw=true)

Main characteristics:
* Up to 4 independent channel outputs on GPIO pin #5,, #18 #19, #21
* Frequency range from 0.01 Hz to 500 Khz
* Duty cycle between 0.01 and 0.99, 0.50 by default (square wave)
* command line interface using a serial console. The CLI has a history facility.

# Configure the Project

```
make menuconfig
```

* Set serial port under Serial Flasher Options.

# Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
make -j4 flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

# Example of use

In this example we will define 4 pure square wave (50% DC) frequency generators.

1. It is recommeded to define the oscillators in decreasing order of frequency and let the system assign the GPIO pin:

* 500 KHz
* 5 KHz
* 5 Hz
* 0.05 Hz

```bash
ESP32> create -f 500000
Channel: 07 [stopped]	GPIO: 05	Freq.: 500000.00 Hz	Blocks: 1
ESP32> create -f 5000
Channel: 06 [stopped]	GPIO: 18	Freq.: 5000.00 Hz	Blocks: 1
ESP32> create -f 5
Channel: 05 [stopped]	GPIO: 19	Freq.: 5.00 Hz	Blocks: 1
ESP32> create -f 0.05
Channel: 03 [stopped]	GPIO: 21	Freq.: 0.05 Hz	Blocks: 2
```

Note that most frequencies need only one block of RMT RAM. Extremely low frequencies will take up all the 8 available blocks. See some examples at the bottom of this readme file.

2. Then, we'll save the configuration to non-volatile storage (NVS). Just typing `save` will store all 4 channels.

```bash
ESP32> save
```
3. We can review both the RAM data with `list` and the NVS configuration with `list -n`. 
Note that the oscilators are stopped upon creation.

```bash
ESP32> list
------------------------------------------------------------------
Channel: 03 [stopped]	GPIO: 21	Freq.: 0.05 Hz	DC.: 50%	Blocks: 2
Channel: 05 [stopped]	GPIO: 19	Freq.: 5.00 Hz	DC.: 50%	Blocks: 1
Channel: 06 [stopped]	GPIO: 18	Freq.: 5000.00 Hz	DC.: 50%	Blocks: 1
Channel: 07 [stopped]	GPIO: 05	Freq.: 500000.00 Hz	DC.: 50%	Blocks: 1
------------------------------------------------------------------
ESP32> list -n
------------------------------------------------------------------
Channel: 03 [nvs]	GPIO: 21	Freq.: 0.05 Hz	DC.: 50%	Blocks: 0
Channel: 05 [nvs]	GPIO: 19	Freq.: 5.00 Hz	DC.: 50%	Blocks: 0
Channel: 06 [nvs]	GPIO: 18	Freq.: 5000.00 Hz	DC.: 50%	Blocks: 0
Channel: 07 [nvs]	GPIO: 05	Freq.: 500000.00 Hz	DC.: 50%	Blocks: 0
------------------------------------------------------------------
```

3. We can start all oscillators at once or each one individually. In this example we start them all just typing `start`.

```bash
ESP32> start
Channel: 03 [started]	GPIO: 21	Freq.: 0.05 Hz	DC.: 50%	Blocks: 2
Channel: 05 [started]	GPIO: 19	Freq.: 5.00 Hz	DC.: 50%	Blocks: 1
Channel: 06 [started]	GPIO: 18	Freq.: 5000.00 Hz	DC.: 50%	Blocks: 1
Channel: 07 [started]	GPIO: 05	Freq.: 500000.00 Hz	DC.: 50%	Blocks: 1
```

4. We can also stop them all at once using `stop` or one at a time (i.e `stop -c 7`).
```bash
ESP32> stop
Channel: 03 [stopped]	GPIO: 21	Freq.: 0.05 Hz	DC.: 50%	Blocks: 2
Channel: 05 [stopped]	GPIO: 19	Freq.: 5.00 Hz	DC.: 50%	Blocks: 1
Channel: 06 [stopped]	GPIO: 18	Freq.: 5000.00 Hz	DC.: 50%	Blocks: 1
Channel: 07 [stopped]	GPIO: 05	Freq.: 500000.00 Hz	DC.: 50%	Blocks: 1
```

5. Finally, when we are ready with our configuration it is quite convenient
to load it automatically at boot time and let the ESP32 start them all. For this,
we have to enable the autoload facility with the `àutoload -y`. command

```bash
ESP32> autoload
Autoload at boot time is currently disabled.
ESP32> autoload -y
ESP32> autoload
Autoload at boot time is currently enabled.
```


# Command Reference

The most important command is `help` which shows everything else, as shown below:

```bash
ESP32> help
help 
  Print the list of registered commands

params  -f <Hz> [-d <duty cycle>]
  Computes the frequency generator parameters as well as the needed resources.
  Does not create a frequency generator. 
  -f, --freq=<Hz>  Frequency
  -d, --duty=<duty cycle>  Defaults to 0.5 (50%) if not given

create  -f <Hz> [-d <duty cycle>] [-g <GPIO num>]
  Creates a frequency generator and binds it to a GPIO pin. Does not start it.
  -f, --freq=<Hz>  Frequency
  -d, --duty=<duty cycle>  Defaults to 0.5 (50%) if not given
  -g, --gpio=<GPIO num>  Defaults to -1 if not given

start  [-c <0-7>]
  Starts frequency generator given by channel id. Starts all if no channel is 
  given.
  -c, --channel=<0-7>  RMT channel number.

stop  [-c <0-7>]
  Stops frequency generator given by channel id. Stops all if no channel is gi
  ven.
  -c, --channel=<0-7>  RMT channel number.

delete  [-n] [-c <0-7>]
  Deletes frequency generator and frees its GPIO pin. Deletes all if no channe
  l is given.
  -c, --channel=<0-7>  RMT channel number.
     -n, --nvs  Delete NVS configuration as well.

list  [-xn]
  List all created frequency generators or NVS configuration.
  -x, --extended  Extended listing.
     -n, --nvs  List saved configuration in NVS.

save  [-c <0-7>]
  Saves frequency generator configuration to NVS given by channel id. Saves al
  l if no channel is given.
  -c, --channel=<0-7>  RMT channel number.

load  [-c <0-7>]
  Loads frequency generator configuration from NVS given by channel id. Loads 
  all if no channel is given.
  -c, --channel=<0-7>  RMT channel number.

autoload  [-yn]
  Enables/disables loading configuration at boot time. Displays current mode i
  f no flag is given
     -y, --yes  Enable loading configuration at boot time.
      -n, --no  Disable loading configuration at boot time.

ESP32> 
```

# Design

The frequency generator is based on these formulae:

```
Fclk = ( FREQ_APB / Prescaler ) [Hz] RMT internal clock
Fout = Fclk / N
```

Where `FREQ_APB` is 80 MHz, `Prescaler` is a value between 1 .. 255 and `N` is an arbitrary integer number. As we have two degrees of freedom, `Prescaler` and `N` are heuristically chosen to minimize roundoff errors.

As seen in the figure below, N eventually becomes the number of `Fclk` clock ticks and it is divided into low level tick count `NL` and high level tick count `NH`. The duty cycle becomes `NH / (NH + NL)`.

```
 <------------------------- NRep ---------------->
 <-------- N ------->
 <-- NH --><-- NL -->
 +--------+         +-- ...     +--------+        +-- ...
 |        |         |           |        |        |
-+        +---------+        ---+        +--------+
```

The number `N`, which can be quite large, is decomposed into [32 bit RMT items](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/rmt.html#transmit-data) forming a list that is loaded in RMT's internal RAM. When the RMT hardware reaches the end-of-list item, it loops again. According to ESP32 Technical manual, looping introduces a small `1/Fclk` delay (jitter):

>***Note***: When enabling the continuous transmission mode by setting
>RMT_REG_TX_CONTI_MODE, the transmitter will transmit the data on the channel
>continuously, that is, from the first byte to the last one, then from the first to the last again, and
>so on. In this mode, there will be an idle level lasting one clk_div cycle between N and N+1
>transmissions.

The frequency generator software tries to repeat the items `NRep` times before looping so that it minimizes jitter. Depending on the available RMT RAM this is not always possible. The available RMT internal RAM is divided into 8 64-item blocks and can be flexibily assigned to RMT channels (with some restrictions). Very low frequency generators such as 0.01 Hz can take up the whole RMT RAM. The `params` utility shows us some examples:


```bash
ESP32> params -f 500000
------------------------------------------------------------------
                 FREQUENCY GENERATOR PARAMETERS                   
Final Frequency:	500000.0000 Hz
Final Duty Cycle:	50.00%
Prescaler:		80
N:			2 (1 high + 1 low)
Nitems:			1, repeated x62
Blocks:			1 (64 items each)
Jitter:			1.000 us each 62 times
------------------------------------------------------------------
```

```bash
ESP32> params -f 5
------------------------------------------------------------------
                 FREQUENCY GENERATOR PARAMETERS                   
Final Frequency:	5.0000 Hz
Final Duty Cycle:	50.00%
Prescaler:		250
N:			64000 (32000 high + 32000 low)
Nitems:			1, repeated x62
Blocks:			1 (64 items each)
Jitter:			3.125 us each 62 times
------------------------------------------------------------------
```

```bash
ESP32> params -f 1
------------------------------------------------------------------
                 FREQUENCY GENERATOR PARAMETERS                   
Final Frequency:	1.0000 Hz
Final Duty Cycle:	50.00%
Prescaler:		250
N:			320000 (160000 high + 160000 low)
Nitems:			5, repeated x12
Blocks:			1 (64 items each)
Jitter:			3.125 us each 12 times
------------------------------------------------------------------
```

```bash
ESP32> params -f 0.04
------------------------------------------------------------------
                 FREQUENCY GENERATOR PARAMETERS                   
Final Frequency:	0.0400 Hz
Final Duty Cycle:	50.00%
Prescaler:		250
N:			8000000 (4000000 high + 4000000 low)
Nitems:			123, repeated x1
Blocks:			2 (64 items each)
Jitter:			3.125 us each 1 times
------------------------------------------------------------------
```

```bash
ESP32> params -f 0.03
------------------------------------------------------------------
                 FREQUENCY GENERATOR PARAMETERS                   
Final Frequency:	0.0300 Hz
Final Duty Cycle:	50.00%
Prescaler:		203
N:			13136290 (6568145 high + 6568145 low)
Nitems:			201, repeated x1
Blocks:			4 (64 items each)
Jitter:			2.537 us each 1 times
------------------------------------------------------------------
```

```bash
ESP32> params -f 0.01
------------------------------------------------------------------
                 FREQUENCY GENERATOR PARAMETERS                   
Final Frequency:	0.0100 Hz
Final Duty Cycle:	50.00%
Prescaler:		250
N:			32000000 (16000000 high + 16000000 low)
Nitems:			489, repeated x1
Blocks:			8 (64 items each)
Jitter:			3.125 us each 1 times
------------------------------------------------------------------
```
