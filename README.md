# OB1 Firmware

The OB1 is the control card for the SC1a and DCR1a units, the first siacoin and
decred miners shipped by Obelsik. This repo contains the files and instructions
needed to build and load firmware onto the ob1 control cards.

The firmware is broken into two pieces. The first piece is the image that goes
on the ob1 SOM itself. The second piece is an image that goes onto an SD card to
provision the control card. At the factory, the SD card is inserted into the
control card, and then the control card is rebooted. The control card defaults
to booting from the SD card, which will run a program that wipes the control
card SOM and adds the firmware image.

The build process then is to create firmware for the control card first, then
create the image for the SD card which provisions the control card.

## Setup

You do not need to be root or to have admin priviledges to create the firmware
images. You can switch to a clean installation at any time by running `make
clean`. You can get the dependencies (buildroot) by running `make dependencies`.
Finally, you can build the software fully with `make`.

We attempt to use reproducable builds. Buildroot reproducible buidls are
currently considered experimental.
