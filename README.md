### chkuuid ###

Compare the UUID of each partition with those found in FSTAB.


<img src="https://github.com/daltomi/chkuuid/raw/master/screenshot/scr0.png"/>

### Platform
* GNU/Linux

### Package - ArchLinux - AUR
[chkuuid-git](https://aur.archlinux.org/packages/chkuuid-git/)

  * GPG key
	```bash
	gpg --keyserver gozer.rediris.es --recv B1B08540E74FE8A2
	```

### Dependencies
* Libraries : **libblkid** **libmount**  **libudev**
* Build:  **gcc**, **make**, **pkg-config**


### Build
```bash
make
-- or --
make debug
```

- Compile without color

```
CFLAGS=-DNOCOLOR make

```
