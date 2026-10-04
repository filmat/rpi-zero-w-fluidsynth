# Yocto for Raspberry Pi Zero W (MIDI synthesizer)

A Yocto (`scarthgap`) image for the Raspberry Pi Zero W that works as a
simple headless synthesizer. USB MIDI controllers (tested with an Arturia
KeyStep 37 and an Akai MPK Mini MK3) are connected to fluidsynth
automatically, and the sound comes out of an I2S DAC (Waveshare PCM5122).
The synthesizer starts at boot, so it plays without a console or a monitor.
The image also provides WiFi that automatically connects to saved networks,
SSH (dropbear) and a serial console.

## What is in this repo

The repo contains only the `meta-local` layer and `kas.yml`, which describes
where to fetch the rest (poky, meta-openembedded, meta-raspberrypi) and how to
configure the build. The upstream layers are not stored here.

```
meta-local/
├── conf/layer.conf
├── conf/machine/raspberrypi0-wifi-synth.conf   machine: UART console, USB host (dwc2), I2S DAC overlay
├── recipes-core/images/rpi0-synth-image.bb     image (core-image-minimal + kernel modules + WiFi + SSH + alsa-utils + fluidsynth + soundfont + autostart)
├── recipes-connectivity/wpa-supplicant/        WiFi network configuration
├── recipes-core/init-ifupdown/                 wlan0 in /etc/network/interfaces
├── recipes-bsp/bootfiles/                      rpi-bootfiles fetch fix (checksum)
├── recipes-kernel/linux/                       shallow git clone of the kernel
└── recipes-multimedia/
    ├── fluidsynth/                             fluidsynth built with ALSA only (no pulseaudio)
    ├── soundfonts/                             TimGM6mb soundfont (GPL-2.0), fetched from GitHub
    ├── synth-autostart/                        init script that starts fluidsynth, and synth-connect
    └── pc-bridge/                              CC 3 -> Program Change, synth knobs -> SoundFont NRPN
```

The custom machine `raspberrypi0-wifi-synth` inherits from `raspberrypi0-wifi`
(meta-raspberrypi) and adds `raspberrypi0-wifi` to `MACHINEOVERRIDES`, so the
upstream overrides (e.g. the kernel defconfig) keep applying to it. It adds
the `config.txt` lines for the serial console (`disable-bt`, `enable_uart`),
for USB host mode (`dwc2,dr_mode=host`) and for the sound card
(`iqaudio-dac`).

## Hardware

- Raspberry Pi Zero W v1.1
- Waveshare USB HUB HAT on the GPIO header, connected with a
  microUSB-to-microUSB cable to the board's OTG port. The port runs in host
  mode (`dwc2,dr_mode=host`). **Do not unplug USB devices from it while the
  board is running!** See "Do not unplug USB devices while the board is
  running" in the "Synthesizer" section.
- Serial console: the HUB HAT has a CP2102 USB-to-UART converter, reached
  through its "USB TO UART" micro-USB port (the two switches on the back of
  the HAT set to ON/ON), 115200 baud. The HAT covers the GPIO header, so an
  adapter wired to pins 6, 8 and 10 does not fit next to it. Bluetooth is
  disabled (`disable-bt`) so the full UART is on GPIO14/15. On a Linux host
  the console shows up as `/dev/ttyUSB0` (the user has to be in the
  `dialout` group). In a VirtualBox VM the USB filter has to pass the CP2102
  through, and Windows needs the Silicon Labs CP210x driver.
- Power: the "PWR IN" port of the board. The "USB TO UART" port of the HAT
  also powers the board (it started with only that cable connected), and the
  Waveshare documentation does not say whether the two inputs are protected
  against each other. Use one power source at a time, and connect the UART
  cable only when you need the console.
- Waveshare PCM5122 Audio Board (A) (I2S DAC) on the GPIO header. It has a
  HAT EEPROM (see `/proc/device-tree/hat/`), but the firmware does not load
  its overlay by itself. It is enabled by the `iqaudio-dac` overlay, the ALSA
  card is called `IQaudIODAC`. Active speakers (we used Edifier) are
  connected to its output.
