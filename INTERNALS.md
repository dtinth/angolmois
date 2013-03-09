Angolmois Internals
===================

This document is intended for serious BMS writers and other software
developers.


History and Significant Changes
-------------------------------

Angolmois 1.0 final (204 lines of code) was finished on July 2007. It ended up
as an unmaintainable mess of highly obfuscated code, and was only useful as
an example of very obfuscated C code. (For example, can you figure out what
`i<18?(q&r)||(m?!(l&4)||r<4:r-((l&12)-8?l&1?1:2:8)):i-19?q:q&r` means?!)

Angolmois 2.0 alpha 1 (1797 lines of code) was finished on January 2012. This
was a general cleanup from Angolmois 1.0, and introduced a sane command line
interface and more stable game engine. There are so many changes that I cannot
enumerate them here.

Angolmois 2.0 alpha 2 (1994 lines of code) was finished on February 2013. This
is intended as the only major feature release until 2.0 final. Important
changes include:

* Joystick support.
* Multiple movie playback support.
* PMS support.
* Bomb (channels `Dx`/`Ex`) support.
* Alpha channel support for BGA.
* More stable key sound playing.
* More recent RANDOM BMS extensions (e.g. `#SETRANDOM`) are implemented.
* New path resolution algorithm. Nested directories can be used in BMS files.
* Fixed various long note rendering problems.
* _Incompatible to 2.0 alpha 1:_ Updated environment variable format.
* _Incompatible to 2.0 alpha 1:_ C99 compiler is required now.

Angolmois 2.0 beta 1 is being developed as of February 2013. This is mainly
a feature-complete, stable release candidate for 2.0 final. It is expected
that beta 1 will be released as final without much change. Important changes
include:

