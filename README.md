# Rooftop ice buildup detection using Edge Impulse with synthetic data created with NVIDIA Omniverse Replicator

## Intro
This portable device monitors buildings and warns when potential hazardous icicles are formed. In ideal conditions icicles can form at a rate of [more than 1 cm (0.39 in) per minute](https://en.wikipedia.org/wiki/Icicle). As numerous people are injured and killed by these solid projectiles each year, responsible building owners often close sidewalks in the spring to minimize risk. This project demonstrates how an extra set of digital eyes can notify property owners icicles are forming and need to be removed before they can cause harm.

## Hardware used:
* [Arduino Portena H7](https://docs.arduino.cc/hardware/portenta-h7/)
* [Arduino Portena Vision Shield w/LoRa Connectivity](https://docs.arduino.cc/hardware/portenta-vision-shield/)
* NVIDIA GeForce RTX
* Formlabs Form 2 3D printer

## Software used:
* [Edge Impulse Studio](https://studio.edgeimpulse.com/studio)
* [NVIDIA Omniverse Code](https://www.nvidia.com/en-us/omniverse/) with [Replicator](https://developer.nvidia.com/omniverse/replicator)
* [Visual Studio Code](https://code.visualstudio.com/)
* [Blender](https://www.blender.org/)
* [Autodesk Fusion 360](https://www.autodesk.no/products/fusion-360/)

## Code and machine learning repository
Project [Impulse](https://studio.edgeimpulse.com/public/332581/latest) and [code repository](https://github.com/eivholt/icicle-monitor).

## Working principle
Forming icicles are detected using a neural network with an architecture aimed at detecting objects in images from the on-board camera. The NN is trained and tested exclusively on synthesized images. The images are generated with realistic simulated lighting conditions. A small amount of real images are used to verify the model.

## Mobility
A powerful platform combined with a high resolution camera with fish-eye lense would increase the ability to detect icicles. However, by implementing the object detection model on a small, power-efficient, but highly constrained device, options for deployment increase. Properly protected against moisture this device can be mounted outdoors on poles facing roofs in question. LoRaWAN communication enables low battery consumption and long transmission range.

## Limitations
### Weatherproofing
The device enclosure is not properly sealed for permanent outdoor installation. The camera is mounted on the shield PCB and will need some engineering to both be able to see through the enclosure and be water tight. For inspiration on how to create weather-proof enclosures that allow sensors and antennas outside access, [see this project](https://www.hackster.io/eivholt/low-power-snow-depth-sensor-using-lora-e5-b8e7b8) on friction fitting and use of rubber washers. The project in question also proves that battery operated sensors can work with no noticible degradation in winter conditions (to at least -15 degrees Celcius).

### Obscured view
The project has no safe-guard against false negatives. The device will not report if it's view is blocked. This could be resolved by placing static markers on both sides of an area to monitor and included in synthetic training data. Absence of at least one marker could trigger a notification that the view is obscured.

### Object scale
Due to optimization techniques in Faster Objects - More Objects (FoMo) determining relative sizes of the icicles is not feasible. As even icicles with small mass can be harmful at moderate elevation this is not a crucial feature.

### Exact number of icicles
The object detection model has not been trained to give an exact number of icicles in view. This has no practical implication other than the model verification results appearing worse than practical performance.

## Object detection using neural networks
[FOMO (Faster Objects, More Objects)](https://docs.edgeimpulse.com/docs/edge-impulse-studio/learning-blocks/object-detection/fomo-object-detection-for-constrained-devices) is a novel machine learning algorithm that allows for visual object detection on highly constrained devices through training of a neural network with a number of convolutional layers.
![](img/62a1c8f1370c3e1602466641_Group 316.svg "Optical illusion")

TinyML project to detect dangerous ice build-up on buildings.

https://www.youtube.com/watch?v=9H1gRQ6S7gg&t=23s
https://www.insidescience.org/news/riddles-rippled-icicle
https://link.brightcove.com/services/player/bcpid106573614001?bckey=AQ~~,AAAAGKlf6FE~,iSMGT5PckNvcgUb_ru5CAy2Tyv4G5OW3&bctid=2732728840001

3D:
Snow, particle effect
Building, random
Skybox
    https://youtu.be/MRD-oAxaV8w
    https://www.nvidia.com/en-us/on-demand/session/omniverse2020-om1417/

Icicle

## Sun studies
Window->Sun Study

## Semantics Schema Editor
Replicator->Semantics Schema Editor

Snow on roof
Passer-byes

Blender export:
Selection only
Convert Orientation:
Forward Axis: X
Up Axis: Y

Select vertex on model (Edit Mode), Shift+S-> Cursor to selected
(Object Mode) Select Hierarchy, Object>Set Origin\Origin to 3D Cursor
(Object Mode) Shift+S\Cursor to World Origin

