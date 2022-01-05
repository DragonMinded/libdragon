########################################################
# Include this file wherever you prefer in your text segment
########################################################

########################################################
# RSP Assert support
#
# Define a function called "assertion_failed" that is
# used as jump target by assert macros. 
#
########################################################

	.func assertion_failed
assertion_failed:
	# Infinite loop. Use a special break opcode to
	# be able to tell it from standard "break".
	b assertion_failed
	break 0xBA
	.endfunc