- MIDI controllers on the hub: Arturia KeyStep 37 (MIDI channel 15) and
  Akai MPK Mini MK3 (channel 16, drum pads on channel 10). The KeyStep is powered from its own USB
  charger through the data/power splitter that comes with it, and only the
  data leg goes to the hub. We do not know whether that leg also carries 5 V.

## Building

1. Install the host dependencies (Debian; list from the Yocto documentation):

   ```bash
   sudo apt install gawk wget git diffstat unzip texinfo gcc build-essential \
       chrpath socat cpio python3 python3-pip python3-pexpect xz-utils \
       debianutils iputils-ping python3-git python3-jinja2 libegl1 \
       libsdl2-dev python3-subunit mesa-common-dev zstd lz4 file locales \
       libacl1 pipx
   pipx install kas
   ```

2. Configure WiFi. The file with the PSK hashes is not in the repo
   (it is in `.gitignore`), create it from the template:

   ```bash
   cd meta-local/recipes-connectivity/wpa-supplicant/wpa-supplicant
   cp wpa_supplicant.conf-sane.example wpa_supplicant.conf-sane
   wpa_passphrase "SSID" "PASSWORD" | grep -v '#psk'   # paste the result into the file
   ```

   Add one `network={}` block per network. `priority` decides which network
   is preferred when several are in range (higher wins). A phone hotspot must
   use WPA2 on 2.4 GHz, the Zero W has no 5 GHz radio. Never commit the file.

3. Build:

   ```bash
   kas build kas.yml
   ```

   The first build takes several hours. The image ends up in
   `build/tmp/deploy/images/raspberrypi0-wifi-synth/`.

`kas` creates `build/`, generates `conf/local.conf` and `conf/bblayers.conf`
from `kas.yml` and loads the build environment (the equivalent of
`source oe-init-build-env`). Do not edit `build/conf/local.conf` by hand, kas
overwrites it. Put persistent settings in `kas.yml` (`local_conf_header`) or
in the layer.

For manual BitBake commands (`bitbake -e`, `bitbake-layers`, ...) open a
shell with the environment loaded:

```bash
kas shell kas.yml
```

### Pinning layer versions

`kas.yml` follows the `scarthgap` branch of each upstream layer (poky,
meta-openembedded, meta-raspberrypi). A branch moves whenever upstream pushes
to it, so two builds made at different times could use different code.

`kas.lock.yml` prevents that. It stores the exact commit of each upstream
layer, and kas picks it up automatically when it sits next to `kas.yml`, so
every `kas build kas.yml` checks out the same versions. The commits in the
lock file are the ones the image was built and tested with on the board.

#### Updating to newer layer versions

Do this on purpose, not by accident:

```bash
kas lock --update kas.yml    # pull newer commits from the branches, rewrite kas.lock.yml
git diff kas.lock.yml        # see which layers moved
kas build kas.yml            # rebuild
```

Flash the new image and test it on the board (boot, WiFi, SSH). Only if it
works, commit the new lock file:

```bash
git add kas.lock.yml
git commit -m "Update layer versions"
```

If the new build or the board misbehaves, go back to the last working
versions with `git checkout kas.lock.yml`.

After changing the repositories or branches in `kas.yml`, run
`kas lock kas.yml` again so the lock file matches. The lock file covers only
the upstream layers, `meta-local` is versioned by this repo's own commits.

## Flashing and first boot

```bash
lsblk    # find the SD card device, do not pick your system disk
sudo bmaptool copy \
    build/tmp/deploy/images/raspberrypi0-wifi-synth/rpi0-synth-image-raspberrypi0-wifi-synth.rootfs.wic.bz2 \
    /dev/sdX
```

Insert the card, connect power and open the serial console:
`screen /dev/ttyUSB0 115200` (login `root`, no password). Once the board has
joined WiFi, find its IP address on the router or with `ip a show wlan0`
and log in with `ssh root@<IP>`.

Passwordless `root` is a development setting (`debug-tweaks` in `kas.yml`),
do not leave it on a device exposed to a network.

### Changing WiFi networks on a running board

`update_config=1` is enabled, so networks can be added over SSH or UART
without rebuilding:

```bash
wpa_passphrase "SSID" "PASSWORD" | grep -v '#psk' >> /etc/wpa_supplicant.conf
wpa_cli -i wlan0 reconfigure
```

