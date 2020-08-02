### chkuuid ###

Compare the UUID of each partition with those found in FSTAB.


<img src="https://git.disroot.org/daltomi/chkuuid/raw/branch/master/screenshot/scr0.png"/>

#### Libraries:

- libblkid

- libmount

- libudev (*)

(*) Note: This project does not use sytemd libraries, use the `libeudev` package from your distribution.

#### Tool

- pkg-config


#### Compile

```
make

```

- To compile in debug mode:

```
make clean # optional
make debug
```

- Compile without color

```
CFLAGS=-DNOCOLOR make

```
