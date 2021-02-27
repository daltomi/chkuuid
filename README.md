### chkuuid ###

Compare the UUID of each partition with those found in FSTAB.


<img src="https://github.com/daltomi/chkuuid/raw/master/screenshot/scr0.png"/>

#### Libraries:

- libblkid

- libmount

- libudev


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