Changes survive reboots but are lost when a new image is flashed. Networks
that should always be present belong in `wpa_supplicant.conf-sane` in the
layer.

### Testing the speakers

The sound card can be opened by one program at a time. While the synth
service runs (see "Synthesizer" below), fluidsynth holds the card, and
`speaker-test` or `aplay` fail with `Device or resource busy`. Stop the
service for the test and start it again afterwards:

```bash
/etc/init.d/synth stop
# ... test ...
/etc/init.d/synth start
```

Check that the sound card is there (`IQaudIODAC`; there is no HDMI sound card, VC4 graphics is disabled in the machine config):

```bash
aplay -l
```

The DAC starts at full volume (`Digital` = 0 dB), which can be very loud.
Lower it before playing anything, and turn the volume of your speakers or
headphones down. A value starting with `-` needs `--` in front of it,
otherwise `amixer` takes it for an option:

```bash
amixer -c IQaudIODAC scontrols                     # list the mixer controls
amixer -c IQaudIODAC sset Digital -- -30dB         # start quiet, raise if needed
amixer -c IQaudIODAC sget Digital                  # show the current level
```

Play a short sine wave (one loop, left channel first, then right):

```bash
speaker-test -D plughw:IQaudIODAC -c 2 -t sine -f 440 -l 1
```

Use the card name (`IQaudIODAC`) and not its number, the numbers of the ALSA
cards change with the order in which USB devices are detected. A mixer level
set by hand is not saved: the synth service sets `Digital` again (`VOLUME`)
every time it starts, and without the service it goes back to the default.

## Synthesizer

The image starts fluidsynth at boot and connects the USB MIDI controllers to
it, so the board plays without a console or a monitor. The recipe is
`meta-local/recipes-multimedia/synth-autostart/` (init script `synth`, helper
`synth-connect`). The soundfont comes from `timgm6mb-soundfont` (TimGM6mb,
GPL-2.0), installed in `/usr/share/sounds/sf2/`.

### Do not unplug USB devices while the board is running!

Unplugging a USB device (we tried the KeyStep) from the hub while the board is
running can freeze the whole system. **Shut the board down first
(`poweroff`), then plug or unplug, then power it on again.** Plugging a
device in is not a problem: it is picked up and connected to fluidsynth by
`synth-connect` within a few seconds (tested with the KeyStep, and devices
present at boot are connected too).

What we saw on the Zero W with the `dwc2` USB driver (the one in
`raspberrypi0-wifi-synth.conf`, as the Waveshare documentation recommends):

- After the KeyStep was unplugged, the kernel printed hundreds to thousands
  of `usb 1-1.2: urb status -32` errors and only then `USB disconnect`. In
  our first try that took about 11 seconds, in the second the whole user space
  stopped for 36 seconds (a logger that writes the time every 0.2 s had a
  gap of 36 s that ended exactly at the `USB disconnect` message). During
  that time SSH stopped answering, the speakers popped and packets were lost.
  After the disconnect was processed the board recovered on its own.
- The USB controller is very busy even when nothing happens: about 10 000
  interrupts per second (`20980000.usb, dwc2_hsotg` in `/proc/interrupts`).
  The controllers (full speed) sit behind a high speed hub, and that needs
  split transactions, which this driver handles poorly. This is our
  interpretation, we did not prove it.
- We also tried the other driver in the kernel (`dwc_otg`, by commenting out
  `dtoverlay=dwc2,dr_mode=host`). It enumerates the hub and controllers fine
  and uses fewer interrupts (about 4 850 per second at rest, its FIQ
  interrupts are not counted), but it was **worse** when a device was
  unplugged: the board stopped answering completely (no `ping`, no SSH) and
  did not come back for minutes, only cutting the power helped. Do not switch
  to it.
- The exact cause is not identified. A board with a real USB host controller
  (for example a Raspberry Pi 4B) may not have this problem, but we did not
  test that.

### How it works

- `/etc/init.d/synth` (linked as `S90synth` in `rc5.d` by `update-rc.d`)
  waits for the `IQaudIODAC` card, sets the mixer level and starts fluidsynth
  (ALSA audio output, MIDI input from the ALSA sequencer) and `synth-connect`.
  Control it with `/etc/init.d/synth start|stop|restart`.
