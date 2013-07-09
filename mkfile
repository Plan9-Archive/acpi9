SRC=ac.c acpi.c cpu.c bat.c ec.c io.c
</$objtype/mkfile

acpi:   ac.$O acpi.$O cpu.$O bat.$O io.$O tp.$O ec.$O
        $LD $LDFLAGS -o $target $prereq

clean:
		rm -f *.$O acpi

%.$O:   %.c
        $CC $CFLAGS $stem.c
