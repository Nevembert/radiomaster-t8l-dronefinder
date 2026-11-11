/* OFFLINE candidate placement; not proof of bootloader/update compatibility. */
MEMORY {
    PATCH (rx) : ORIGIN = 0x0801d150, LENGTH = 0x1eb0
    STATE (rw) : ORIGIN = 0x20002fb8, LENGTH = 0x80
}
SECTIONS {
    .text : {
        KEEP(*(.text.hook_*))
        *(.text*) *(.rodata*)
        . = ALIGN(4);
    } > PATCH
    .data : { *(.data*) } > STATE
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > STATE
    /DISCARD/ : { *(.comment*) *(.note*) *(.ARM.exidx*) *(.ARM.extab*) }
    ASSERT(SIZEOF(.data)==0, "no data loader provided")
    ASSERT(SIZEOF(.bss)<=0x80, "state exceeds reserved heap tail")
    ASSERT(ADDR(.text)+SIZEOF(.text)<=0x0801f000, "settings flash overlap")
}
