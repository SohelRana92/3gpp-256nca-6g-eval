# hw_program.tcl - load a bitstream into the Arty S7-25 over USB (JTAG).
#   vivado -mode batch -source hw_program.tcl -tclargs hw/nca5_A/nca5_A.bit
set bit [lindex $argv 0]
open_hw_manager
connect_hw_server
open_hw_target
set dev [lindex [get_hw_devices xc7s25*] 0]
current_hw_device $dev
set_property PROGRAM.FILE $bit $dev
program_hw_devices $dev
puts "R4 PROGRAMMED $bit"
close_hw_manager
