# Cave Explorer

Cave Explorer is Brian Xiao's implementation of Adriel Luo's [*Design Document*](http://graphics.cs.cmu.edu/courses/15-466-f17/game1-designs/aluo) for game1 in 15-466-f17.

![](https://github.com/0aix/15-466-f17-base1/blob/master/screenshots/cave-explorer.png?raw=true)

## Asset Pipeline

The only asset is used is an atlas/spritesheet courtesy of [Kenney.nl](http://kenney.nl/assets/roguelike-rpg-pack). 

Because I only use 10 sprites from the spritesheet, and each sprite is 16x16 and lined up, I hardcode the uv-coordinates for each sprite. The SpriteInfo objects are loaded before the game loop is entered. 

## Architecture

I implemented the map as a 2D-array of tile types. Unfortunately, these are hard-coded as I felt randomly generating a map would be too difficult. I also have a corresponding 'lit' 2D-array that determines whether or not the tile has been stepped on and therefore lit. Otherwise, these tiles are rendered as a "dirt wall" tile. 

There are a number of states that the minecart (player) can take. These are horizontal stationary; moving left; moving right; vertical stationary; moving up; moving down; and all of those but with gold. 

Input takes in only left, right, up, down, and space and only does anything in stationary states. 
Space will check for gold surrounding the player and attempt to 'mine' the gold. If the gold is *actually* gold, then the player wins; otherwise, the gold bars turn into logs. (This is done by probability: 1/#-of-gold-left)

The game state is checked each iteration to see if the player should still be moving on a track or if the player should stop. When the player is stopped, it will check if gold is in the surrounding blocks, and if so, reveal it. 

## Reflection

If I were doing it again, I would make the game more difficult and add randomization as well as different mechanics. 

The design document was clear on mostly everything except the map/grid layout. I had to make some amenities and stretch things out (making walls actual tiles so the map was 2x as big as in the design document).


# About Base1

This game is based on Base1, starter code for game1 in the 15-466-f17 course. It was developed by Jim McCann, and is released into the public domain.

## Requirements

 - modern C++ compiler
 - glm
 - libSDL2
 - libpng

On Linux or OSX these requirements should be available from your package manager without too much hassle.

## Building

This code has been set up to be built with [FT jam](https://www.freetype.org/jam/).

### Getting Jam

For more information on Jam, see the [Jam Documentation](https://www.perforce.com/documentation/jam-documentation) page at Perforce, which includes both reference documentation and a getting started guide.

On unixish OSs, Jam is available from your package manager:
```
	brew install ftjam #on OSX
	apt get ftjam #on Debian-ish Linux
```

On Windows, you can get a binary [from sourceforge](https://sourceforge.net/projects/freetype/files/ftjam/2.5.2/ftjam-2.5.2-win32.zip/download),
and put it somewhere in your `%PATH%`.
(Possibly: also set the `JAM_TOOLSET` variable to `VISUALC`.)

### Bulding
Open a terminal (on windows, a Visual Studio Command Prompt), change to this directory, and type:
```
	jam
```

### Building (local libs)

Depending on your OSX, clone 
[kit-libs-linux](https://github.com/ixchow/kit-libs-linux),
[kit-libs-osx](https://github.com/ixchow/kit-libs-osx),
or [kit-libs-win](https://github.com/ixchow/kit-libs-win)
as a subdirectory of the current directory.

The Jamfile sets up library and header search paths such that local libraries will be preferred over system libraries.