- `synth-connect` is a small shell loop. Every `INTERVAL` seconds (5 by
  default, a variable at the top of the script) it reads `aconnect -l` once,
  finds the hardware MIDI clients (a client with `card=`) and the fluidsynth
  client, and connects only the controllers that are not connected yet. The
  work is done by one `awk` process, and `aconnect` is called only when
  something is missing. Controllers plugged in after boot, and connections
  lost when fluidsynth restarts, are restored within about `INTERVAL`
  seconds. The clients are matched by name, so the ALSA client numbers, which
  change between boots, do not matter.
- The output of fluidsynth goes to `/var/log/synth.log` (in RAM, gone after a
  reboot).

### Tuning parameters

The parameters are variables at the top of
`meta-local/recipes-multimedia/synth-autostart/files/synth`. To change them
for good, edit that file and rebuild the image. To try a value on a running
board, edit `/etc/init.d/synth` and run `/etc/init.d/synth restart`.

| Variable | Default | Meaning |
|---|---|---|
| `GAIN` | `0.5` | Master gain of fluidsynth (`-g`). The fluidsynth default of `0.2` is too quiet with this DAC. Too high a value distorts with many voices. |
| `VOLUME` | `207` | Raw value of the `Digital` mixer control: `207` = 0 dB, one step = 0.5 dB (`201` = -3 dB). |
| `PERIOD_SIZE` | `256` | Frames in one ALSA period. |
| `PERIODS` | `4` | Number of periods in the buffer. `256 x 4` = 1024 frames, about 23 ms at 44.1 kHz. |
| `POLYPHONY` | `64` | Limit of simultaneous voices. |
| `REVERB` | room size `0.95`, damp `0.2`, width `1`, level `1` | Settings of the reverb unit (`-o synth.reverb.*`). A large room, so the reverb knob (CC 91) has something to work with. |
| `CHORUS` | nr `4`, level `1.5`, speed `0.5` Hz, depth `20` ms | Settings of the chorus unit (`-o synth.chorus.*`). With `nr 3` the chorus seemed to lean to the left channel (by ear), so an even number is used. |

The effect units are on, but the sound starts dry: `pc-bridge` sets the
reverb and chorus send of every channel to the "knob at 0" position when
it connects to fluidsynth (see "`pc-bridge`" below).

Notes from testing on the board:

- The fluidsynth default buffer (`64 x 16`) gave underruns (`XRUN`) and
  dropouts when the KeyStep arpeggiator played chords, even though the CPU was
  about 55% idle. With `256 x 4` there was no `XRUN` in a 30 second test and
  the latency was not noticeable when playing.
- Polyphony counts voices, not notes. One note uses several voices (about 5
  for one strings program in our test). The fluidsynth default of `256` is
  too much for the single ARM1176 core, a KeyStep arpeggiator at 120 BPM with
  8-note chords fills it up.
- The CPU headroom is small, we saw about 15% idle at around 40 voices while
  the arpeggiator was running. Do not raise `POLYPHONY` without testing.
- `synth-connect` has its own setting, `INTERVAL` (seconds between checks,
  at the top of `files/synth-connect`). Polling is not free on this board. We
  measured the CPU time of the loop over 60 seconds with nothing playing: the
  first version (a check every 2 s, two `aconnect -l`, two `sed` and one
  `aconnect` per controller on every round) used about 6.8% of the single
  core, more than half of what fluidsynth uses when idle (about 14–16%). The
  current version (one `aconnect -l` and one `awk` every 5 s) uses about 1.1%.
  Measured once for each version, on the running board. A controller that is
  disconnected is connected again within about 5 seconds (2.3 s and 5.2 s in
  our test, when the connections were removed with `aconnect -d`).

### Checking that it works

```bash
pgrep -l fluidsynth                                 # the process is running
aconnect -l                                         # KeyStep and MPK show "Connecting To: <fluidsynth client>:0"
aseqdump -p <client>:0                              # watch the MIDI events of a controller
grep -E "period_size|buffer_size|rate" /proc/asound/IQaudIODAC/pcm0p/sub0/hw_params
head -n 1 /proc/asound/IQaudIODAC/pcm0p/sub0/status # RUNNING is fine, XRUN means an underrun
amixer -c IQaudIODAC sget Digital                   # mixer level
tail /var/log/synth.log
```

