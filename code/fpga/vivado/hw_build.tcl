# hw_build.tcl - bitstream for the hardware test of one engine on the Arty S7-25.
#   vivado -mode batch -source hw_build.tcl -tclargs <tag> SET=5 ARCH=1 D=8
set tag  [lindex $argv 0]
set gens [lrange $argv 1 end]
set here [file dirname [file normalize [info script]]]
set rtl  [file normalize $here/../rtl]
set hw   [file normalize $here/../rtl_hw]
set out  [file normalize $here/hw/$tag]
file mkdir $out
set_param general.maxThreads 8
create_project -in_memory -part xc7s25csga324-1
read_vhdl -vhdl2008 [list $rtl/r4_pkg.vhd $rtl/polyval_mac.vhd $rtl/aead1_ctrl.vhd $rtl/ksg_aes.vhd \
    $rtl/ksg_snow.vhd $rtl/ksg_zuc.vhd $rtl/nca5_top.vhd $rtl/nca4_top.vhd $rtl/nca6_top.vhd \
    $hw/uart.vhd $hw/clkgen_mmcm.vhd $hw/hwtest_top.vhd]
read_xdc $hw/arty_s7_25.xdc
set g {}
foreach x $gens { lappend g -generic $x }
synth_design -top hwtest_top -part xc7s25csga324-1 {*}$g
opt_design
place_design
route_design
report_utilization -file $out/utilization.rpt
report_timing_summary -file $out/timing.rpt
set wns [get_property SLACK [get_timing_paths -delay_type max -max_paths 1]]
write_bitstream -force $out/$tag.bit
puts "R4 HWBUILD $tag: WNS $wns ns, bitstream $out/$tag.bit"
