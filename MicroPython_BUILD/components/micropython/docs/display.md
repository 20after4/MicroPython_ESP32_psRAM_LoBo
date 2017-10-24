# **display** Module

## Class **TFT**

---

This class includes full support for **ILI9341**, **ILI9488**, **ST7789V** and **ST7735** based TFT modules in 4-wire SPI mode.

Some [TFT examples](https://github.com/loboris/MicroPython_ESP32_psRAM_LoBo/tree/master/MicroPython_BUILD/components/micropython/esp32/modules_examples/tft) are available.

---

### Connecting the display

| ESP32 pin | Display module | Notes |
| - | - | - |
| Any output pin | MOSI | SPI input on Display module |
| Any pin | MISO | SPI output from Display module, optional |
| Any output pin | SCK | SPI clock input on Display module |
| Any output pin | CS  | SPI CS input on Display module |
| Any output pin | DC  | DC (data/command) input on Display module |
| Any output pin | TCS  | Touch pannel CS input (if touch panel is used |
| Any output pin | RST  | **optional**, reset input of the display module, if not used **pullup the reset input** to Vcc |
| Any output pin | BL  | **optional**, backlight input of the display module, if not used connect to +3.3V (or +5V) |
| GND | GND  | Power supply ground |
| 3.3V or +5V | Vcc  | Power supply positive |

**Make shure the display module has 3.3V compatible interface, if not you must use level shifter!**

---


Before using the display, the **display** module must be imported and the instance of the TFT class has to be created:

```
import display
tft = display.TFT()
```

#### Colors

**Color** value are given as 24 bit integer numbers, 8-bit per color.

For example: **0xFF0000** represents the RED color. Only upper 6 bits of the color component value is used.

The following color constants are defined and can be used as color arguments: 

**BLACK, NAVY, DARKGREEN, DARKCYAN, MAROON, PURPLE, OLIVE, LIGHTGREY, DARKGREY, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE, ORANGE, GREENYELLOW, PINK**

#### Drawing

All **drawings** coordinates are **relative** to the **display window**.

Initialy, the display window is set to full screen, and there are methods to set the window to the part of the full screen.

#### Fonts

9 bit-mapped fornts and one vector 7-segment font are included.
Unlimited number of fonts from file can also be used.

The following font constants are defined and can be used as font arguments: 

**FONT_Default, FONT_DefaultSmall, FONT_DejaVu18, FONT_Dejavu24, FONT_Ubuntu, FONT_Comic, FONT_Minya, FONT_Tooney, FONT_Small, FONT_7seg**

#### Touch panel

Touch panels based on **XPT2066** and **STMPE610** controllers are supported at the moment.

---

## Methods

---

#### tft.init( type, mosi=pinnum, miso=pinnum, clk=pinnum, cs=pinnum [, tcs,rst_pin,backl_pin,backl_on, hastouch, invrot,bgr] )

Initialize the SPI interface and set the warious operational modes.

All arguments, except for **type** are **KW** arguments.

*Pins have to be given as* **pin numbers**, *not the machine.Pin objects*

| Argument | Description |
| - | - |
| type | required, sets the display controler type, use one of the constants: **ST7789**, **ILI9341**, **ILI9488**, **ST7735**, **ST7735B**, **ST7735R** |
| spihost | optional, default=HSPI_HOST, select ESP32 spi host, use constants: **HSPI_HOST** or **VSPI_HOST** |
| width | optional, default=240, display phisical width in pixels (display's **smaller** dimension). |
| height | optional, default=320, display phisical height in pixels (display's **larger** dimension). |
| speed | optional, default=26000000, SPI speed for display comunication in Hz. Maximal usable speed depends on display type and the wiring length |
| mosi | required, SPI MOSI pin number |
| miso | required, SPI MISO pin number |
| clk | required, SPI CLK pin number |
| cs | required, SPI CS pin number |
| tcs | optional, Touch panel CS pin number if used |
| rst_pin | optional, default=nut used, pin to drive the RESET input on Display module, if not set the **software** reset will be used |
| backl_pin | optional, default=nut used, pin to drive the backlight input on Display module, **do not use if the display module does not have some kind of backlight driver**, the display's backlight usualy needs more current than gpio can provide. |
| backl_on | optional, default=0, polarity of *backl_pin* for backlight ON, 0 or 1 |
| hastouch | optional, default=**TOUCH_NONE**, set to **TOUCH_XPT** or **TOUCH_STMPE** if touch panel is used |
| invrot | optional, default=auto, configure special display rotation options, if not set default value for display type is used. If you get some kind of mirrored display, try to use values 0, 1, 2 or 3 |
| bgr | optional, default=False, set to True if the display panel has BGR matrix. If you get inverted *RED* and *BLUE* colors, try to change this argument |


### tft.pixel(x, y [,color])

Draw the pixel at position (x,y). If *color* is not given, current foreground color is used.

### tft.readPixel(x, y)

Get the pixel color value at position (x,y).

### tft.line(x, y, x1, y1 [,color])

Draw the line from point (x,y) to point (x1,y1). If *color* is not given, current foreground color is used.

### tft.lineByAngle(x, y, start, length, angle [,color])

Draw the line from point (x,y) with length *lenght* starting st distance *start* from center. If *color* is not given, current foreground color is used.

The angle is given in degrees (0~359).

### tft.triangle(x, y, x1, y1, x2, y2 [,color, fillcolor])

Draw the triangel between points (x,y), (x1,y1) and (x2,y2). If *color* is not given, current foreground color is used.

If *fillcolor* is given, filled triangle will be drawn.

### tft.circle(x, y, r [,color, fillcolor])

Draw the circle with center at (x,y) and radius r. If *color* is not given, current foreground color is used.

If *fillcolor* is given, filled circle will be drawn.


### tft.ellipse(x, y, rx, ry [opt, color, fillcolor])

Draw the circle with center at (x,y) and radius r. If *color* is not given, current foreground color is used.

**opt* argument defines the ellipse segment to be drawn, default id 15, all ellipse segments.

Multiple segments can drawn, combine (logical or) the values.
* 1 - upper left segment
* 2 - upper right segment
* 4 - lower left segment
* 8 - lower right segment

If *fillcolor* is given, filled elipse will be drawn.


### tft.arc(x, y, r, thick, start, end [color, fillcolor])

Draw the arc with center at (x,y) and radius *r*, starting at angle *start* and ending at angle *end*

The thicknes of the arc outline is set by the *thick* argument

If *fillcolor* is given, filled arc will be drawn.


### tft.poly(x, y, r, sides, thick, [color, fillcolor, rotate])

Draw the polygon with center at (x,y) and radius *r*, with number of sides *sides*

The thicknes of the polygon outline is set by the *thick* argument

If *fillcolor* is given, filled polygon will be drawn.

If *rotate* is given, the polygon is rotated by the given angle (0~359)


### tft.rect(x, y, width, height, [color, fillcolor])

Draw the rectangle from the upper left point at (x,y) and width *width* and height *height*

If *fillcolor* is given, filled rectangle will be drawn.


### tft.roundrect(x, y, width, height, r [color, fillcolor])

Draw the rectangle with rounded corners from the upper left point at **(x,y)** and width **width** and height **height**

Corner radius is given by **r** argument.

If **fillcolor** is given, filled rectangle will be drawn.


### tft.clear([color])

Clear the screen with default background color or specific color if given.

### tft.clearWin([color])

Clear the current display window with default background color or specific color if given.


### tft.orient(orient)

Set the display orientation. Use one of predifined constants: **tft.PORTRAIT**, **tft.LANDSCAPE**, **tft.PORTRAIT_FLIP**, **tft.LANDSCAPE_FLIP**


### tft.font(font [,rotate, transparent, fixedwidth, dist, width, outline, color])

Set the active font and its characteristics.

| Argument | Description |
| - | - |
| font | required, use font name constant or font file name |
| rotate | optional, set font rotation angle (0~360) |
| transparent | only draw font's foreground pixels |
| fixedwidth | draw proportional font with fixed character width, max character width from the font is used |
| dist | only for 7-seg font, the distance between bars |
| width | only for 7-seg font, the width of the bar |
| outline | only for 7-seg font, draw the outline |
| color | font color, if not given the current foreground color is used |


### tft.attrib7seg(dist, width, outline, color)

Set characteristics of the 7-segment font

| Argument | Description |
| - | - |
| dist | the distance between bars |
| width | the width of the bar |
| outline | outline color |
| color | fill color |


### tft.fontSize()

Return width and height of the active font


### tft.text(x, y, text [, color])

Display the string *text* at possition (x,y).
If *color* is not given, current foreground color is used.

* **x**: horizontal position of the upper left point in pixels, special values can be given:
  * CENTER, centers the text
  * RIGHT, right justifies the text
  * LASTX, continues from last X position; offset can be used: LASTX+n
* **y**: vertical position of the upper left point in pixels, special values can be given:
  * CENTER, centers the text
  * BOTTOM, bottom justifies the text
  * LASTY, continues from last Y position; offset can be used: LASTY+n
* **text**: string to be displayed. Two special characters are allowed in strings:
  * ‘\r’ CR (0x0D), clears the display to EOL
  * ‘\n’ LF (ox0A), continues to the new line, x=0

 
### tft.textWidth(text)

Return the width of the string *text* using the active font fontSize


### tft.textClear(x, y, text [, color])

Clear the the screen area used by string *text* at possition (x,y) using the bacckground color *color*.
If *color* is not given, current background color is used.


### tft.image(x, y, file [,scale, type])

Display the image from the file *file* on position (x,y)
* **JPG** and **BMP** can be displayed.
* Constants **tft.CENTER**, **tft.BOTTOM**, **tft.RIGHT** can be used for x&y
* **x** and **y** values can be negative

**scale** (jpg): image scale factor: 0 to 3; if scale>0, image is scaled by factor 1/(2^scale) (1/2, 1/4 or 1/8)

**scale** (bmp): image scale factor: 0 to 7; if scale>0, image is scaled by factor 1/(scale+1)

**type**: optional, set the image type, constants *tft.JPG* or *tft.BMP* can be used. If not set, file extension and/or file content will be used to determine the image type.


### tft.setwin(x, y, x1, y1)

Set active display window to screen rectangle (x,y) - (x1,y1)


### tft.resetwin()

Reset active display window to full screen size.


### tft.savewin()

Save active display window dimensions.


### tft.restorewin()

Restore active display window dimensions previously saved wint savewin().


### tft.screensize()

Return the display size, (width, height)


### tft.winsize()

Return the active display window size, (width, height)


### tft.hsb2rgb(hue, saturation, brightness)

Converts the components of a color, as specified by the HSB model, to an equivalent set of values for the default RGB model.
Returns 24-bit integer value suitable to be used as color argiment
Arguments
* **hue**: float: any number, the floor of this number is subtracted from it to create a fraction between 0 and 1. This fractional number is then multiplied by 360 to produce the hue angle in the HSB color model.
* **saturation**: float; 0 ~ 1.0
* **brightness**: float; 0 ~ 1.0


### tft.compileFont(file_name [,debug])

Compile the source font file (must have **.c** extension) to the binary font file (same name, **.fon** extension) which can be used as external font.
If *debug=True* the information about compiled font will be printed,

You can create the **c** source file from any **tft** font using the included [ttf2c_vc2003.exe](https://github.com/loboris/MicroPython_ESP32_psRAM_LoBo/tree/master/MicroPython_BUILD/components/micropython/esp32/modules_examples/tft/font_tool/) program.
See [README](https://github.com/loboris/MicroPython_ESP32_psRAM_LoBo/tree/master/MicroPython_BUILD/components/micropython/esp32/modules_examples/tft/font_tool/README.md) for instructions.


### tft.gettouch([raw])

Get the touch status and coordinates.
The tuple (touched, x, y) wil be returned.

**thouch** is *True* if the touch panel is touched, *False* if not.
**x**, **y** - touch point coordinates, valid only if *touched=True*

If the optional argument *raw* is True, the raw touch controller coordinates are returned. Otherwise, the calibrated screen coordinates are returned.


#### Touch calibration can be done using the frozen module **tpcalib**.

You must initialise the displayfirst.

Usage:

tpc(tft.TOUCH_STMPE|tft.TOUCH_XPT [, save])


'''
import tpcalib
tpc = tpcalib.Calibrate(tft)
dosave = True
tp_type = tft.TOUCH_STMPE
tpc.tpcalib(tt_type, dosave)

'''

If optional argument id **True**, calibration constants will be updated and saved in NVS memory as **tpcalibX** & **tpcalibY**.

After reboot you can get them from NVS:

'''
import machine

tft.setCalib(machine.nvs_getint("tpcalibX"), machine.nvs_getint("tpcalibY"))

'''


---


### Tested on

ESP32-WROVER-KIT v3, ST7789V controller, 240x320
![Tested on](https://raw.githubusercontent.com/loboris/MicroPython_ESP32_psRAM_LoBo/master/Documents/disp_wrower-kit.jpg)

2.4" 240x320 ILI9341 conroller with Touch panel from eBay
![Tested on](https://raw.githubusercontent.com/loboris/MicroPython_ESP32_psRAM_LoBo/master/Documents/disp_ili9341.jpg)

3.5" 320x480 ILI9844 controller with Touch panel from BuyDisplay
![Tested on](https://raw.githubusercontent.com/loboris/MicroPython_ESP32_psRAM_LoBo/master/Documents/disp_9488.jpg)

1.8" 128x160 ST7735 conroller from eBay
![Tested on](https://raw.githubusercontent.com/loboris/MicroPython_ESP32_psRAM_LoBo/master/Documents/disp_7735.jpg)