`aseqdump` and the fluidsynth shell count MIDI channels from 0: the KeyStep
(channel 15) shows up as `14`, the MPK (channel 16) as `15` and the drum
pads (channel 10) as `9`.

### The fluidsynth shell over TCP

The service starts fluidsynth with `-s`, so it listens on port 9800 for
commands. There is no authentication and the port is open on all network
interfaces, so anyone on the same network can send commands to the synth. This
is a debugging convenience: remove `-s` from `files/synth` if you do not want
it. The shell prints no prompt, and BusyBox `nc` has no `-w` option.

```bash
echo "voice_count" | nc 127.0.0.1 9800      # one command, run it on the board
nc 127.0.0.1 9800                           # interactive, type a command and press Enter, Ctrl+C to leave
```

Some useful commands (`help all` lists everything). Changes made in the shell
last until fluidsynth restarts, permanent ones belong in `files/synth`:

| Command | What it does |
|---|---|
| `help`, `help all` | List the command topics or all commands. |
| `voice_count` | Number of active voices (compare with `POLYPHONY`). |
| `gain 0.5` | Set the master gain. `get synth.gain` shows the current value. |
| `get <name>`, `set <name> <value>`, `info <name>` | Read, change or describe a setting, e.g. `get synth.polyphony`. `info` says whether a setting can be changed on the fly (`Real-time: yes`). `settings` lists them all. |
| `reverb off`, `reverb on`, `chorus off`, `chorus on` | Turn the effect units off or on, they cost CPU. With a unit off, its knob does nothing. |
| `rev_setroomsize`, `rev_setdamp`, `rev_setwidth`, `rev_setlevel`, `cho_set_nr`, `cho_set_level`, `cho_set_speed`, `cho_set_depth` | Try other effect settings live (`help reverb`, `help chorus`), then copy the good ones to `REVERB`/`CHORUS`. |
| `noteon <chan> <key> <vel>`, `noteoff <chan> <key>` | Play or release a note without a keyboard, e.g. `noteon 0 60 100`. |
| `cc <chan> <ctrl> <value>`, `prog <chan> <num>` | Send a control change or a program change. |
| `channels`, `fonts` | Show the instrument on each channel and the loaded soundfonts. |
| `reset` | Release all notes and reset the controllers (it does not help while a controller, e.g. an arpeggiator, keeps sending notes). |

### MIDI controllers

The KeyStep 37 sends on channel 15 and the MPK Mini MK3 on channel 16, its
drum pads on channel 10 (the General MIDI drum channel, the only drum
channel in fluidsynth by default). Channels 1 and 10 are the busiest on
grooveboxes, so the keyboards stay away from them. Control changes apply per
channel, so a KeyStep knob changes only the sound of channel 15.

Knob map used on both controllers (set in Arturia MIDI Control Center and
the MPK Editor; all knobs are endless encoders in **absolute** mode, the
controller keeps the 0–127 value):

| CC | Function | Handled by |
|---|---|---|
| 3 | Patch select | `pc-bridge` (Program Change) |
| 7, 10, 11 | Volume, pan, expression | fluidsynth |
| 1 | Modulation (vibrato depth) | fluidsynth |
| 5, 65 | Portamento time, portamento on/off (≥ 64 = on) | fluidsynth |
| 73, 75, 79, 72 | Attack, decay, sustain, release | `pc-bridge` (NRPN) |
| 74, 71 | Filter cutoff, resonance | `pc-bridge` (NRPN) |
| 76 | Vibrato rate | `pc-bridge` (NRPN) |
| 91, 93 | Reverb send, chorus send | `pc-bridge` (NRPN); fluidsynth also handles them, but only up to 20 % |

Notes from testing:

- Fluidsynth handles CC 1, 7, 10, 11, 91 and 93 through the default
  modulators of the SoundFont spec, and CC 5, 64, 65 itself. CC 91 and 93
  reach only 20 % of the effect send that way, too little to hear, so
  `pc-bridge` handles them too.
