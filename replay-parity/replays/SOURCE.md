# Replays: source

These are Slippi recordings from the **HoG** (House of Games) tournament series, from hog.gg's public
Slippi archive (the "HoG: Phoenix Series" and "Giga HoG Series" folders). They were recorded on
consoles running Nintendont with Slippi (Slippi format 3.17.0, UCF 0.84 toggles on), all on
Battlefield. They are included unmodified, under short names, as the test set for
`replay-parity/tools/run_all.sh`; the table below gives each file's name in the archive.

Two files are not tournament recordings but controls made from them:

- `b11-tampered.slp` is `b11.slp` with port 3's main-stick X altered on frames 500 to 560. A faithful
  playback must leave every frame before 500 unchanged and diverge from `b11.slp` at frame 500.
- `b09-tamper200.slp` is `b09.slp` with port 1's raw main-stick X set to 127 on frames 200 to 230
  (port 1 is standing still there). A faithful playback diverges from `b09.slp` at frame 200.

Ports are numbered from 1 here, as on the console; the tools print them from 0.

| file | archive folder and file name |
|---|---|
| `b01.slp` | HoG: Phoenix Series/HoG_Phoenix_10_12/Setup_2/1150_(Red Falco)_vs_(Default Fox)_(Battlefield).slp |
| `b02.slp` | HoG: Phoenix Series/HoG_Phoenix_10_17/Setup_3/1212_(Blue Fox)_vs_(Blue Falco)_(Battlefield).slp |
| `b03.slp` | HoG: Phoenix Series/HoG_Phoenix_10_18/Setup_1/0257_HAWK(Default Falco)_vs_XYER(Red Falco)_(Battlefield).slp |
| `b04.slp` | HoG: Phoenix Series/HoG_Phoenix_9_12/Stream/0227_STB(Default Falco)_vs_LOVE(Green Falco)_(Battlefield).slp |
| `b05.slp` | HoG: Phoenix Series/HoG_Phoenix_10_1/Setup_2/0148_(Red Marth)_vs_(Red Falco)_(Battlefield).slp |
| `b06.slp` | Giga HoG Series/Giga_HoG_9_1/Setup_1/0043_!(White Marth)_vs_(Default Falco)_(Battlefield).slp |
| `b07.slp` | Giga HoG Series/Giga_HoG_9_1/Setup_14/1823_(Black Marth)_vs_(Red Fox)_(Battlefield).slp |
| `b08.slp` | Giga HoG Series/Giga_HoG_10_1/Setup_12/1253_(Default Marth)_vs_TVET(Default Fox)_(Battlefield).slp |
| `b09.slp` | HoG: Phoenix Series/HoG_Phoenix_10_12/Setup_1/0355_(Default Fox)_vs_(Green Fox)_(Battlefield).slp |
| `b10.slp` | Giga HoG Series/Giga_HoG_10_1/Setup_12/1556_JEST(Red Marth)_vs_OPUS(Green Marth)_(Battlefield).slp |
| `b11.slp` | Giga HoG Series/Giga_HoG_10_1/Setup_1/1852_TVET(White Sheik)_vs_(Red Fox)_(Battlefield).slp |
| `b12.slp` | Giga HoG Series/Giga_HoG_10_1/Setup_5/1152_(White Peach)_vs_JEST(Green Marth)_(Battlefield).slp |
| `b13.slp` | Giga HoG Series/Giga_HoG_10_1/Setup_5/1241_BUD(Default Fox)_vs_ED(Crown Puff)_(Battlefield).slp |
| `b14.slp` | Giga HoG Series/Giga_HoG_10_1/Setup_4/1451_(Default Falcon)_vs_(Red Fox)_(Battlefield).slp |
| `b15.slp` | HoG: Phoenix Series/HoG_Phoenix_10_13/Setup_1/0551_(Default Falco)_vs_(Green Samus)_(Battlefield).slp |
| `b16.slp` | HoG: Phoenix Series/HoG_Phoenix_10_13/Setup_3/1212_(Green Marth)_vs_LOVE(Party Hat Pikachu)_(Battlefield).slp |
| `b17.slp` | Giga HoG Series/Giga_HoG_10_1/Setup_10/1629_(Blue Fox)_vs_$.01(Blue Luigi)_(Battlefield).slp |
| `b18.slp` | HoG: Phoenix Series/HoG_Phoenix_10_1/Setup_4/1652_CB(Green Marth)_vs_MR.2(Black Doc)_(Battlefield).slp |
| `b19.slp` | Giga HoG Series/Giga_HoG_10_1/Setup_9/0028_(Black Marth)_vs_(Blue Ganon)_(Battlefield).slp |
| `b20.slp` | HoG: Phoenix Series/HoG_Phoenix_10_3/Setup_3/1414_LOVE(Blue Falco)_vs_(Blue Yoshi)_(Battlefield).slp |
| `cal.slp` | HoG: Phoenix Series/HoG_Phoenix_10_17/Setup_1/0405_(Green Falco)_vs_(Default Marth)_(Battlefield).slp |
| `m1.slp` | Giga HoG Series/Giga_HoG_10_1/Setup_1/0054_(Default Falco)_vs_(Default Fox)_(Battlefield).slp |
| `m4.slp` | Giga HoG Series/Giga_HoG_10_1/Setup_1/1844_TVET(Default Fox)_vs_(Red Fox)_(Battlefield).slp |
| `m9.slp` | Giga HoG Series/Giga_HoG_10_1/Setup_10/1759_(Red Marth)_vs_DAYZ(Black Marth)_(Battlefield).slp |
| `s01.slp` | HoG: Phoenix Series/HoG_Phoenix_9_25/Stream/0307_ASHY(Green Samus)_vs_CLUB(White Sheik)_(Battlefield).slp |
| `b11-tampered.slp` | made from `b11.slp` (see above) |
| `b09-tamper200.slp` | made from `b09.slp` (see above) |

SHA-256 of every file: `SHA256SUMS` (check with `sha256sum -c SHA256SUMS` in this folder).
