OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SECTIONS
{
  . = 0x0;
  _text = .;			/* Text and read-only data */
  .text : {
	*(.text)
	*(.text.*)
	*(.gnu.warning)
	} = 0x9090

  _etext = .;			/* End of text section */

  .rodata : {
          *(.rodata)
	  *(.rodata.*)

          /* Constructors/destructors */
          . = ALIGN(64 / 8);
          __init_array_start = . ;
          *(SORT_BY_INIT_PRIORITY(.init_array.*))
          *(SORT_BY_INIT_PRIORITY(.ctors*))
          *(.init_array)
          __init_array_end = . ;
          __fini_array_start = . ;
          *(SORT_BY_INIT_PRIORITY(.fini_array.*))
          *(SORT_BY_INIT_PRIORITY(.dtors*))
          *(.fini_array)
          __fini_array_end = . ;
	  }

  . = ALIGN(4096);
  _erodata = .;

  .data : {			/* Data */
	*(.data)
	}

  _edata = .;			/* End of data section */

  .tdata : {
	_tdata_start = . ;
	*(.tdata)
	_tdata_end = . ;
	}

  .tbss : {
	_tbss_start = . ;
	*(.tbss)
	_tbss_end = . ;
	}

  __bss_start = .;		/* BSS */
  .bss : {
	*(.bss)
	*(.bss.*)
	}
  _end = . ;

  /* Sections to be discarded */
  /DISCARD/ : {
	*(.data.exit)
	*(.exitcall.exit)
	}

  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment 0 : { *(.comment) }
}