- Portamento time is about `CC 5 x 128 ms` in fluidsynth, so almost all of
  the useful range is at the bottom of the knob (10 is already 1.3 s). We
  set the knob range to 0–12 in the controller editor. The glide is heard
  when playing legato (the next key pressed before the previous one is
  released).
- The MPK sends a Program Change with the number typed in the MPK Editor,
  and fluidsynth counts programs from 0: type the General MIDI number
  minus 1 (e.g. 81 for Lead 2, sawtooth). `inst 1` in the fluidsynth shell
  lists the programs as fluidsynth numbers them.
- Pads: on the MPK the pad channel is set per program, not per pad bank. A
  program with drum pads (channel 10) cannot also use its pads for Program
  Change or CC on the keyboard channel, so drums have their own program.
- Channel aftertouch works (fluidsynth maps it to vibrato depth), poly
  aftertouch does nothing with this soundfont.

### Using another controller

`synth-connect` does not know the names of the KeyStep and the MPK. It
connects every hardware MIDI client (a client with `card=` in `aconnect -l`)
to fluidsynth, so another USB MIDI controller should be connected within
about `INTERVAL` seconds after it is plugged in. The kernel modules for class
compliant USB MIDI (`snd-usb-audio`, `snd-usbmidi-lib`) are in the image. Only
the KeyStep and the MPK were tested, the rest below follows from how the
scripts work. **Shut the board down before plugging or unplugging a device**
(see the warning above).

To check a new controller:

```bash
aconnect -l              # the new client has "card=" and "Connecting To: <fluidsynth client>:0"
aseqdump -p <client>:0   # play a few notes and watch the events
```

Things to keep in mind:

- Only port 0 of each controller is connected. A controller with several MIDI
  ports gets only the first one. If port 0 does not send events, the
  connection fails, and the error is hidden, so nothing is printed.
- Devices that need a vendor driver (not class compliant) do not show up as
  an ALSA client.
- Fluidsynth accepts all 16 MIDI channels, and by default every channel has
  the same instrument (`Piano 1`, see `channels` in the shell). The channel the
  controller sends on decides the sound. Channel 10 (9 when counted from 0) is
  drums in General MIDI.
- The knobs work only if they send the control changes from the table in
  "MIDI controllers" above. Two controllers on the same channel
  share the controller state (for example one can change the volume of the
  other).
- The hub is powered from the same supply as the board. The KeyStep declares
  100 mA and the MPK 500 mA. We did not test a controller that draws more.

### Troubleshooting

If the sound crackles or stops while an arpeggiator or a long chord is
playing, look at the state of the card (`status`, `XRUN`) and at
`voice_count`. Stop the arpeggiator, lower `POLYPHONY`, or turn the reverb
and chorus off.

If fluidsynth, `synth-connect` or `pc-bridge` dies (see "Self-healing"
below), it comes back on its own within a few seconds. If sound stays
silent longer than that, log in and check `pgrep -l fluidsynth`,
`aconnect -l` and the log files.

### `pc-bridge`: CC 3 to Program Change, synth knobs to NRPN

#### CC 3 to Program Change

