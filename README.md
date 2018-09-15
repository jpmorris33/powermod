# powermod
ALSA MIDI client to use a Griffin PowerMate as a modulation wheel


This was written for my own use, but brief instructions are included
below:


First, make sure the powermate is appearing as /dev/input/powermate
If not, you can use the -d command to change the device path to match,
e.g.
    powermod -d /dev/input/event18

Obviously this will only work if you have read permission to the device.

Once powermod is running, it should appear as an ALSA device,
e.g.
   aconnect -i
...should show something like:
client 128: 'Griffin PowerMate modwheel' [type=user,pid=7920]
    0 'PowerMate       '

You can then connect the device via aconnect, e.g. if you have a synthesizer
on device 36:

client 36: 'TRITON Extreme' [type=kernel,card=5]
    0 'TRITON Extreme MIDI 1'

Running 'aconnect 128 36'
...should wire powermod to that synthesizer.

