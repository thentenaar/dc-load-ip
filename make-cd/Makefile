CDRECORD	= cdrecord dev=1000,0,0 speed=8
SCRAMBLE	= ./scramble
DD		= dd
CP		= cp
MKISOFS		= mkisofs
CAT		= cat
TOUCH		= touch
1ST_READ	= ../target-src/1st_read/1st_read.bin

all: $(1ST_READ) audio.raw burn-audio data.raw burn-data

audio.raw:
	$(DD) if=/dev/zero of=audio.raw bs=2352 count=300

burn-audio: audio.raw
	$(CDRECORD) -multi -audio audio.raw
	touch burn-audio

1ST_READ.BIN: $(1ST_READ)
	$(SCRAMBLE) $(1ST_READ) 1ST_READ.BIN

data.raw: tmp.iso IP.BIN
	( cat IP.BIN ; dd if=tmp.iso bs=2048 skip=16 ) > data.raw

tmp.iso: 1ST_READ.BIN burn-audio
	$(MKISOFS) -C `$(CDRECORD) -msinfo` -o tmp.iso 1ST_READ.BIN

burn-data: data.raw
	$(CDRECORD) -xa data.raw
	rm -f audio.raw data.raw burn-audio

.PHONY : clean
clean:
	rm -f tmp.iso burn-audio 1ST_READ.BIN

.PHONY : distclean
distclean: clean
	rm -f audio.raw data.raw 1ST_READ.BIN