Some controllers (the Arturia KeyStep 37 mk1 among them) have knobs that
only send control changes, with no way to send a MIDI Program Change, so
they cannot switch the fluidsynth patch on their own. `pc-bridge`
(`meta-local/recipes-multimedia/pc-bridge/`) is a small ALSA sequencer
client that turns **CC 3** (an "undefined" controller number in the MIDI
spec, rarely used by factory presets) on any channel into a real Program
Change on that channel, sent to fluidsynth. A real Program Change sent by a
controller (for example the MPK's PROG CHANGE pad mode) is not touched, it
reaches fluidsynth directly through the existing connections.

- It finds fluidsynth by client name (containing `FLUID`) and reconnects
  when fluidsynth restarts. It subscribes to every readable MIDI port
  except `Midi Through` and fluidsynth itself, and follows hotplug through
  `System:Announce`, so a controller does not need a fixed client number.
  This runs independently of `synth-connect`, and its output port is
  `NO_EXPORT` so `synth-connect` leaves it alone.
- To use it: set a knob to CC 3, absolute mode, in the controller's own
  editor (Arturia MIDI Control Center, MPK Editor, ...). On our MPK Mini
  MK3 the third knob already sent CC 3 in the factory preset.
- Log: `/var/log/pc-bridge.log` (lines `in <- ...`, `out -> FLUID Synth
  ...`, `ch N -> program M`, `reverb/chorus send -> knob 0 on all
  channels`). Knob moves translated to NRPN are not logged, a knob sweep
  would fill the log, which lives in RAM. See `CCtoPC.md` in the repo root
  for the full design rationale of CC 3.
- CC 3 and the NRPN knobs are handled on every channel, so a controller
  whose factory mapping happens to use one of these CC numbers for
  something else would have that knob silently change the sound instead.
  Check this before adding a new controller (or a groovebox that sends CC).

#### Synth knobs to NRPN

Fluidsynth has no default mapping for the envelope, the filter or the
vibrato rate, and the soundfont has no modulators for them. Instead of
editing the soundfont, `pc-bridge` turns the knob CCs into SoundFont 2.01
NRPN messages, which fluidsynth supports (`fluid_synth.c`, "SoundFont 2.01
NRPN Message"): CC 99 = 120, CC 98 = generator number, CC 38 and CC 6 =
14-bit value. Fluidsynth adds `(value - 8192) * nrpn_scale` to the
generator of the instrument, on that channel, for every instrument. So a
knob is an offset from the instrument's own setting, and the setting stays
when the patch changes.

The mapping is the `nrpn_map` table in `pc-bridge.c`. A knob value
0–127 maps linearly to an offset from `lo` to `hi`, in the generator's own
units; `scale` is the generator's `nrpn_scale` from `fluid_gen.c` of
fluidsynth 2.3.4. To tune a knob, change `lo`/`hi` and rebuild.

| CC | Generator | Units | Knob |
|---|---|---|---|
| 73 attack | 34 | timecents (1200 = time x 2) | 0 = as the instrument, 127 = up to about 4 s |
| 75 decay | 36 | timecents | 64 = as the instrument |
| 79 sustain | 37 | centibels of attenuation | 127 = as the instrument, 0 = -40 dB |
| 72 release | 38 | timecents | 64 = as the instrument |
| 74 cutoff | 8 | cents | 127 = as the instrument (usually open), 0 = about 80 Hz |
| 71 resonance | 9 | centibels | 0 = as the instrument, 127 = +24 dB |
| 76 vibrato rate | 24 | cents | 64 = as the instrument, x/÷ 4 at the ends |
| 91 reverb send | 16 | 0.1 % | 0 = dry (for instruments with up to 20 % of their own), 127 = 100 % |
| 93 chorus send | 15 | 0.1 % | as reverb |

Notes from testing:

- The first version had the whole range of sustain at -96 dB..0 dB and of
  reverb/chorus at -100 %..+100 %: half of the knob did nothing audible.
  The current ranges came from that.
- Sustain is a level, not a time: the note falls to it after the decay,
  and many sustained instruments have a very long decay. Turn decay down
  to hear the sustain knob.
- Attack, decay and sustain changes apply to notes played after the
  change, release to notes released after it.
- The knobs and the synth can disagree after fluidsynth restarts (offsets
  back to 0, the controller still remembers its values): the first knob
  move sets the synth to the knob's value.

### Self-healing

fluidsynth, `synth-connect` and `pc-bridge` all run under a small
supervisor, `respawn` (`.../synth-autostart/files/respawn`, installed as
`/usr/bin/respawn`; a duplicate ships with `pc-bridge` as
`/usr/bin/pcbridge-respawn`, to avoid two packages installing the same
path). It restarts the program whenever it exits, up to 5 times within 60
seconds, after which it gives up and logs that. `SIGTERM` (what
`/etc/init.d/synth stop` and `/etc/init.d/pc-bridge stop` send) kills the
supervised process cleanly and stops the loop, so `stop`/`restart` work as
before.

This matters most for fluidsynth, whose failure is loud (silence), but also
for `synth-connect` and `pc-bridge`, whose failure is quiet: sound keeps
playing, only auto-connecting new controllers, or CC 3 to Program Change
and the synth knobs, silently stops working.

## License

MIT, see `LICENSE`. The license covers the files in this repository only;
the built image contains software under its own licenses.
