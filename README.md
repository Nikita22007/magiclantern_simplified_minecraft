This is a modified copy of magiclantern_simplified to run a minecraft server using avrcraft on the 200D **only**.
THIS IS A DRAFT(BODGE), I DONT RECCOMEND RUNNING THIS WITHOUT KNOWING WHAT YOU'RE DOING AS THIS COULD DESTORY YOUR CAMERA. I AM NOT RESPONSILBE IF YOU ATTEMPT THIS AND YOUR CAMERA BREAKS, TRY THIS AT YOUR OWN RISK.
If you still want to run this, make sure you have a linux machine. (I used ubuntu) and have arm-none-eabi-gcc compiler installed along with make and then run the following commmands
```
git clone https://github.com/turtiustrek/magiclantern_simplified/
cd magiclantern_simplified/platform/200D.101
make
```

Attributions:
https://github.com/cnlohr/avrcraft - for avrcraft

Magic Lantern
=============

Magic Lantern (ML) is a software enhancement that offers increased
functionality to the excellent Canon DSLR cameras.
  
It's an open framework, licensed under GPL, for developing extensions to the
official firmware.

Magic Lantern is not a *hack*, or a modified firmware, **it is an
independent program that runs alongside Canon's own software**. 
Each time you start your camera, Magic Lantern is loaded from your memory
card. Our only modification was to enable the ability to run software
from the memory card.

ML is being developed by photo and video enthusiasts, adding
functionality such as: HDR images and video, timelapse, motion
detection, focus assist tools, manual audio controls much more.

For more details on Magic Lantern please see [http://www.magiclantern.fm/](http://www.magiclantern.fm/)
