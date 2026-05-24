# Knoscillator for Citadel

Originally written for Befaco's Lich module, Knoscillator for Citadel combines the original oscillator with Citadel's
built in sequencer and stereo delay to offer a full synth voice.

Knoscillator is a stereo oscillator that oscillates over a parametric 3D curve called a knot. 
The generated 3D coordinates are rotated around the origin in 3D space before being projected to 2D.
The X-Y coordinates of the 2D points are used as the left and right audio outputs. 
The knot can be visualized by plotting the audio output on a scope in X-Y mode. 

The knot type can be selected by pressing the MODE button to cycle through the available knots.
The selected knot type will be indicated by the color of the upper LEDs. Available knot types are:

- [Trefoil](https://en.wikipedia.org/wiki/Trefoil_knot) (Teal)
- [Lissajous](https://en.wikipedia.org/wiki/Lissajous_knot) (Lime Green)
- [Torus](https://en.wikipedia.org/wiki/Torus_knot) (Orange)

The complexity of each knot is determined by two coefficients called P and Q, which are restricted to integer values.
These coefficients are shared by all knots, but it is possible to morph between adjacent knots to create shapes that 
are not knots at all, but retain a similar flavor.

## Controls

Knoscillator's interface is largely patterned after the [Alchemist firmware](https://bastl-instruments.com/eurorack/modules/citadel-alchemist), 
so use of that faceplate is recommended when running Knoscillator. Faceplate labels below refer to Alchemist labels.

### Sequencing
The CLK, PATTERN, TRIG, ENV, ENV MOD, and NOTE jacks behave the same as Alchemist. As such, SHIFT+LFO MOD will set the 
RHYTHM parameter of the PATTERN, SHIFT+LFO will set TEMPO, holding SHIFT while turning the ENV knob will set
the amount of envelope modulation from the ENV MOD CV, and holding SHIFT while tapping MODE will set the TEMPO based
on speed of tapping.

The PITCH and PITCH MOD knobs behave the same as Alchemist, as well as MODE+PITCH MOD, MODE+TIMBRE MOD, and MODE+LFO MOD 
for setting the root note and scale used by the quantizer. However, the >PLAY jack is NOT quantized and there is currently 
no setting for controlling portamento.

### Knot Settings

As mentioned above, the MODE button can be used to cycle through the different knots types. MODE+ENV can be used set the 
MORPH amount between the selected knot and the next knot in the cycle. For example, with the Trefoil knot selected, 
MODE+ENV will morph from Trefoil when fully counter-clockwise, to Lissajous when fully clockwise. The MORPH setting is 
retained when cycling between knot types.

The MODE CV input will cycle between knot types. When using this input the MORPH setting controls both the morph amount 
_and_ the attenuation of the MODE CV (this is likely to change in a future release).

The P & Q coefficients for the knot can be set using MODE+PITCH for P and MODE+TIMBRE for Q. P & Q both range from 1 to 8 
and produce the most knot-like shapes when they are [coprime](https://en.wikipedia.org/wiki/Coprime_integers). The 
effect of these coefficients is most clearly seen by viewing the output in X-Y mode on an oscilloscope.

### Motion

Knoscillator replaces Citadel's LFO with its own LFO that controls the knot's speed of rotation. Rotation rates around
the X, Y, and Z axes are related, but not identical, and have been chosen to keep the stereo field moving in a way that
is not perceived as repetitive. 

When the LFO knob is at NOON, the knot will stop rotating. Turning CLOCKWISE from NOON 
increases rotation speed in the "forward" direction. Turning COUNTER-CLOCKWISE from NOON reverses the direction of the 
LFO and thus the knot rotation. The LFO MOD knob acts as an attenuverter on the signal present at the LFO MOD
jack, as with Alchemist. 

Sending a gate to the LFO RESET CV input resets the rotation of the knot to zero on all axes 
(i.e. resets the rotation matrix to an identity matrix).

The LFO TRIANGLE CV output is a sine wave that represents the rotation of the knot around the Y-axis. 
The LFO PULSE CV output will be high when the TRI CV output is above 2.5 volts (i.e. the first half of the sine wave).

A fun way to modulate rotation is by patching the ENV OUT jack to LFO MOD, setting the LFO MOD knob to around 2 o'clock, 
and setting the LFO knob to around 11 o'clock. As the envelope rises the rotation will be slowed to a stop and then 
reversed, with the same happening as the envelope falls.

### Timbre

Knoscillator implements TIMBRE using FM synthesis (i.e. phase modulation). However, since this happens in 3D space
(remember that we are oscillating over a 3D parametric equation), TIMBRE changes both the pitch content _and_ the 
stereo field. On a scope it looks a bit like the knot is being traced over multiple times with each trace slightly rotated.

The TIMBRE knob sets the depth of phase modulation and the TIMBRE MOD knob acts as an attenuverter on the signal in 
the TIMBRE CV input, which is then added to the current TIMBRE settings. 

SHIFT+TIMBRE sets the FM RATIO from 2:1 when 
fully counter-clockwise, to 4:1 at noon, to 8:1 at fully clockwise. The FM ratio changes smoothly, which is useful when 
using Knoscillator as an FM drum voice.

### FX

The built-in FX for Knoscillator uses the Citadel's stereo delay. The delay time is always tempo relative and is 
determined by the ratio of the P & Q coefficients (e.g. P = 2 and Q = 1 will delay time equal to 2 clock cycles).

SHIFT+TIMBRE MOD is a macro control that sets how much of the wet delay signal is mixed with the dry signal, the cutoff of 
the delay's filter, and the amount of feedback. 

At NOON, the wet signal is silent. Turning the knob CLOCKWISE increases 
the wet signal, applies a high pass filter, and increases the amount of feedback. Turning the knob COUNTER-CLOCKWISE 
increases the wet signal, applies a low pass filter, and increases the amount of feedback, but inverts the feedback signal.