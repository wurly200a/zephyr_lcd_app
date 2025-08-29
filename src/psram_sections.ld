/* LVGLのVDB用セクションをPSRAM(ext_ram_seg)へ */
SECTION_DATA_PROLOGUE(.lvgl_buf,(NOLOAD),SUBALIGN(4))
{
    . = ALIGN(4);
    KEEP(*(.lvgl_buf*))
    . = ALIGN(4);
} > ext_ram_seg
