SECTIONS
{
  .lvgl_buf (NOLOAD) :
  {
    KEEP(*(SORT(.lvgl_buf*)))
  } > ext_ram_seg
}
