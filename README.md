# FlickerstripFirmware
The Firmware for the Flickerstrip WiFi LED Strip

## Basic Custom Firmware Instructions
* First, download the code with `git clone git@github.com:Flickerstrip/FlickerstripFirmware.git` (you can use alternative methods such as Download ZIP if that's better)
* Next, we need to download some dependencies:
  * Download [M25PX_Flash_Memory](https://github.com/Flickerstrip/M25PX_Flash_Memory) into the "lib" folder
* Next, find a lightwork you want to include and open it in the editor (eg: [Exchange](https://hohmbody.com/flickerstrip/lightwork/?id=330)
* In the top right corner, choose "Download pattern definition"
* Place the downloaded pattern inside the `src/patterns` folder
* Edit the `src/defaultPatterns.h` file with an import for your new pattern eg: `#include "patterns/Exchange.h`
* Edit the `src/PatternManager.cpp` file, look for the text `resetPatternsToDefault` and a line eg: `this->addPatternFromProgmem(&patternDef330); //Adds Exchange`
  * Note: You can find the exact line that you need to add inside the pattern definition that you downloaded (it's at the top as a comment, you'll need to remove the `//` from the beginning)
* Compile using platformIO. Depending on how you install this, it may be a little different.. but if you're using the command line `pio run` will do what you want.
* Find the compiled firmware, it should be inside a hidden folder inside the project: `.pio/build/esp12e/firmware.bin`
  * You can upload this using the emergency firmware mode described in the [Flickerstrip Manual](https://hohmbody.com/flickerstrip-manual/)
