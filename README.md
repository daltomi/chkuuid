### chkuuid ###

Compare the UUID of each partition with those found in FSTAB.


<img src="https://github.com/daltomi/chkuuid/raw/master/screenshot/scr0.png"/>

### Platform
* GNU/Linux


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