* Dropped support of consecutive long notes. (Issue #15)


BMS Support Status
------------------

BMS has lots of extensions (quite comprehensive document can be found
[here][bmscmds]) and Angolmois supports lots of them. As most extensions are
not specified or specified vaguely, this section tries to cover most concerns
useful for BMS writers and other software developers.

[bmscmds]: http://hitkey.nekokan.dyndns.info/cmds.htm

This section is not intended to be complete. In doubt, always consult
the source code. The source code of Angolmois is in public after all.

### Glossary

This document uses the following terms:

* A **command** refers to one significant line in the BMS file, starting with
  optional whitespace followed by `#`. A **data command** is a subset of
  command which has a form of `#xxxyy:...`.
* A **key** refers to one input element of the input device, normally
  a button, a joystick axis or a turntable.
* An **alphanumeric key** refers to a two-letter, base-36 data element present
  in data commands. Alphanumeric keys are used as an identifier or direct
  value.
* A **measure** is a unit of time, which length is associated to the actual
  time by BPM. The song is composed of multiple measures in BMS and each
  measure is given a **measure number** starting at 0.
* A **channel** refers to the type of data commands, conveniently represented
  as one alphanumeric key.
* A **lane** refers to a game element where objects are vertically positioned
  and mapped to the physical key and screen area. Each lane has an associated
  group of four channels.
* An **object** refers to a game element positioned in the certain lane which
  directly or indirectly affects the game play. There are five kinds of
  objects supported by Angolmois: visible object (note), invisible object,
  long note start and end, and bomb. Note that the horizontal measure bar is
  not an object.
* A **key model** refers to how the channels `10`-`2Z`, `30`-`4Z`, `50`-`6Z`
  and `D0`-`EZ` (which forms 72 groups in total) are mapped to lanes, and also
  how those lanes are mapped to the physical input device and screen.

### Basic Rules

* Lines can end with `\r` (Mac OS classic), `\n` (Unix) or `\r\n` (Windows).
* Too long lines (currently, about 4000 bytes) may be cut.
* Everything is parsed case-insensitively if not specified.
* Every 1296 two-letter alphanumeric key is supported. That is, you can always
  use `00` to `ZZ`. They are case-insensitive as well.
* Both one or more tabs and spaces count as whitespace.
* Preceding whitespace is ignored.
* Invalid alphanumeric key or number results in the command ignored entirely,
  but if the prefix of the line parses to the correct command then
  the remainder is ignored.[^1] As an exception, invalid alphanumeric keys in
  the data commands does not affect other valid alphanumeric keys.
* The duplicate command overrides the prior command unless the command itself
  allows for duplicates. (It currently does not hold for the data commands due
  to the implementation strategy.)
* In the absence of duplicates and possible ambiguities, all commands are
  processed independently of their relative positions.

[^1]: Thus `#PLAYLEVEL 12.3` is valid even while `#PLAYLEVEL` requires
      an integer; it is equivalent to `#PLAYLEVEL 12`.

### Metadata

In the following commands, `<string>` is parsed case-sensitively.

* `#TITLE <string>`
* `#GENRE <string>`
* `#ARTIST <string>`
* `#PLAYLEVEL <integer>` (defaults to `#PLAYLEVEL 0`)

They are displayed before the actual game play unless `--no-info` option is
given. Angolmois does not try to detect or assume the character encoding;
bytes outside the ASCII region will displayed broken.

### Game Settings

* `#PLAYER 1` (default)
* `#PLAYER 2`
* `#PLAYER 3`

Corresponds to the single play (SP), couple play and double play (DP) mode
respectively. Angolmois does not support battle play (`#PLAYER 4`) and has
very limited support for couple play (`#PLAYER 2`).

Angolmois always puts the objects on the left hand side and BGA on the right
hand side. `#PLAYER 2` is implemented as split lanes on both sides, and
otherwise same as `#PLAYER 3` (they share the same gauge and grading).

* `#RANK <integer>` (defaults to `#RANK 2`)

Angolmois assigns a grading area to every visible, invisible object and long
note start/end, which size is no larger than the threshold for BAD grade.
A grading area is specific to single lane (currently). No two grading areas
overlap. Therefore the actual grading area can be a lot shorter when objects
are very dense, but the grading is done strictly by the time difference basis.

The concept of grading area is quite confusing, so let me explain with
an example. Assume that there are two grades (GOOD or BAD), which threshold is
respectively within ±50ms and ±100ms. So the size of grading area is at most
±100ms, i.e. a threshold of the most forgiving grade. Now if there are two
visible object separated by 50ms, the grading area of the first object starts
at 100ms before it and ends at 25ms after it (contracted by 75ms); one of
the second object starts at 25ms (again, contracted by 75ms) before it and
ends at 100ms after it. The contracted grading areas entirely contain
thresholds for BAD, so if the player presses a key between two objects,
the closest object is graded and its grade will always be GOOD since it is
always within ±50ms.

The current system will give BAD, GOOD, GREAT and COOL grade if the object is
approximately[^2] within `±{144, 84, 48, 14.4} / (1.5 - #RANK * 0.25)`
milliseconds respectively. Unpressing the key at the end of LN has the same
grading area as BAD. The following table summarizes the hard values:

	#RANK:  5       4       3       2       1       0       -1      ...

	BAD:    576ms   288ms   192ms   144ms   115ms   96ms    82ms    ...
	GOOD:   336ms   168ms   112ms   84ms    67ms    56ms    48ms    ...
	GREAT:  192ms   96ms    64ms    48ms    38ms    32ms    27ms    ...
	COOL:   58ms    29ms    19ms    14ms    12ms    10ms    8ms     ...

This results in considerably narrower COOL area and wider GREAT area, and also
support for every integer `#RANK` up to 5. `#RANK 6` or higher is same as
`#RANK 5`. Angolmois `#RANK` system is certainly ill-designed however, so it
may change without a notice.

[^2]: Due to the current implementation strategy, the actual grading area may
      vary if the object is very close to the point where the BPM or size of
      measure is changed. This behavior may change.

### Resources

In the following commands, `<path>` refers to the file path relative to the
BMS/BME/BML/PMS file (or `#PATH_WAV` if any). The file path may contain
directory separators `/` or `\` which are automatically normalized. The file
name matching is done case-insensitively even in non-Windows platforms.

If the original path was given a file extension, Angolmois also tries to use
alternative file extensions for given kind of resources. This may result in
multiple candidates for a single path; in this case Angolmois choose one
arbitrarily. If the candidate does not exist, Angolmois will issue an warning.

* `#WAVxx <path>`
* Channel `01`

Angolmois supports every audio file format that SDL\_mixer supports, including
WAV, OGG (Vorbis), MP3 (MPEG-1/2 Audio Layer 3). Angolmois also tries to use
common audio file extensions: `.wav`, `.ogg` and `.mp3`. `#WAV00` is used for
channel `Dx` and `Ex` as a bomb sound.

Due to the limitation of SDL's WAV parser, WAVs with sampling rates other than
11025, 22050 or 44100 Hz may sound incorrectly.

* `#BMPxx <path>`
* Channels `04` (bottom layer), `06` (POOR BGA), `07` (middle layer) and `0A`
  (top layer)

Angolmois supports every image file format that SDL\_image supports, including
BMP (including RLE-compressed ones), PNG, JPEG, GIF and so on. Angolmois also
tries to use common image file extensions: `.bmp`, `.png`, `.jpg`/`.jpeg` and
`.gif`. `#BMP00` is used as an initial POOR BGA if given. If the image has an
alpha channel it will retain its transparency; otherwise the color black
(`#000000`) will be used as a transparent color.

Angolmois also supports an MPEG-1 video file playback if the file name ends
with `.mpg` (case insensitive). There may be multiple video files loaded and
played. It plays while it is "in display", that is, not replaced by other
image or video via data command (but may have been obscured by other layers or
hidden due to POOR BGAs). If one video is shared among multiple layers,
the playback status is shared among them and may restart unexpectedly. A video
does not support a transparent color.

If the image or video is larger than 256 by 256 pixels, only the upper left
region will be used. Conversely, smaller one will be padded to the transparent
color (or black).

* `#BGAxx yy <integer> <integer> <integer> <integer> <integer> <integer>`

The image composition using `#BGAxx` is done after completely loading images,
and executed in the order of appearance. The target slot (`xx`) should be
empty, i.e. not used by `#BMPxx` or other `#BGAxx`, and should not be a video.

The top/left coordinates are clipped to 0 if negative; the width and height
calculated from both coordinates are clipped to 256 if larger than 256. Other
clippings do not occur.

* `#STAGEFILE <path>`

The image is displayed before the actual game play, along with other metadata.
Otherwise its behavior is same as `#BMPxx` without a video support. The image
is automatically rescaled to 800 by 600 pixels.

* `#PATH_WAV <path>` (defaults to the directory containing the BMS file)

This command overrides the base directory used for resolving file paths. It
may be located anywhere in the BMS file; the resolution is done after parsing.

### Playback Settings

* `#BPM <number>` (defaults to `#BPM 130`)
* `#BPMxx <number>`
* Channels `03` and `08`

Variable BPM is fully supported, with a caveat that very large or very small
BPM may not work as intended (related to floating point calculations). You can
mix a hexadecimal BPM (channel `03`) and extended BPM format (channel `08`);
extended BPM is preferred if they are in the same position. Angolmois ignores
non-hexadecimal BPM in the channel `03`.

Angolmois has a limited support for negative BPM. A negative BPM is assumed
specially to indicate the end of the song, so that no more grading is
performed and you can only watch the chart scrolling backwards until it hits
the bottom (measure -1). If not all objects have been graded (like many
variations of "Sofa $15 -> $1"), it is not considered "cleared" and the result
message is ignored. The initial BPM cannot be negative, and `#BPM` with
a negative value is ignored.

Angolmois does not support zero BPM both for the initial BPM and variable BPM.
Angolmois ignores them.

* `#STOPxx <integer>`
* `#STP<integer>.<integer> <integer>`
* Channel `09`

Again, stopping scroll for certain amount of time (hereafter "STOP") is
supported with a similar caveat as variable BPM. You can specify a STOP time
as a multiple of 192nd of measure (`#STOPxx` and channel `09`) or milliseconds
(`#STP`). Angolmois will choose one of them arbitrarily if they are in
the same position.[^3] Negative STOP time is ignored just like zero BPM.

[^3]: The current implementation does have a preference for milliseconds, but
      this behavior may change without a notice.

### Data Commands

In general, data commands do not allow whitespace between alphanumeric keys,
but does strip preceding and trailing whitespace.

* Channel `02` (defaults to 1.0)

Technically speaking, the size of measure affects the interval of horizontal
measure bars and nothing else. Angolmois ignores a measure size less than
0.001, including negative ones.

* Channels `1x` and `2x`

Angolmois supports five basic key models, namely 5KEY, 7KEY, 9KEY, 10KEY and
14KEY. The first two are used for `#PLAYER 1` and 9KEY is used for `pms` key
preset and others are for `#PLAYER 2` and `#PLAYER 3`. The names reflect
the number of keys besides the scratch and foot pedal (which 5KEY and 7KEY
have one, 9KEY has none, and 10KEY and 14KEY have two).

Angolmois automatically distinguishes 5KEY and 10KEY from 7KEY and 14KEY, by
counting the number of objects in channels `18`, `19`, `28` and `29`.
(Therefore objects in `28` may trigger 7KEY even for `#PLAYER 1`.) Similarily,
it automatically distinguishes two variants of PMS channels from each other by
counting the number of objects in channels `16`, `17`, `18` and `19`. This
automatic format detection can be suppressed with `--preset` or `--key-spec`
option; the latter can entirely override Angolmois' key model too.

Angolmois supports channels `17` and `27` as foot pedals (sort of); if those
channels are not empty, the pedal lane is added to the right (for 10KEY and
14KEY, two pedal lanes are added in the middle of keys). BM98's FREE ZONE,
which also uses these channels, is explicitly unsupported.

* Long notes

The player should press the key when the start of the long note is within
the grading area, and should release the key when the end of the long note is
within the grading area. If the first grading is missed (MISS) the second
grading does not happen at all. If the first grading is done (BAD or better),
and the player releases the key before or after the second grading area, then
the second grading results in MISS. Consequently, once MISS is given no
further grading happens for that long note.

In Angolmois, a single long note has two key sounds assigned (one at its start
and one at its end). The first key sound is played during the first grading
(unless it results in MISS); the second key sound is not played, but it is
treated similar to invisible objects (channels `3x` and `4x`).

* Channels `3x` and `4x`

Invisible objects may appear at any positions as long as they does not
coincide with other visible objects or long note endpoints (in which case
the object is reused as a BGM). This means that invisible objects may appear
*inside* the long note, which may be played when the player missed the start
of a long note and jammed the keyboard at anger before the long note is
finished.

Angolmois always plays a key sound closest to the current position in terms of
measure. The grading and playing a key sound is done independently; it is very
possible that the key sound for the closest invisible object is played and
the closest visible object (without a key sound) is graded. This behavior can
be exploited to, for instance, play different sounds according to the grading.

* `#LNOBJ xx`

Forces the alphanumeric key `xx` in channels `1x` and `2x` to be used as
an end marker for long notes. The end marker also forces the last visible
object before it to become a start of a long note. Long notes defined in this
manner are resolved after parsing, so the *last* normal object may be defined
after an end marker. `xx` is ignored if it is the first visible object or
the last visible object before it have been used as an end marker. `xx` is
used as a key sound at the end of long notes defined in this manner.

`#LNOBJ` can be used with channels `5x` and `6x`. In this case, the resolution
for `#LNOBJ` and one for channels `5x` and `6x` is done independently (as if
each other does not exist) and combined later. Angolmois does its best effort
to fix problematic charts (e.g. overlapping or consecutive long notes);
offending objects that have been removed will be reused as BGMs. This process
is also used for avoiding delicate parsing problems in channels `5x` and `6x`
alone.

* `#LNTYPE 1` (default)
* Channels `5x` and `6x`

Forces every matching pair of two non-`00` alphanumeric keys in those channels
to be a start and end of new long note. (Both alphanumeric keys are assigned
as a key sound.)

The matching is resolved after parsing like `#LNOBJ`, but if the same measure
for those channels is defined multiple times then Angolmois matches pairs of
alphanumeric keys in the order of appearance in that measure. This may result
in problematic charts, which are later fixed as described above. BMS writers
should avoid writing the same measure multiple times for those channels.

* `#LNTYPE 2`
* Channels `5x` and `6x`

Forces every consecutive row of the non-`00` (not necessarily same)
alphanumeric keys to be a long note. Only a start of the long note is assigned
a key sound of the first non-`00` alphanumeric key.

Again the row is resolved after parsing, but as like `#LNTYPE 1`, defining
the same measure multiple times may result in problematic charts. (Unlike
`#LNTYPE 1`, such construction in `#LNTYPE 2` almost always result in a
problem.) BMS writers should avoid writing the same measure multiple times for
those channels.

* Channels `Dx` and `Ex`

Inserts a "bomb" which is detonated when the player is pressing the key as
the bomb passes through the bottom of the screen. The bomb is displayed as
a dark red object (regardless of the color of lanes and other objects).
The bomb has no grading area. The bomb cannot be placed inside the long note.

Detonating a bomb results in MISS grade. The amount of gauge decrease is
specified with the base-36 alphanumeric key, or `ZZ` for the instant
death[^4]. The base-36 key has a unit of 0.5%; `01` for 0.5% decrease, `02`
for 1% decrease, `2S` for 50% decrease, and `5K` for 100% decrease. Angolmois
ignores more than 100% decrease except for `ZZ`.

The bomb itself cannot be placed in the long note or at the same position as
other objects, but the invisible object can overlap with it. It is possible
to abuse this behavior to provide a custom bomb sound (somewhat).

[^4]: Angolmois does not have the instant death except for this kind of bombs.
      It is not same as pressing ESC, as it will always display "YOU FAILED!"
      message. No bomb sound is played in this case.

### Control Flow

These commands may occur at any position in the file.

* `#RANDOM <integer>`

Upon the `#RANDOM` command, Angolmois generates one random number between one
and given integer (inclusive) with an equal probability. If the integer was
zero or negative the random number is still "generated", but put in the
indeterminate state so that the number is not equal to any integer. Similarly
the initial random number (when not set by other `#RANDOM` or `#SETRANDOM`
commands) is in the indeterminate state.

* `#SETRANDOM <integer>`

Functionally similar to `#RANDOM`, but the random number is fixed to given
integer. The number is put in the indeterminate state if the integer is zero
or negative.

* `#ENDRANDOM`

`#ENDRANDOM`, which closes the innermost `#RANDOM` block, is recommended for
reducing memory consumption but not required. It does nothing if there is no
open `#RANDOM` block.

* `#IF <integer>`
* `#ELSEIF <integer>`
* `#ELSE`
* `#ENDIF` (actually, `#END`)

These commands affects a flag whether the following commands will be processed
or not. The integer in `#IF` and `#ELSEIF` should be positive, otherwise the
following commands are unconditionally ignored.

`#IF` blocks cannot be nested unless other `#RANDOM` or `#SETRANDOM`
blocks wrap them; the nested `#IF` overrides the preceding `#IF`. In the
absence of an open `#IF` block, `#ELSEIF` is same as `#IF` and `#ELSE` always
toggles a processing flag while implicitly opening `#IF` block. `#ENDIF`
closes an open `#IF` block; over the course it also implicitly closes
innermost `#RANDOM` blocks without no open `#IF` block.

Angolmois assumes every line starting with `#END` as the end of `#IF` block,
except for `#ENDRANDOM` and `#ENDSW` (unsupported but disambiguated). This is
intended to handle occasional mistakes like `#END IF`.


Development Policy
------------------

Angolmois has an unusual policy that limits a number of its lines of code in
order to be compact and avoid implementing unimportant features, that is
unrelated to actual game play or can be replaced by other libraries or tools.
The threshold is currently 2,000 lines. (It is rumored that Angolmois 3.0 will
increase the threshold to 3,000 lines. This observation is actually consistent
with Angolmois 1.0...)

